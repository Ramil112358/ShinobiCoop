#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/SpringArmComponent.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiDragonMount.h"
#include "Shinobi/VFX/ShinobiDragonFlame.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Dragon.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/ShinobiCombat.h"

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
	TestEqual(TEXT("the dragon"), Dragon->GetBody()->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Tairn")));
	TestTrue(TEXT("Rides on the rider"), Dragon->GetAttachParentActor() == P);

	W.Tick(0.3f);
	TestEqual(TEXT("Hovering: the idle hover"), DragonClip(Dragon), FString(TEXT("ANIM_Tairn_Idle")));
	P->SetFlightClimb(true);
	W.Tick(0.4f);
	TestEqual(TEXT("Climbing: wings beating"), DragonClip(Dragon), FString(TEXT("ANIM_Tairn_Flap")));
	P->SetFlightClimb(false);
	W.Tick(1.5f);
	// Flying level and fast: gliding.
	for (int32 Step = 0; Step < 30; ++Step)
	{
		P->AddMovementInput(FVector::ForwardVector, 1.f);
		W.Tick(0.05f);
	}
	TestTrue(FString::Printf(TEXT("up to speed (%.0f)"), P->GetVelocity().Size2D()), P->GetVelocity().Size2D() > 800.f);
	TestEqual(TEXT("Fast and level: gliding"), DragonClip(Dragon), FString(TEXT("ANIM_Tairn_Glide")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBreathTest, "ShinobiCoop.Mount.DragonBreathBurnsWhatIsInFrontAndNothingElse", ShinobiTestFlags)
bool FShinobiBreathTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	AShinobiCharacterBase* Front = W.Spawn<AShinobiCharacterBase>(FVector(800.f, 0.f, 0.f));
	AShinobiCharacterBase* Side = W.Spawn<AShinobiCharacterBase>(FVector(800.f, 600.f, 0.f)); // 37 degrees off
	AShinobiCharacterBase* Behind = W.Spawn<AShinobiCharacterBase>(FVector(-800.f, 0.f, 0.f));
	AShinobiCharacterBase* TooFar = W.Spawn<AShinobiCharacterBase>(FVector(3000.f, 0.f, 0.f));
	AShinobiPlayerCharacter* Friend = W.Spawn<AShinobiPlayerCharacter>(FVector(800.f, -40.f, 0.f));
	for (AShinobiCharacterBase* Each : { Front, Side, Behind, TooFar, (AShinobiCharacterBase*)Friend })
	{
		ShinobiTest::Hover(Each);
	}
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	TestTrue(TEXT("The swing button breathes fire"), ShinobiTest::IsActive(P, UGA_DragonBreath::StaticClass()));
	TestEqual(TEXT("for 15 chakra"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 85.f);
	W.Tick(2.f);
	TestFalse(TEXT("It is over after a moment"), ShinobiTest::IsActive(P, UGA_DragonBreath::StaticClass()));
	TestTrue(FString::Printf(TEXT("The enemy ahead took several pulses (%.0f left)"), ShinobiTest::Get(Front, Health)), ShinobiTest::Get(Front, Health) <= 500.f - 3 * UGA_DragonBreath::PulseDamage);
	TestTrue(TEXT("and burns"), ShinobiTest::HasTag(Front, ShinobiTags::State_Burning));
	TestEqual(TEXT("Off to the side: untouched"), ShinobiTest::Get(Side, Health), 500.f);
	TestEqual(TEXT("Behind: untouched"), ShinobiTest::Get(Behind, Health), 500.f);
	TestEqual(TEXT("Out of range: untouched"), ShinobiTest::Get(TooFar, Health), 500.f);
	TestEqual(TEXT("A teammate in the flames: untouched"), ShinobiTest::Get(Friend, Health), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBreathRulesTest, "ShinobiCoop.Mount.DragonBreathNeedsADragonAndCoolsDown", ShinobiTestFlags)
bool FShinobiBreathRulesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* OnFoot = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(OnFoot);
	ShinobiTest::Press(OnFoot, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("No breath on foot (the swing is a punch)"), ShinobiTest::IsActive(OnFoot, UGA_DragonBreath::StaticClass()));

	AShinobiPlayerCharacter* P = Rider(W, FVector(0.f, 5000.f, 0.f));
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Melee);
	W.Tick(2.f);
	TestTrue(TEXT("Cooling down"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_DragonBreath));
	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("so a second breath has to wait"), ShinobiTest::IsActive(P, UGA_DragonBreath::StaticClass()));
	ShinobiTest::Release(P, EShinobiAbilityInputID::Melee);
	W.Tick(UGA_DragonBreath::CooldownSeconds);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	TestTrue(TEXT("and then it is ready again"), ShinobiTest::IsActive(P, UGA_DragonBreath::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountSeatedTest, "ShinobiCoop.Mount.TheRiderSitsAndKeepsSittingWhileMounted", ShinobiTestFlags)
bool FShinobiMountSeatedTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py if the montage is missing)
	const UAnimMontage* Sit = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Shinobi/Anims/AM_Mount_Sit.AM_Mount_Sit"));
	if (!TestNotNull(TEXT("seated montage"), Sit))
	{
		return false;
	}
	const UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Shinobi/Anims/AS_Mount_Sit.AS_Mount_Sit"));
	if (TestNotNull(TEXT("seated clip"), Clip))
	{
		TestTrue(FString::Printf(TEXT("The montage is as long as its clip (%.2f vs %.2f): rerun setup_jutsu_anims.py if not"), Sit->GetPlayLength(), Clip->GetPlayLength()), FMath::IsNearlyEqual(Sit->GetPlayLength(), Clip->GetPlayLength(), 0.02f));
	}
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	UAnimInstance* Anim = P->GetMesh()->GetAnimInstance();
	if (!TestNotNull(TEXT("the character's animation instance"), Anim))
	{
		return false;
	}
	W.Tick(0.3f);
	TestFalse(TEXT("Standing on foot"), Anim->Montage_IsPlaying(Sit));

	ShinobiTest::ApplyEffect(P, UGE_Mounted::StaticClass());
	W.Tick(0.5f);
	TestTrue(TEXT("Seated once mounted"), Anim->Montage_IsPlaying(Sit));
	W.Tick(Sit->GetPlayLength() * 2.5f);
	TestTrue(TEXT("and still seated after the clip has looped"), Anim->Montage_IsPlaying(Sit));

	P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Mounted));
	W.Tick(1.f);
	TestFalse(TEXT("Standing again after the dismount"), Anim->Montage_IsPlaying(Sit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDragonSeatOffsetTest, "ShinobiCoop.Mount.TheSaddleDoesNotBakeInNetworkSmoothing", ShinobiTestFlags)
bool FShinobiDragonSeatOffsetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A = Rider(W);
	AShinobiPlayerCharacter* B = Rider(W, FVector(0.f, 3000.f, 0.f));
	AShinobiDragonMount* Normal = AShinobiDragonMount::Spawn(A);

	// On a client the movement component nudges the mesh every frame while smoothing; a dragon that appears mid-nudge must not keep it.
	B->GetMesh()->AddLocalOffset(FVector(0.f, 0.f, 40.f));
	AShinobiDragonMount* Nudged = AShinobiDragonMount::Spawn(B);
	B->GetMesh()->AddLocalOffset(FVector(0.f, 0.f, -40.f));
	W.Tick(0.05f);
	const FVector Where = Normal->GetActorLocation() - A->GetActorLocation();
	const FVector Then = Nudged->GetActorLocation() - B->GetActorLocation();
	TestTrue(FString::Printf(TEXT("Same saddle either way (%s vs %s)"), *Where.ToString(), *Then.ToString()), Where.Equals(Then, 2.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBreathGatesTest, "ShinobiCoop.Mount.BreathObeysTheCombatRules", ShinobiTestFlags)
bool FShinobiBreathGatesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	// Phased (Kamui): no attacking from the other side.
	AShinobiPlayerCharacter* Ghost = Rider(W);
	ShinobiTest::ApplyEffect(Ghost, UGE_Kamui::StaticClass());
	ShinobiTest::Press(Ghost, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("A phased rider cannot breathe"), ShinobiTest::IsActive(Ghost, UGA_DragonBreath::StaticClass()));

	// Starting a jutsu cancels the breath, like a melee combo.
	AShinobiPlayerCharacter* Caster = Rider(W, FVector(0.f, 3000.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Press(Caster, EShinobiAbilityInputID::Melee);
	TestTrue(TEXT("Breathing"), ShinobiTest::IsActive(Caster, UGA_DragonBreath::StaticClass()));
	ShinobiTest::Release(Caster, EShinobiAbilityInputID::Melee);
	ShinobiTest::Tap(Caster, EShinobiAbilityInputID::Jutsu1); // page 1: Fireball
	TestTrue(TEXT("The Fireball is being cast"), ShinobiTest::IsActive(Caster, UGA_Fireball::StaticClass()));
	TestFalse(TEXT("and the breath stopped"), ShinobiTest::IsActive(Caster, UGA_DragonBreath::StaticClass()));

	// A staggering hit puts the breath out.
	AShinobiPlayerCharacter* Hurt = Rider(W, FVector(0.f, -3000.f, 0.f));
	ShinobiTest::Press(Hurt, EShinobiAbilityInputID::Melee);
	W.Tick(0.1f);
	TestTrue(TEXT("Breathing"), ShinobiTest::IsActive(Hurt, UGA_DragonBreath::StaticClass()));
	ShinobiTest::Damage(Hurt, 30.f);
	TestFalse(TEXT("A staggering hit stops it"), ShinobiTest::IsActive(Hurt, UGA_DragonBreath::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDismountAnywhereTest, "ShinobiCoop.Mount.TheDismountKeyWorksOnAnyPage", ShinobiTestFlags)
bool FShinobiDismountAnywhereTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	P->ApplyJutsuPage(3); // key 1 is Fire Meteor here, so the page-5 dismount is out of reach
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Dismount);
	TestFalse(TEXT("The dedicated key still gets the rider down"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStaleFlightKeysTest, "ShinobiCoop.Mount.HeldKeysDoNotOutliveTheRide", ShinobiTestFlags)
bool FShinobiStaleFlightKeysTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	P->SetFlightClimb(true); // Space held... and the ride ends before it is released
	P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Mounted));
	W.Tick(0.3f);

	ShinobiTest::ApplyEffect(P, UGE_Mounted::StaticClass());
	W.Tick(0.3f);
	const float Level = P->GetActorLocation().Z;
	W.Tick(0.6f);
	TestTrue(FString::Printf(TEXT("A new ride starts level (%.0f)"), P->GetActorLocation().Z - Level), FMath::Abs(P->GetActorLocation().Z - Level) < 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBoomLeftAloneTest, "ShinobiCoop.Mount.TheBoomIsOnlyDrivenAroundARide", ShinobiTestFlags)
bool FShinobiBoomLeftAloneTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	USpringArmComponent* Boom = P->FindComponentByClass<USpringArmComponent>();
	Boom->TargetArmLength = 120.f; // the world tour's close-ups
	W.Tick(1.f);
	TestEqual(TEXT("Never mounted: someone else's setting stays"), Boom->TargetArmLength, 120.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountClearanceTest, "ShinobiCoop.Mount.TheDragonNeverSinksIntoTheGround", ShinobiTestFlags)
bool FShinobiMountClearanceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("engine cube"), Cube))
	{
		return false;
	}
	// A floor whose top is at Z = -88: where a standing player's feet are (the capsule centre is 88 above it).
	AStaticMeshActor* Floor = W.World->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -138.f), FRotator::ZeroRotator);
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Floor->SetActorScale3D(FVector(400.f, 400.f, 1.f));
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	W.Tick(0.3f); // lands
	const float Bottom = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	ShinobiTest::ApplyEffect(P, UGE_Mounted::StaticClass());
	W.Tick(1.5f);
	const float Gap = P->GetActorLocation().Z - Bottom + 88.f; // the capsule's floor above the ground
	TestTrue(FString::Printf(TEXT("Mounting lifts the rider clear of the ground (%.0f)"), Gap), Gap >= UShinobiMovementComponent::MountedGroundClearance - 15.f);

	P->SetFlightDive(true);
	W.Tick(2.f);
	const float Dived = P->GetActorLocation().Z - Bottom + 88.f;
	TestTrue(FString::Printf(TEXT("and diving cannot push the dragon into it (%.0f)"), Dived), Dived >= UShinobiMovementComponent::MountedGroundClearance - 15.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBreathAnimTest, "ShinobiCoop.Mount.TheDragonBreathesWithItsWholeHeadAndAJetOfFlame", ShinobiTestFlags)
bool FShinobiBreathAnimTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	AShinobiDragonMount* Dragon = AShinobiDragonMount::Spawn(P);
	if (!TestNotNull(TEXT("dragon"), Dragon))
	{
		return false;
	}
	W.Tick(0.2f);
	TestEqual(TEXT("No jet before the breath"), ShinobiTest::CountActors<AShinobiDragonFlame>(W), 0);

	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee);
	W.Tick(0.2f);
	TestTrue(TEXT("The breath tag is up"), ShinobiTest::HasTag(P, ShinobiTags::State_DragonBreathing));
	TestEqual(TEXT("The dragon plays its breath clip"), DragonClip(Dragon), FString(TEXT("ANIM_Tairn_Breath")));
	TestEqual(TEXT("One jet of flame"), ShinobiTest::CountActors<AShinobiDragonFlame>(W), 1);
	for (TActorIterator<AShinobiDragonFlame> It(W.World); It; ++It)
	{
		TestTrue(TEXT("with plenty of flames in it"), It->GetParticleCount() >= 40);
		TestTrue(TEXT("at the dragon's mouth"), FVector::Dist(It->GetActorLocation(), AShinobiDragonMount::MouthLocation(*P)) < 60.f);
		TestTrue(TEXT("belonging to the rider"), It->GetRider() == P);
	}

	W.Tick(UGA_DragonBreath::PulseCount * UGA_DragonBreath::PulseGap + 1.5f);
	TestFalse(TEXT("The breath tag is down again"), ShinobiTest::HasTag(P, ShinobiTags::State_DragonBreathing));
	TestEqual(TEXT("and the jet has burnt out"), ShinobiTest::CountActors<AShinobiDragonFlame>(W), 0);
	TestNotEqual(TEXT("The dragon is back to hovering"), DragonClip(Dragon), FString(TEXT("ANIM_Tairn_Breath")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTwoDragonsTest, "ShinobiCoop.Mount.TairnAndTheCrimsonDragonAreBothSummonable", ShinobiTestFlags)
bool FShinobiTwoDragonsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(5);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2); // slot 2: the crimson dragon
	TestEqual(TEXT("Cost 70 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 30.f);
	W.Tick(1.3f);
	TestTrue(TEXT("Mounted"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	TestTrue(TEXT("with its own cooldown running (Tairn's is not)"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonCrimsonDragon) && !ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonDragon));
	AShinobiDragonMount* Crimson = TheDragon(W);
	if (!TestNotNull(TEXT("a dragon"), Crimson))
	{
		return false;
	}
	TestEqual(TEXT("the crimson one"), Crimson->GetKind(), EShinobiDragonKind::Crimson);
	if (TestTrue(TEXT("with its model (run Scripts/setup_creatures.py if not)"), Crimson->GetBody()->GetSkeletalMeshAsset() != nullptr))
	{
		TestEqual(TEXT("the crimson dragon"), Crimson->GetBody()->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_CrimsonDragon")));
	}

	// Dismount, and Tairn can be called straight away (a different jutsu, a different cooldown).
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Dismount);
	W.Tick(0.3f);
	TestEqual(TEXT("The crimson dragon leaves"), ShinobiTest::CountActors<AShinobiDragonMount>(W), 0);
	ShinobiTest::Set(P, UShinobiAttributeSet::GetChakraAttribute(), 100.f);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.3f);
	AShinobiDragonMount* Tairn = TheDragon(W);
	if (TestNotNull(TEXT("and then Tairn comes"), Tairn))
	{
		TestEqual(TEXT("Tairn"), Tairn->GetKind(), EShinobiDragonKind::Tairn);
		TestEqual(TEXT("with his model"), Tairn->GetBody()->GetSkeletalMeshAsset() ? Tairn->GetBody()->GetSkeletalMeshAsset()->GetName() : FString(), FString(TEXT("SK_Tairn")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDragonFireTest, "ShinobiCoop.Mount.EachDragonBreathesItsOwnFire", ShinobiTestFlags)
bool FShinobiDragonFireTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A = Rider(W);
	AShinobiPlayerCharacter* B = Rider(W, FVector(0.f, 4000.f, 0.f));
	AShinobiDragonMount::Spawn(A, EShinobiDragonKind::Tairn);
	AShinobiDragonMount::Spawn(B, EShinobiDragonKind::Crimson);
	TestFalse(TEXT("The two sit at different mouths"), AShinobiDragonMount::MouthLocation(*A, EShinobiDragonKind::Tairn).Equals(A->GetActorLocation() + (AShinobiDragonMount::MouthLocation(*B, EShinobiDragonKind::Crimson) - B->GetActorLocation()), 1.f));

	ShinobiTest::Press(A, EShinobiAbilityInputID::Melee);
	ShinobiTest::Press(B, EShinobiAbilityInputID::Melee);
	W.Tick(0.2f);
	int32 Blue = 0, Orange = 0;
	for (TActorIterator<AShinobiDragonFlame> It(W.World); It; ++It)
	{
		Blue += It->GetKind() == EShinobiDragonKind::Tairn && It->GetRider() == A ? 1 : 0;
		Orange += It->GetKind() == EShinobiDragonKind::Crimson && It->GetRider() == B ? 1 : 0;
	}
	TestEqual(TEXT("Tairn's rider has the blue-white jet"), Blue, 1);
	TestEqual(TEXT("and the crimson dragon's rider the orange one"), Orange, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCrimsonAssetsTest, "ShinobiCoop.Mount.CrimsonDragonJutsuHasSealsAndIcon", ShinobiTestFlags)
bool FShinobiCrimsonAssetsTest::RunTest(const FString& Parameters)
{
	const UGA_SummonCrimsonDragon* Jutsu = GetDefault<UGA_SummonCrimsonDragon>();
	TestNotNull(TEXT("seal montage"), Jutsu->SealMontage.LoadSynchronous());
	TestNotNull(TEXT("release montage"), Jutsu->ReleaseMontage.LoadSynchronous());
	TestNotNull(TEXT("icon (run Scripts/setup_hud.py)"), Jutsu->Icon.LoadSynchronous());
	TestEqual(TEXT("slot 2 of page 5"), static_cast<int32>(Jutsu->InputID), static_cast<int32>(EShinobiAbilityInputID::Jutsu2));
	TestEqual(TEXT("page 5"), Jutsu->Page, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMountSteerTest, "ShinobiCoop.Mount.TheDragonFacesWhereTheCameraLooks", ShinobiTestFlags)
bool FShinobiMountSteerTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Mounted = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* OnFoot = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 4000.f, 0.f));
	AAIController* A = W.Spawn<AAIController>();
	AAIController* B = W.Spawn<AAIController>();
	A->bSetControlRotationFromPawnOrientation = false; // else the AI controller keeps resetting its rotation to the pawn's
	B->bSetControlRotationFromPawnOrientation = false;
	A->Possess(Mounted);
	B->Possess(OnFoot);
	ShinobiTest::Hover(OnFoot);
	ShinobiTest::ApplyEffect(Mounted, UGE_Mounted::StaticClass());
	W.Tick(0.2f);
	A->SetControlRotation(FRotator(0.f, 90.f, 0.f));
	B->SetControlRotation(FRotator(0.f, 90.f, 0.f));
	W.Tick(1.5f);
	TestTrue(FString::Printf(TEXT("A mounted rider turns to the camera's yaw (%.0f)"), Mounted->GetActorRotation().Yaw), FMath::Abs(FRotator::NormalizeAxis(Mounted->GetActorRotation().Yaw - 90.f)) < 8.f);
	TestTrue(FString::Printf(TEXT("a rider on foot does not (%.0f)"), OnFoot->GetActorRotation().Yaw), FMath::Abs(FRotator::NormalizeAxis(OnFoot->GetActorRotation().Yaw)) < 8.f);

	Mounted->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Mounted));
	W.Tick(0.3f);
	A->SetControlRotation(FRotator(0.f, -90.f, 0.f));
	W.Tick(1.f);
	TestTrue(FString::Printf(TEXT("and after the dismount it goes back to turning with its movement (%.0f)"), Mounted->GetActorRotation().Yaw), FMath::Abs(FRotator::NormalizeAxis(Mounted->GetActorRotation().Yaw - 90.f)) < 8.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBreathAimTest, "ShinobiCoop.Mount.TheBreathGoesWhereTheDragonFacesNotWhereTheCameraLooks", ShinobiTestFlags)
bool FShinobiBreathAimTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = Rider(W);
	AAIController* Eyes = W.Spawn<AAIController>();
	Eyes->bSetControlRotationFromPawnOrientation = false;
	Eyes->Possess(P);
	P->SetActorRotation(FRotator(0.f, 90.f, 0.f)); // facing +Y
	Eyes->SetControlRotation(FRotator(30.f, 0.f, 0.f)); // the camera looks along +X and down: the breath ignores it
	AShinobiCharacterBase* Ahead = W.Spawn<AShinobiCharacterBase>(FVector(0.f, 900.f, 0.f));
	AShinobiCharacterBase* WhereTheCameraLooks = W.Spawn<AShinobiCharacterBase>(FVector(900.f, 0.f, 0.f));
	ShinobiTest::Hover(Ahead);
	ShinobiTest::Hover(WhereTheCameraLooks);

	TestTrue(TEXT("The direction is the facing, level"), (AShinobiDragonMount::BreathDirection(*P) - FVector(0.f, 1.f, 0.f)).IsNearlyZero(0.01f));
	ShinobiTest::Press(P, EShinobiAbilityInputID::Melee); // the first pulse fires at once
	W.Tick(0.05f);
	TestTrue(TEXT("What the dragon faces is burnt"), ShinobiTest::Get(Ahead, UShinobiAttributeSet::GetHealthAttribute()) < 500.f);
	TestEqual(TEXT("what the camera looks at is not"), ShinobiTest::Get(WhereTheCameraLooks, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBreathTargetTest, "ShinobiCoop.Mount.TheCrosshairMarksWhereTheFireEnds", ShinobiTestFlags)
bool FShinobiBreathTargetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("engine cube"), Cube))
	{
		return false;
	}
	AShinobiPlayerCharacter* P = Rider(W);
	const FVector Mouth = AShinobiDragonMount::MouthLocation(*P);
	const FVector Open = AShinobiDragonMount::BreathTarget(*P);
	TestTrue(FString::Printf(TEXT("In the open it is the full reach ahead (%.0f)"), (Open - Mouth).Size()), FMath::IsNearlyEqual((Open - Mouth).Size(), UGA_DragonBreath::Range, 5.f));

	// A wall 6 m ahead stops it there.
	AStaticMeshActor* Wall = W.World->SpawnActor<AStaticMeshActor>(Mouth + FVector(650.f, 0.f, 0.f), FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Wall->SetActorScale3D(FVector(1.f, 30.f, 30.f));
	Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	const FVector Blocked = AShinobiDragonMount::BreathTarget(*P);
	TestTrue(FString::Printf(TEXT("and at a wall it ends on it (%.0f ahead)"), (Blocked - Mouth).Size()), FMath::Abs((Blocked - Mouth).Size() - 600.f) < 30.f);
	return true;
}

#endif
