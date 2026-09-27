#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiTrainingDummy.h"
#include "Shinobi/Hero/ShinobiRetargetAnimInstance.h"
#include "Shinobi/UI/ShinobiOverheadTextComponent.h"

// These run against whatever /Game/Shinobi/Hero holds: the stand-in Mixamo mannequin until the real hero is imported
// (Scripts/setup_hero.py). Both are Mixamo rigs, so the same checks apply.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiHeroInUseTest, "ShinobiCoop.Hero.PlayerUsesHeroWhenAssetsExist", ShinobiTestFlags)
bool FShinobiHeroInUseTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	USkeletalMeshComponent* Hero = P->GetHeroMesh();
	if (!TestNotNull(TEXT("HeroMesh in use"), Hero))
	{
		return false;
	}
	TestTrue(TEXT("Hero runs the retarget anim instance"), Cast<UShinobiRetargetAnimInstance>(Hero->GetAnimInstance()) != nullptr);
	TestFalse(TEXT("Manny driver hidden"), P->GetMesh()->IsVisible());
	TestTrue(TEXT("Driver keeps animating while hidden"),
		P->GetMesh()->VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones);
	TestTrue(TEXT("Body mesh is the hero"), P->GetBodyMesh() == Hero);
	TestEqual(TEXT("Hand bone for the cast orb"), P->GetRightHandBone(), FName(TEXT("RightHand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiHeroCvarOffTest, "ShinobiCoop.Hero.CvarOffKeepsManny", ShinobiTestFlags)
bool FShinobiHeroCvarOffTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* HeroVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.Hero"));
	if (!TestNotNull(TEXT("Shinobi.Hero exists"), HeroVar))
	{
		return false;
	}
	TGuardConsoleVariable<bool> HeroGuard(HeroVar, false, ECVF_SetByConsole);
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	TestNull(TEXT("No HeroMesh"), P->GetHeroMesh());
	TestTrue(TEXT("Manny visible"), P->GetMesh()->IsVisible());
	TestTrue(TEXT("Body mesh is Manny"), P->GetBodyMesh() == P->GetMesh());
	TestEqual(TEXT("Manny hand bone"), P->GetRightHandBone(), FName(TEXT("hand_r")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBodyMeshFallbackTest, "ShinobiCoop.Hero.BodyMeshFallsBackToDriver", ShinobiTestFlags)
bool FShinobiBodyMeshFallbackTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	const AShinobiTrainingDummy* Dummy = W.Spawn<AShinobiTrainingDummy>();
	TestTrue(TEXT("Dummies are their own visible Manny"), Dummy->GetBodyMesh() == Dummy->GetMesh());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiHeroRagdollTest, "ShinobiCoop.Hero.DeadRagdollsBody", ShinobiTestFlags)
bool FShinobiHeroRagdollTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Dead::StaticClass());
	TestTrue(TEXT("The visible body ragdolls"), P->GetBodyMesh()->IsSimulatingPhysics());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiHeroRespawnTest, "ShinobiCoop.Hero.HeroSurvivesRespawn", ShinobiTestFlags)
bool FShinobiHeroRespawnTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* First = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(First, UGE_Dead::StaticClass());
	First->Destroy();
	W.Tick(0.1f);
	const AShinobiPlayerCharacter* Second = W.Spawn<AShinobiPlayerCharacter>();
	TestNotNull(TEXT("The respawned pawn is the hero too"), Second->GetHeroMesh());
	TestFalse(TEXT("And not a ragdoll"), Second->GetBodyMesh()->IsSimulatingPhysics());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiNameTagTest, "ShinobiCoop.Hero.NameTagOnlyOnTeammates", ShinobiTestFlags)
bool FShinobiNameTagTest::RunTest(const FString& Parameters)
{
	// The headless test world has no ULocalPlayer, and in UE 5.8 a PlayerController without one is never "local"
	// when there's no net driver, so the own-character case can't be staged here (it's playtest row 3). This checks
	// the rule the tag follows (shown exactly when the pawn isn't locally controlled) on a teammate.
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Mate = W.Spawn<AShinobiPlayerCharacter>();
	W.World->SpawnActor<APlayerController>()->Possess(Mate);
	ShinobiTest::Hover(Mate);
	W.Tick(0.1f);
	TestFalse(TEXT("A teammate isn't locally controlled here"), Mate->IsLocallyControlled());
	TestTrue(TEXT("Teammate tagged"), Mate->GetNameTag()->IsVisible());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
