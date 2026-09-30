#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Retargeter/IKRetargeter.h"
#include "Shinobi/ShinobiPlayerController.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickSwapsBodyTest, "ShinobiCoop.Hero.PickSwapsBody", ShinobiTestFlags)
bool FShinobiPickSwapsBodyTest::RunTest(const FString& Parameters)
{
	// Every roster character that's been imported (Scripts/setup_hero.py), last to first so the hero comes back last.
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	if (!TestNotNull(TEXT("HeroMesh in use"), P->GetHeroMesh()))
	{
		return false;
	}
	int32 Checked = 0;
	for (int32 Index = UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster) - 1; Index >= 0; --Index)
	{
		const FShinobiRosterEntry& Entry = AShinobiPlayerCharacter::Roster[Index];
		const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Entry.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
		const UIKRetargeter* Retargeter = LoadObject<UIKRetargeter>(nullptr, Entry.Retargeter, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh || !Retargeter)
		{
			continue;
		}
		P->SetCharacterIndex(uint8(Index));
		W.Tick(0.1f);
		const USkeletalMeshComponent* Body = P->GetHeroMesh();
		const UShinobiRetargetAnimInstance* Anim = Cast<UShinobiRetargetAnimInstance>(Body->GetAnimInstance());
		const FString Name = Entry.Name;
		TestTrue(Name + TEXT(" body"), Body->GetSkeletalMeshAsset() == Mesh);
		TestTrue(Name + TEXT(" retargeter"), Anim && Anim->GetRetargeter() == Retargeter);
		TestTrue(Name + TEXT(" has Mixamo hand and head bones"), Body->GetBoneIndex(TEXT("RightHand")) != INDEX_NONE && Body->GetBoneIndex(TEXT("Head")) != INDEX_NONE);
		TestTrue(Name + TEXT(" is the body mesh"), P->GetBodyMesh() == Body);
		// The retargeted pose must be full size: a stray scaled root bone (an FBX armature node) shrinks it to nothing.
		const float HeadHeight = Body->GetBoneLocation(TEXT("Head")).Z - Body->GetComponentLocation().Z;
		TestTrue(FString::Printf(TEXT("%s head %.0f cm above the feet"), *Name, HeadHeight), HeadHeight > 120.f && HeadHeight < 200.f);
		++Checked;
	}
	AddInfo(FString::Printf(TEXT("Checked %d imported roster character(s)"), Checked));
	TestTrue(TEXT("At least the hero was checked"), Checked > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBadPickTest, "ShinobiCoop.Hero.BadPickFallsBackToHero", ShinobiTestFlags)
bool FShinobiBadPickTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	P->SetCharacterIndex(200);
	const USkeletalMesh* Hero = LoadObject<USkeletalMesh>(nullptr, AShinobiPlayerCharacter::Roster[0].Mesh);
	TestTrue(TEXT("Out-of-range pick shows the hero"), P->GetHeroMesh() && P->GetHeroMesh()->GetSkeletalMeshAsset() == Hero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickSurvivesRespawnTest, "ShinobiCoop.Hero.PickSurvivesRespawn", ShinobiTestFlags)
bool FShinobiPickSurvivesRespawnTest::RunTest(const FString& Parameters)
{
	// Standalone test world: the Server RPC runs in place.
	FShinobiTestWorld W;
	AShinobiPlayerController* PC = W.World->SpawnActor<AShinobiPlayerController>();
	AShinobiPlayerCharacter* First = W.Spawn<AShinobiPlayerCharacter>();
	PC->Possess(First);
	// The last roster entry (just the hero until photo-based heroes join; then this checks a real pick).
	const int32 Pick = UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster) - 1;
	PC->ShinobiCharacter(FString(AShinobiPlayerCharacter::Roster[Pick].Name).ToLower());
	TestEqual(TEXT("The pick reaches the pawn"), int32(First->GetCharacterIndex()), Pick);
	PC->UnPossess();
	First->Destroy();
	W.Tick(0.1f);
	AShinobiPlayerCharacter* Second = W.Spawn<AShinobiPlayerCharacter>();
	PC->Possess(Second);
	TestEqual(TEXT("The respawned pawn keeps it"), int32(Second->GetCharacterIndex()), Pick);
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
