#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "Animation/AnimMontage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "Shinobi/ShinobiMeteor.h"
#include "Shinobi/ShinobiWaterDragon.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/ShinobiTelegraph.h"
#include "Shinobi/Abilities/GA_FireMeteor.h"
#include "Shinobi/Abilities/GA_IceMeteor.h"
#include "Shinobi/Abilities/GA_WaterDragon.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "Shinobi/VFX/ShinobiFirePatch.h"

namespace
{
	/** A 100-unit cube scaled into a wall for the aim to hit (the test world is empty). */
	AStaticMeshActor* SpawnMeteorWall(FShinobiTestWorld& W, const FVector& Location, const FVector& Scale)
	{
		AStaticMeshActor* Block = W.World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Block->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Block->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Block->SetActorScale3D(Scale);
		return Block;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChillSlowsTest, "ShinobiCoop.Chill.SlowsMovementRefreshesAndExpires", ShinobiTestFlags)
bool FShinobiChillSlowsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>();
	const UCharacterMovementComponent* Movement = E->GetCharacterMovement();
	const float Normal = Movement->GetMaxSpeed();
	TestTrue(TEXT("A base speed to compare with"), Normal > 100.f);

	ShinobiTest::ApplyEffect(E, UGE_Chill::StaticClass());
	TestTrue(TEXT("Chilled"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	TestEqual(TEXT("Moves at 60%"), Movement->GetMaxSpeed(), Normal * 0.6f);

	W.Tick(2.5f);
	ShinobiTest::ApplyEffect(E, UGE_Chill::StaticClass()); // another hit refreshes it instead of stacking
	W.Tick(2.5f);
	TestTrue(TEXT("Still chilled 5 s after the first hit"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	W.Tick(0.8f);
	TestFalse(TEXT("Three seconds after the last hit it wears off"), ShinobiTest::HasTag(E, ShinobiTags::State_Chilled));
	TestEqual(TEXT("and the speed is back"), Movement->GetMaxSpeed(), Normal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChillAuraTest, "ShinobiCoop.Chill.TintsTheBody", ShinobiTestFlags)
bool FShinobiChillAuraTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Chill::StaticClass());
	TestTrue(TEXT("The aura shows the chill"), P->GetChakraAura()->GetShownTag() == ShinobiTags::State_Chilled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTelegraphTest, "ShinobiCoop.Telegraph.ShowsThenExpires", ShinobiTestFlags)
bool FShinobiTelegraphTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	const AShinobiTelegraph* Ring = AShinobiTelegraph::Spawn(W.World, FVector::ZeroVector, 300.f, 1.f, FLinearColor::White);
	if (TestNotNull(TEXT("A telegraph"), Ring))
	{
		TestEqual(TEXT("with the radius it was given"), Ring->GetRadius(), 300.f);
	}
	W.Tick(0.5f);
	TestEqual(TEXT("still showing half way"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 1);
	W.Tick(0.8f);
	TestEqual(TEXT("gone once its time is up"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeteorTargetingTest, "ShinobiCoop.Meteor.TargetPreviewRequiresConfirmation", ShinobiTestFlags)
bool FShinobiMeteorTargetingTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1500.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f));
	SpawnMeteorWall(W, FVector(1500.f, 600.f, 0.f), FVector(1.f, 10.f, 10.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::EquipPage(P, 3);
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestTrue(TEXT("The first press enters target placement"), ShinobiTest::IsActive(P, UGA_FireMeteor::StaticClass()));
	TestEqual(TEXT("Target placement spends no chakra"), ShinobiTest::Get(P, Chakra), 100.f);
	TestFalse(TEXT("Target placement does not start cooldown"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_FireMeteor));
	TestEqual(TEXT("A local ground-area preview is visible"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 1);
	AShinobiTelegraph* Preview = nullptr;
	for (TActorIterator<AShinobiTelegraph> It(W.World); It; ++It)
	{
		Preview = *It;
	}
	if (TestNotNull(TEXT("The preview actor"), Preview))
	{
		TestEqual(TEXT("The preview uses the meteor's target radius"), Preview->GetRadius(), 450.f);
		TestFalse(TEXT("The movable preview is local, not replicated"), Preview->GetIsReplicated());
		const FVector InitialTarget = Preview->GetActorLocation();
		P->SetActorRotation(FRotator(0.f, 20.f, 0.f));
		W.Tick(0.11f);
		TestTrue(TEXT("The preview follows updated camera aim"), FVector::Dist2D(Preview->GetActorLocation(), InitialTarget) > 100.f);
	}

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Confirmation pays the meteor cost"), ShinobiTest::Get(P, Chakra), 50.f);
	TestEqual(TEXT("The preview is replaced by the normal seal phase"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 0);
	W.Tick(0.9f);
	TestEqual(TEXT("The confirmed cast starts its replicated warning"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeteorLeftClickConfirmTest, "ShinobiCoop.Meteor.LeftClickConfirmsPlacement", ShinobiTestFlags)
bool FShinobiMeteorLeftClickConfirmTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1500.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);

	TestTrue(TEXT("The left-click press is consumed by placement"), P->RouteTargetPlacementInput(EShinobiAbilityInputID::Melee, true));
	TestTrue(TEXT("The meteor proceeds into its cast"), ShinobiTest::IsActive(P, UGA_FireMeteor::StaticClass()));
	TestFalse(TEXT("Placement has ended"), P->IsJutsuTargetPlacementActive());
	TestEqual(TEXT("The click commits the jutsu cost"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 50.f);
	TestTrue(TEXT("The corresponding release is consumed too"), P->RouteTargetPlacementInput(EShinobiAbilityInputID::Melee, false));
	W.Tick(0.9f);
	TestEqual(TEXT("The click-confirmed meteor is performed"), ShinobiTest::CountActors<AShinobiMeteor>(W), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTelegraphSimplePreviewTest, "ShinobiCoop.Telegraph.PreviewIsASimpleSteadyOutline", ShinobiTestFlags)
bool FShinobiTelegraphSimplePreviewTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiTelegraph* Preview = AShinobiTelegraph::SpawnPreview(W.World, FVector::ZeroVector, 300.f, FLinearColor(0.05f, 0.85f, 0.75f));
	if (TestNotNull(TEXT("The placement preview exists"), Preview))
	{
		TArray<UStaticMeshComponent*> Meshes;
		Preview->GetComponents(Meshes);
		TestEqual(TEXT("The preview uses only the two outline rings"), Meshes.Num(), 2);
		TArray<FVector> InitialScales;
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			TestNotNull(*FString::Printf(TEXT("%s has ring geometry"), *Mesh->GetName()), Mesh->GetStaticMesh().Get());
			TestNotNull(*FString::Printf(TEXT("%s has a visible ring material"), *Mesh->GetName()), Mesh->GetMaterial(0));
			InitialScales.Add(Mesh->GetRelativeScale3D());
		}
		W.Tick(0.2f);
		for (int32 Index = 0; Index < Meshes.Num(); ++Index)
		{
			TestTrue(TEXT("The outline remains steady during placement"), Meshes[Index]->GetRelativeScale3D().Equals(InitialScales[Index]));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeteorPlacementRangeTest, "ShinobiCoop.Meteor.PlacementRangeIsReducedWithoutChangingArea", ShinobiTestFlags)
bool FShinobiMeteorPlacementRangeTest::RunTest(const FString& Parameters)
{
	const UGA_FireMeteor* FireCDO = GetDefault<UGA_FireMeteor>();
	const UGA_IceMeteor* IceCDO = GetDefault<UGA_IceMeteor>();
	TestEqual(TEXT("Fire Meteor placement reaches 2000 units"), FireCDO->Range, 2000.f);
	TestEqual(TEXT("Ice Meteors placement reaches 2000 units"), IceCDO->Range, 2000.f);
	TestEqual(TEXT("Fire Meteor area radius is unchanged"), FireCDO->Radius, 450.f);
	TestEqual(TEXT("Ice Meteors area radius is unchanged"), IceCDO->AreaRadius, 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeteorTargetCancelTest, "ShinobiCoop.Meteor.EscapeCancelsTargetPreviewForFree", ShinobiTestFlags)
bool FShinobiMeteorTargetCancelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1500.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);

	AShinobiPlayerController* PC = W.Spawn<AShinobiPlayerController>();
	PC->Possess(P);
	PC->TogglePauseMenu();
	TestFalse(TEXT("Escape cancels target placement"), ShinobiTest::IsActive(P, UGA_FireMeteor::StaticClass()));
	TestEqual(TEXT("Cancelling does not spend chakra"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	TestEqual(TEXT("Cancelling removes the preview"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeteorTargetsGroundAlongAim, "ShinobiCoop.Meteor.TargetsGroundAlongDownwardAim", ShinobiTestFlags)
bool FShinobiMeteorTargetsGroundAlongAim::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(0.f, 0.f, -150.f), FVector(100.f, 100.f, 1.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->SetActorRotation(FRotator(-30.f, 0.f, 0.f));
	ShinobiTest::EquipPage(P, 3);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestTrue(TEXT("Ground within the downward aim is a valid placement"), ShinobiTest::IsActive(P, UGA_FireMeteor::StaticClass()));
	TestEqual(TEXT("Placement remains free until confirmed"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeteorLookMovesTargetNotCamera, "ShinobiCoop.Meteor.LookInputMovesTargetNotCamera", ShinobiTestFlags)
bool FShinobiMeteorLookMovesTargetNotCamera::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1500.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestTrue(TEXT("Meteor is waiting for target placement"), P->IsJutsuTargetPlacementActive());

	AShinobiPlayerController* PC = W.Spawn<AShinobiPlayerController>();
	PC->Possess(P);
	PC->SetControlRotation(FRotator::ZeroRotator);
	const FRotator CameraBefore = PC->GetControlRotation();
	const FVector AimBefore = P->GetJutsuTargetAimDirection();
	AShinobiTelegraph* Preview = nullptr;
	for (TActorIterator<AShinobiTelegraph> It(W.World); It; ++It)
	{
		Preview = *It;
	}
	const FVector TargetBefore = Preview ? Preview->GetActorLocation() : FVector::ZeroVector;
	P->ApplyLookInput(FVector2D(1.f, 0.f));
	const FVector AimAfter = P->GetJutsuTargetAimDirection();
	TestTrue(TEXT("Look input leaves the camera rotation fixed"), PC->GetControlRotation().Equals(CameraBefore));
	TestTrue(TEXT("Horizontal mouse movement shifts the target ray"), FVector::Dist(AimBefore, AimAfter) > 0.001f);
	W.Tick(0.11f);
	TestTrue(TEXT("The world preview follows the shifted target ray"),
		Preview && FVector::Dist2D(Preview->GetActorLocation(), TargetBefore) > 1.f);

	TestTrue(TEXT("Escape cancels placement"), P->CancelJutsuTargetPlacement());
	TestFalse(TEXT("Target placement is no longer active"), P->IsJutsuTargetPlacementActive());
	const FVector2D CursorAfterCancel = P->GetJutsuTargetScreenPosition();
	P->ApplyLookInput(FVector2D(1.f, 0.f));
	TestEqual(TEXT("Look input no longer moves the virtual cursor"), P->GetJutsuTargetScreenPosition(), CursorAfterCancel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiFireMeteorTest, "ShinobiCoop.Meteor.FireMeteorWarnsFallsAndBurns", ShinobiTestFlags)
bool FShinobiFireMeteorTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1500.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f)); // the aim lands on its face at X = 1450
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(1400.f, 150.f, 0.f));
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(1400.f, 0.f, 0.f));
	for (AShinobiCharacterBase* Each : { static_cast<AShinobiCharacterBase*>(P), static_cast<AShinobiCharacterBase*>(Ally), E })
	{
		ShinobiTest::Hover(Each);
	}
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("First press only opens target placement"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Cost 50 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 50.f);
	W.Tick(0.9f);
	TestEqual(TEXT("A ring marks the spot after the seals"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 1);
	TestEqual(TEXT("and the meteor is falling"), ShinobiTest::CountActors<AShinobiMeteor>(W), 1);
	TestEqual(TEXT("Nothing is hurt yet: they can run"), ShinobiTest::Get(E, Health), 500.f);

	W.Tick(1.8f);
	TestEqual(TEXT("The meteor has landed"), ShinobiTest::CountActors<AShinobiMeteor>(W), 0);
	TestEqual(TEXT("and the ring is gone"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 0);
	TestEqual(TEXT("The enemy under it took 90"), ShinobiTest::Get(E, Health), 410.f);
	TestTrue(TEXT("and burns"), ShinobiTest::HasTag(E, ShinobiTags::State_Burning_Normal));
	TestEqual(TEXT("Fire is left on the ground"), ShinobiTest::CountActors<AShinobiFirePatch>(W), 1);
	TestEqual(TEXT("The ally beside it is unharmed"), ShinobiTest::Get(Ally, Health), 500.f);
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_FireMeteor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiFireMeteorSkyTest, "ShinobiCoop.Meteor.OpenSkyFailsFree", ShinobiTestFlags)
bool FShinobiFireMeteorSkyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W; // nothing to aim at: no wall, no ground
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestFalse(TEXT("The cast fails"), ShinobiTest::IsActive(P, UGA_FireMeteor::StaticClass()));
	TestEqual(TEXT("for free"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiIceShardTest, "ShinobiCoop.Meteor.IceShardHurtsChillsAndWaitsForItsDelay", ShinobiTestFlags)
bool FShinobiIceShardTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Hit = W.Spawn<AShinobiCharacterBase>(FVector(800.f, 0.f, 0.f));
	AShinobiCharacterBase* Beside = W.Spawn<AShinobiCharacterBase>(FVector(800.f, 300.f, 0.f));
	for (AShinobiCharacterBase* Each : { static_cast<AShinobiCharacterBase*>(P), Hit, Beside })
	{
		ShinobiTest::Hover(Each);
	}
	FShinobiMeteorImpact Impact;
	Impact.Damage = 12.f;
	Impact.Radius = 140.f;
	Impact.Status = UGE_Chill::StaticClass();
	AShinobiMeteor::Spawn(W.World, P, EShinobiMeteor::Ice, FVector(800.f, 0.f, 0.f), 0.5f, 1.f, Impact, 1.0f);

	W.Tick(0.9f); // 0.4 s into the fall, which only starts after the 1 s delay
	TestEqual(TEXT("Still waiting in the sky: nobody hurt"), ShinobiTest::Get(Hit, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	W.Tick(1.0f);
	TestEqual(TEXT("Landed"), ShinobiTest::CountActors<AShinobiMeteor>(W), 0);
	TestEqual(TEXT("The enemy under it took 12"), ShinobiTest::Get(Hit, UShinobiAttributeSet::GetHealthAttribute()), 488.f);
	TestTrue(TEXT("and is chilled"), ShinobiTest::HasTag(Hit, ShinobiTags::State_Chilled));
	TestEqual(TEXT("The one beside it is untouched"), ShinobiTest::Get(Beside, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestFalse(TEXT("and not chilled"), ShinobiTest::HasTag(Beside, ShinobiTags::State_Chilled));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiIceShowerTest, "ShinobiCoop.Meteor.IceShowerScattersShardsOverFourSeconds", ShinobiTestFlags)
bool FShinobiIceShowerTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1500.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f)); // the aim lands on its face at X = 1450
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Far = W.Spawn<AShinobiCharacterBase>(FVector(1400.f, 1500.f, 0.f)); // well outside the circle
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(1400.f, 100.f, 0.f));
	for (AShinobiCharacterBase* Each : { static_cast<AShinobiCharacterBase*>(P), Far, static_cast<AShinobiCharacterBase*>(Ally) })
	{
		ShinobiTest::Hover(Each);
	}

	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestEqual(TEXT("Cost 45 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 55.f);
	W.Tick(0.9f);
	TestEqual(TEXT("A ring marks the area"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 1);
	TestEqual(TEXT("Fourteen shards are queued"), ShinobiTest::CountActors<AShinobiMeteor>(W), 14);
	for (TActorIterator<AShinobiMeteor> It(W.World); It; ++It)
	{
		TestTrue(FString::Printf(TEXT("every shard lands inside the 500 circle (%.0f from the centre)"), FVector::Dist2D(It->GetImpactPoint(), FVector(1450.f, 0.f, 0.f))),
			FVector::Dist2D(It->GetImpactPoint(), FVector(1450.f, 0.f, 0.f)) <= 500.5f);
		TestTrue(TEXT("and is an ice shard"), It->GetKind() == EShinobiMeteor::Ice);
		// A hidden actor with no collision is not relevant to other clients: a partner would only get the shard when it
		// starts to fall, then wait out its delay a second time. Queued shards hide their parts, not themselves.
		TestFalse(TEXT("and a queued shard is not a hidden actor"), It->IsHidden());
	}

	W.Tick(2.0f);
	const int32 Midway = ShinobiTest::CountActors<AShinobiMeteor>(W);
	TestTrue(FString::Printf(TEXT("Some have landed, some still fall (%d left)"), Midway), Midway > 0 && Midway < 14);
	W.Tick(3.0f);
	TestEqual(TEXT("All have landed after about 4.5 s"), ShinobiTest::CountActors<AShinobiMeteor>(W), 0);
	TestEqual(TEXT("The ring is gone"), ShinobiTest::CountActors<AShinobiTelegraph>(W), 0);
	TestEqual(TEXT("An enemy outside the circle is untouched"), ShinobiTest::Get(Far, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestEqual(TEXT("An ally inside it is untouched"), ShinobiTest::Get(Ally, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestFalse(TEXT("and not chilled"), ShinobiTest::HasTag(Ally, ShinobiTags::State_Chilled));
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_IceMeteor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWaterDragonTest, "ShinobiCoop.Dragon.HitsEachEnemyOnceAndSkipsAllies", ShinobiTestFlags)
bool FShinobiWaterDragonTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(); // aims along +X
	AShinobiCharacterBase* First = W.Spawn<AShinobiCharacterBase>(FVector(700.f, 0.f, 0.f));
	AShinobiCharacterBase* Second = W.Spawn<AShinobiCharacterBase>(FVector(1300.f, 0.f, 0.f));
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(1000.f, 0.f, 0.f)); // in its path
	AShinobiCharacterBase* Aside = W.Spawn<AShinobiCharacterBase>(FVector(700.f, 600.f, 0.f));
	for (AShinobiCharacterBase* Each : { static_cast<AShinobiCharacterBase*>(P), First, Second, static_cast<AShinobiCharacterBase*>(Ally), Aside })
	{
		ShinobiTest::Hover(Each);
	}
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	TestEqual(TEXT("Cost 40 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 60.f);
	W.Tick(0.9f);
	TestEqual(TEXT("The dragon is flying after the seals"), ShinobiTest::CountActors<AShinobiWaterDragon>(W), 1);

	W.Tick(1.5f);
	TestEqual(TEXT("and is gone when its time is up"), ShinobiTest::CountActors<AShinobiWaterDragon>(W), 0);
	TestEqual(TEXT("The first enemy took 55, once"), ShinobiTest::Get(First, Health), 445.f);
	TestEqual(TEXT("The second took 55, once"), ShinobiTest::Get(Second, Health), 445.f);
	TestTrue(TEXT("and both were thrown on down its path"), First->GetActorLocation().X > 700.0 && Second->GetActorLocation().X > 1300.0);
	TestEqual(TEXT("The ally in its path is unharmed"), ShinobiTest::Get(Ally, Health), 500.f);
	TestEqual(TEXT("and was not moved"), Ally->GetActorLocation().X, 1000.0);
	TestEqual(TEXT("An enemy off to the side is untouched"), ShinobiTest::Get(Aside, Health), 500.f);
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_WaterDragon));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWaterDragonWallTest, "ShinobiCoop.Dragon.BreaksOnAWall", ShinobiTestFlags)
bool FShinobiWaterDragonWallTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1050.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f)); // its face is at X = 1000
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Before = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	AShinobiCharacterBase* Behind = W.Spawn<AShinobiCharacterBase>(FVector(1300.f, 0.f, 0.f));
	for (AShinobiCharacterBase* Each : { static_cast<AShinobiCharacterBase*>(P), Before, Behind })
	{
		ShinobiTest::Hover(Each);
	}
	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	W.Tick(2.4f);
	TestEqual(TEXT("The enemy before the wall is hit"), ShinobiTest::Get(Before, UShinobiAttributeSet::GetHealthAttribute()), 445.f);
	TestEqual(TEXT("The one behind it is not"), ShinobiTest::Get(Behind, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestEqual(TEXT("The dragon is gone"), ShinobiTest::CountActors<AShinobiWaterDragon>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiElementalAssetsTest, "ShinobiCoop.Meteor.ElementalJutsuHaveAnimationsAndIcons", ShinobiTestFlags)
bool FShinobiElementalAssetsTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py and Scripts/setup_hud.py if an asset is missing)
	for (const UShinobiJutsuAbility* Jutsu : { static_cast<const UShinobiJutsuAbility*>(GetDefault<UGA_FireMeteor>()), static_cast<const UShinobiJutsuAbility*>(GetDefault<UGA_IceMeteor>()), static_cast<const UShinobiJutsuAbility*>(GetDefault<UGA_WaterDragon>()) })
	{
		const FString Name = Jutsu->DisplayName.ToString();
		TestNotNull(*(Name + TEXT(" seal montage")), Jutsu->SealMontage.LoadSynchronous());
		TestNotNull(*(Name + TEXT(" release montage")), Jutsu->ReleaseMontage.LoadSynchronous());
		TestNotNull(*(Name + TEXT(" icon")), Jutsu->Icon.LoadSynchronous());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeteorFloorTest, "ShinobiCoop.Meteor.AimingAtAWallLandsOnTheFloorBelow", ShinobiTestFlags)
bool FShinobiMeteorFloorTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnMeteorWall(W, FVector(1500.f, 0.f, -150.f), FVector(30.f, 30.f, 1.f)); // a floor whose top is at Z = -100
	SpawnMeteorWall(W, FVector(1500.f, 0.f, 0.f), FVector(1.f, 10.f, 10.f));     // the wall the crosshair is on, at about eye height
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	W.Tick(0.9f);
	const AShinobiTelegraph* Ring = nullptr;
	for (TActorIterator<AShinobiTelegraph> It(W.World); It; ++It)
	{
		Ring = *It;
	}
	if (TestNotNull(TEXT("The ring is up"), Ring))
	{
		TestTrue(FString::Printf(TEXT("and sits on the floor, not on the wall at chest height (Z=%.0f)"), Ring->GetActorLocation().Z), FMath::Abs(Ring->GetActorLocation().Z + 100.0) < 5.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWaterDragonCloseTest, "ShinobiCoop.Dragon.PointBlankTargetStillSendsItForward", ShinobiTestFlags)
bool FShinobiWaterDragonCloseTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Adjacent = W.Spawn<AShinobiCharacterBase>(FVector(100.f, 0.f, 0.f)); // the crosshair lands on it, nearer than the dragon's mouth
	AShinobiCharacterBase* Further = W.Spawn<AShinobiCharacterBase>(FVector(700.f, 0.f, 0.f));
	for (AShinobiCharacterBase* Each : { static_cast<AShinobiCharacterBase*>(P), Adjacent, Further })
	{
		ShinobiTest::Hover(Each);
	}
	ShinobiTest::EquipPage(P, 3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	W.Tick(2.4f);
	TestEqual(TEXT("The dragon went forward and caught the enemy behind the adjacent one"), ShinobiTest::Get(Further, UShinobiAttributeSet::GetHealthAttribute()), 445.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
