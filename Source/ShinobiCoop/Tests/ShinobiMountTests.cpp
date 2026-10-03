#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Dodge.h"
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

#endif
