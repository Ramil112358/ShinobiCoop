#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/SpringArmComponent.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiDragonMount.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Dragon.h"
#include "Shinobi/Abilities/GA_Melee.h"

namespace
{
	AShinobiPlayerCharacter* Rider(FShinobiTestWorld& W, const FVector& Location = FVector::ZeroVector)
	{
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(Location);
		ShinobiTest::ApplyEffect(P, UGE_Mounted::StaticClass());
		W.Tick(0.2f); // the movement component notices the tag on its next move
		return P;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountFlightTest, "ShinobiCoop.Mount.TheRiderFliesWhileMountedAndFallsAfterwards", ShinobiTestFlags)
bool FShinobiMountFlightTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	TestTrue(TEXT("Mounted"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	TestEqual(TEXT("flying"), P->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Flying);
	TestEqual(TEXT("at dragon speed"), P->GetCharacterMovement()->GetMaxSpeed(), UShinobiMovementComponent::FlightSpeed);

	const float Height = P->GetActorLocation().Z;
	W.Tick(1.f);
	TestTrue(FString::Printf(TEXT("and does not fall (%.0f)"), P->GetActorLocation().Z - Height), FMath::Abs(P->GetActorLocation().Z - Height) < 5.f);

	W.Tick(UGE_Mounted::Seconds);
	TestFalse(TEXT("The mount ends"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	W.Tick(0.2f);
	TestEqual(TEXT("and the rider falls"), P->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Falling);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountClimbTest, "ShinobiCoop.Mount.SpaceClimbsAndControlDives", ShinobiTestFlags)
bool FShinobiMountClimbTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	const float Start = P->GetActorLocation().Z;

	P->SetFlightClimb(true);
	W.Tick(0.6f);
	const float Climbed = P->GetActorLocation().Z - Start;
	TestTrue(FString::Printf(TEXT("Climbs while Space is held (%.0f)"), Climbed), Climbed > 200.f);
	P->SetFlightClimb(false);

	P->SetFlightDive(true);
	W.Tick(2.f); // first the climb's momentum has to brake
	TestTrue(FString::Printf(TEXT("Dives while Control is held (%.0f)"), P->GetActorLocation().Z - Start), P->GetActorLocation().Z - Start < Climbed - 200.f);
	P->SetFlightDive(false);

	// Both keys cancel out; and neither does anything on the ground.
	W.Tick(1.f); // let it come to rest
	P->SetFlightClimb(true);
	P->SetFlightDive(true);
	const float Level = P->GetActorLocation().Z;
	W.Tick(0.5f);
	TestTrue(TEXT("Both held: level"), FMath::Abs(P->GetActorLocation().Z - Level) < 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountGroundTest, "ShinobiCoop.Mount.FlightKeysDoNothingOnTheGround", ShinobiTestFlags)
bool FShinobiMountGroundTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->SetFlightClimb(true);
	const float Start = P->GetActorLocation().Z;
	W.Tick(0.5f);
	TestTrue(TEXT("Not mounted: Space is only a jump"), FMath::Abs(P->GetActorLocation().Z - Start) < 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountEndsTest, "ShinobiCoop.Mount.EndsWhenTheRiderIsDownedOrDies", ShinobiTestFlags)
bool FShinobiMountEndsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Downed = Rider(W);
	AShinobiPlayerCharacter* Dead = Rider(W, FVector(0.f, 2000.f, 0.f));
	ShinobiTest::Damage(Downed, 600.f);
	TestTrue(TEXT("Downed"), ShinobiTest::HasTag(Downed, ShinobiTags::State_Downed));
	TestFalse(TEXT("A downed rider is dismounted"), ShinobiTest::HasTag(Downed, ShinobiTags::State_Mounted));

	ShinobiTest::ApplyEffect(Dead, UGE_Dead::StaticClass());
	TestFalse(TEXT("and so is a dead one"), ShinobiTest::HasTag(Dead, ShinobiTags::State_Mounted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountBlocksTest, "ShinobiCoop.Mount.MeleeAndDodgeAreOffWhileMounted", ShinobiTestFlags)
bool FShinobiMountBlocksTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Control dives instead of dodging"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	ShinobiTest::Release(P, EShinobiAbilityInputID::Dodge);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("and the swing is the dragon's breath, not a punch"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountCameraTest, "ShinobiCoop.Mount.TheCameraPullsBackWhileMounted", ShinobiTestFlags)
bool FShinobiMountCameraTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	W.Tick(0.2f);
	const USpringArmComponent* Boom = P->FindComponentByClass<USpringArmComponent>();
	if (!TestNotNull(TEXT("camera boom"), Boom))
	{
		return false;
	}
	const float OnFoot = Boom->TargetArmLength;
	ShinobiTest::ApplyEffect(P, UGE_Mounted::StaticClass());
	W.Tick(3.f);
	TestTrue(FString::Printf(TEXT("Pulled back from %.0f to %.0f"), OnFoot, Boom->TargetArmLength), Boom->TargetArmLength > OnFoot * 1.8f);
	P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Mounted));
	W.Tick(3.f);
	TestTrue(TEXT("and eased back on dismount"), FMath::IsNearlyEqual(Boom->TargetArmLength, OnFoot, 5.f));
	return true;
}

namespace
{
	AShinobiDragonMount* TheDragon(FShinobiTestWorld& W)
	{
		AShinobiDragonMount* Found = nullptr;
		for (TActorIterator<AShinobiDragonMount> It(W.World); It; ++It)
		{
			Found = *It;
		}
		return Found;
	}

	FString DragonClip(const AShinobiDragonMount* Dragon)
	{
		UAnimSingleNodeInstance* Instance = Dragon->GetBody()->GetSingleNodeInstance();
		return Instance && Instance->GetCurrentAsset() ? Instance->GetCurrentAsset()->GetName() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDragonCastTest, "ShinobiCoop.Mount.SummonDragonCostsChakraThenMountsTheRider", ShinobiTestFlags)
bool FShinobiDragonCastTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(5);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestEqual(TEXT("Cost 70 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 30.f);
	TestFalse(TEXT("Not mounted during the seals"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	W.Tick(1.3f);
	TestTrue(TEXT("Mounted after them"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonDragon));
	AShinobiDragonMount* Dragon = TheDragon(W);
	if (!TestNotNull(TEXT("with a dragon under them"), Dragon))
	{
		return false;
	}
	TestTrue(TEXT("that is theirs"), Dragon->GetRider() == P);
	TestEqual(TEXT("and the only one"), ShinobiTest::CountActors<AShinobiDragonMount>(W), 1);

	// While mounted the summon is off (and cooling down anyway): the same key is the dismount, tested below.
	TestFalse(TEXT("The jutsu itself is over"), ShinobiTest::IsActive(P, UGA_SummonDragon::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDragonDismountTest, "ShinobiCoop.Mount.TheSameKeyDismounts", ShinobiTestFlags)
bool FShinobiDragonDismountTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(5);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.3f);
	const float Chakra = ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute());

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Dismounted"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	TestTrue(TEXT("for free"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()) >= Chakra);
	W.Tick(0.2f);
	TestEqual(TEXT("and the dragon is gone"), ShinobiTest::CountActors<AShinobiDragonMount>(W), 0);
	TestTrue(TEXT("The cooldown still runs"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonDragon));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDragonLimitTest, "ShinobiCoop.Mount.OneDragonPerRiderAndItLeavesWithThem", ShinobiTestFlags)
bool FShinobiDragonLimitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A = Rider(W);
	AShinobiPlayerCharacter* B = Rider(W, FVector(0.f, 3000.f, 0.f));
	AShinobiDragonMount::Spawn(A);
	AShinobiDragonMount::Spawn(A); // replaces
	AShinobiDragonMount::Spawn(B);
	TestEqual(TEXT("One each"), ShinobiTest::CountActors<AShinobiDragonMount>(W), 2);

	ShinobiTest::Damage(A, 600.f); // downed: thrown from the dragon
	W.Tick(0.2f);
	TestEqual(TEXT("A downed rider's dragon leaves"), ShinobiTest::CountActors<AShinobiDragonMount>(W), 1);
	B->Destroy();
	W.Tick(0.2f);
	TestEqual(TEXT("and so does one whose rider is gone"), ShinobiTest::CountActors<AShinobiDragonMount>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDragonLookTest, "ShinobiCoop.Mount.TheDragonFlapsHoversAndGlides", ShinobiTestFlags)
bool FShinobiDragonLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	AShinobiDragonMount* Dragon = AShinobiDragonMount::Spawn(P);
	if (!TestNotNull(TEXT("dragon"), Dragon) || !TestTrue(TEXT("with its model (run Scripts/setup_creatures.py if not)"), Dragon->GetBody()->GetSkeletalMeshAsset() != nullptr))
	{
		return false;
	}
	TestEqual(TEXT("the dragon"), Dragon->GetBody()->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Dragon")));
	TestTrue(TEXT("Rides on the rider"), Dragon->GetAttachParentActor() == P);

	W.Tick(0.3f);
	TestEqual(TEXT("Hovering: wings flapping"), DragonClip(Dragon), FString(TEXT("ANIM_Dragon_Flap")));
	// Flying level and fast: gliding.
	for (int32 Step = 0; Step < 30; ++Step)
	{
		P->AddMovementInput(FVector::ForwardVector, 1.f);
		W.Tick(0.05f);
	}
	TestTrue(FString::Printf(TEXT("up to speed (%.0f)"), P->GetVelocity().Size2D()), P->GetVelocity().Size2D() > 800.f);
	TestEqual(TEXT("Fast and level: gliding"), DragonClip(Dragon), FString(TEXT("ANIM_Dragon_Glide")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDragonAssetsTest, "ShinobiCoop.Mount.DragonJutsuHasSealsAndIcon", ShinobiTestFlags)
bool FShinobiDragonAssetsTest::RunTest(const FString& Parameters)
{
	const UGA_SummonDragon* Jutsu = GetDefault<UGA_SummonDragon>();
	TestNotNull(TEXT("seal montage"), Jutsu->SealMontage.LoadSynchronous());
	TestNotNull(TEXT("release montage"), Jutsu->ReleaseMontage.LoadSynchronous());
	TestNotNull(TEXT("icon (run Scripts/setup_hud.py)"), Jutsu->Icon.LoadSynchronous());
	return true;
}

#endif
