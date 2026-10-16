#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "UObject/UObjectIterator.h"
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
	// The visible body ragdolls itself, or follows the driver's ragdoll when physical reactions are on.
	TestTrue(TEXT("The visible body ragdolls"), P->GetBodyMesh()->IsSimulatingPhysics() || P->GetMesh()->IsSimulatingPhysics(TEXT("pelvis")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSealHandContactTest, "ShinobiCoop.Hero.SealHandsTouchOnEveryBody", ShinobiTestFlags)
bool FShinobiSealHandContactTest::RunTest(const FString& Parameters)
{
	// The Fireball seals end on Tiger (palms pressed together, wrists ~3 cm apart in the source clip). Manny's clip has
	// its hand contact baked (Scripts/setup_jutsu_anims.py); each hero keeps Manny's at runtime (UShinobiRetargetAnimInstance).
	UAnimMontage* Seal = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Shinobi/Anims/AM_Fireball_Seal.AM_Fireball_Seal"));
	if (!TestNotNull(TEXT("AM_Fireball_Seal"), Seal))
	{
		return false;
	}
	FShinobiTestWorld W;
	AStaticMeshActor* Floor = W.Spawn<AStaticMeshActor>(FVector(0.f, 0.f, -10.f)); // standing: in the air the driver's arms do their own thing
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetActorScale3D(FVector(40.f, 40.f, 0.2f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 100.f));
	if (!TestNotNull(TEXT("HeroMesh in use"), P->GetHeroMesh()))
	{
		return false;
	}
	W.Spawn<AAIController>()->Possess(P);
	for (TObjectIterator<UPoseSearchDatabase> It; It; ++It) // the driver's motion matching needs its indexes to pose at all
	{
		using namespace UE::PoseSearch;
		FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(*It, ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
	}
	W.Tick(0.5f);
	int32 Checked = 0;
	for (int32 Index = UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster) - 1; Index >= 0; --Index)
	{
		const FShinobiRosterEntry& Entry = AShinobiPlayerCharacter::Roster[Index];
		if (!LoadObject<USkeletalMesh>(nullptr, Entry.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet) ||
			!LoadObject<UIKRetargeter>(nullptr, Entry.Retargeter, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			continue;
		}
		P->SetCharacterIndex(uint8(Index));
		W.Tick(0.1f);
		UAnimInstance* Driver = P->GetMesh()->GetAnimInstance();
		Driver->Montage_Play(Seal, 1.f, EMontagePlayReturnType::MontageLength, 0.f);
		for (int32 Step = 0; Step < 10; ++Step) // into the Tiger hold (0.42 s on)
		{
			W.Tick(0.05f);
		}
		TestTrue(Entry.Name + FString(TEXT(": seal playing")), Driver->Montage_IsPlaying(Seal));
		const USkeletalMeshComponent* Manny = P->GetMesh();
		const USkeletalMeshComponent* Body = P->GetHeroMesh();
		const double MannyApart = FVector::Dist(Manny->GetBoneLocation(TEXT("hand_l")), Manny->GetBoneLocation(TEXT("hand_r")));
		const double Scale = FVector::Dist(Body->GetBoneLocation(TEXT("RightHand")), Body->GetBoneLocation(TEXT("RightForeArm"))) /
			FVector::Dist(Manny->GetBoneLocation(TEXT("hand_r")), Manny->GetBoneLocation(TEXT("lowerarm_r")));
		const double BodyApart = FVector::Dist(Body->GetBoneLocation(TEXT("LeftHand")), Body->GetBoneLocation(TEXT("RightHand")));
		const FString Name = Entry.Name;
		// And each hand faces as Manny's does (setup_hero.py's align_hand_roll; mitten hands have no knuckle row to check).
		for (const TCHAR* Side : {TEXT("Left"), TEXT("Right")})
		{
			if (Body->GetBoneIndex(*(Side + FString(TEXT("HandPinky1")))) == INDEX_NONE)
			{
				continue;
			}
			const FString S = FString(Side) == TEXT("Left") ? TEXT("l") : TEXT("r");
			auto MannyDir = [&](const TCHAR* From, const TCHAR* To) { return (Manny->GetBoneLocation(*(To + S)) - Manny->GetBoneLocation(*(From + S))).GetSafeNormal(); };
			auto BodyDir = [&](const TCHAR* From, const TCHAR* To) { return (Body->GetBoneLocation(*(Side + FString(To))) - Body->GetBoneLocation(*(Side + FString(From)))).GetSafeNormal(); };
			const double Fingers = FMath::RadiansToDegrees(FMath::Acos(MannyDir(TEXT("hand_"), TEXT("middle_01_")) | BodyDir(TEXT("Hand"), TEXT("HandMiddle1"))));
			const double Across = FMath::RadiansToDegrees(FMath::Acos(MannyDir(TEXT("pinky_01_"), TEXT("index_01_")) | BodyDir(TEXT("HandPinky1"), TEXT("HandIndex1"))));
			TestTrue(FString::Printf(TEXT("%s %s hand points %.0f deg off Manny's"), *Name, Side, Fingers), Fingers < 5.0);
			TestTrue(FString::Printf(TEXT("%s %s knuckle row %.0f deg off Manny's"), *Name, Side, Across), Across < 20.0);
		}
		TestTrue(FString::Printf(TEXT("%s: Manny's wrists %.1f cm apart in Tiger"), *Name, MannyApart), MannyApart < 6.0);
		TestTrue(FString::Printf(TEXT("%s: wrists %.1f cm apart, Manny's scaled %.1f"), *Name, BodyApart, MannyApart * Scale),
			FMath::Abs(BodyApart - MannyApart * Scale) < 1.5);
		Driver->Montage_Stop(0.f, Seal);
		++Checked;
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitWornTest, "ShinobiCoop.Hero.OutfitFollowsBodyAndComesOff", ShinobiTestFlags)
bool FShinobiOutfitWornTest::RunTest(const FString& Parameters)
{
	// Every imported outfit (Scripts/setup_hero.py) over every imported roster character.
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	if (!TestNotNull(TEXT("HeroMesh in use"), P->GetHeroMesh()) || !TestNotNull(TEXT("OutfitMesh exists"), P->GetOutfitMesh()))
	{
		return false;
	}
	TestFalse(TEXT("No outfit by default"), P->GetOutfitMesh()->IsVisible());
	int32 Checked = 0;
	for (int32 Outfit = 1; Outfit < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits); ++Outfit)
	{
		const USkeletalMesh* Cloth = LoadObject<USkeletalMesh>(nullptr, AShinobiPlayerCharacter::Outfits[Outfit].Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Cloth)
		{
			continue;
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Index)
		{
			if (!LoadObject<USkeletalMesh>(nullptr, AShinobiPlayerCharacter::Roster[Index].Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				continue;
			}
			P->SetCharacterIndex(uint8(Index));
			P->SetOutfitIndex(uint8(Outfit));
			W.Tick(0.1f);
			const FString Who = FString::Printf(TEXT("%s in %s"), AShinobiPlayerCharacter::Outfits[Outfit].Name, AShinobiPlayerCharacter::Roster[Index].Name);
			const USkeletalMeshComponent* Worn = P->GetOutfitMesh();
			TestTrue(Who + TEXT(" shows the outfit mesh"), Worn->GetSkeletalMeshAsset() == Cloth && Worn->IsVisible());
			TestTrue(Who + TEXT(" follows the body's pose"), Worn->LeaderPoseComponent.Get() == P->GetHeroMesh());
			// The collar sits at the head, the hem above the knees: the cloth hangs within the body's height, centred on it
			// (asset bounds moved by the component's offset: the component's own bounds follow the body's).
			const FBox Cloth2 = Cloth->GetBounds().GetBox().ShiftBy(Worn->GetRelativeLocation());
			const FBox BodyBox = P->GetHeroMesh()->GetSkeletalMeshAsset()->GetBounds().GetBox();
			TestTrue(FString::Printf(TEXT("%s spans %.0f-%.0f cm above the feet"), *Who, Cloth2.Min.Z, Cloth2.Max.Z),
				Cloth2.Min.Z > 20.f && Cloth2.Max.Z < 190.f && Cloth2.Max.Z > 120.f);
			const FVector Off = Cloth2.GetCenter() - BodyBox.GetCenter();
			AddInfo(FString::Printf(TEXT("%s: outfit centre is (%.1f, %.1f) cm from the body's in X/Y"), *Who, Off.X, Off.Y));
			// Y is front-to-back: a tail (Cat, Misha) pulls the body's bounds centre backwards, so it gets more room.
			TestTrue(Who + TEXT(" is centred on the body"), FMath::Abs(Off.X) < 10.f && FMath::Abs(Off.Y) < 15.f);
			++Checked;
		}
	}
	AddInfo(FString::Printf(TEXT("Checked %d imported outfit/character pair(s)"), Checked));
	P->SetOutfitIndex(0);
	TestFalse(TEXT("None takes it off"), P->GetOutfitMesh()->IsVisible());
	P->SetOutfitIndex(200);
	TestFalse(TEXT("An out-of-range pick wears nothing"), P->GetOutfitMesh()->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutfitSurvivesRespawnTest, "ShinobiCoop.Hero.OutfitPickSurvivesRespawn", ShinobiTestFlags)
bool FShinobiOutfitSurvivesRespawnTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerController* PC = W.World->SpawnActor<AShinobiPlayerController>();
	AShinobiPlayerCharacter* First = W.Spawn<AShinobiPlayerCharacter>();
	PC->Possess(First);
	const int32 Pick = UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits) - 1;
	PC->ShinobiOutfit(FString(AShinobiPlayerCharacter::Outfits[Pick].Name).ToLower());
	TestEqual(TEXT("The pick reaches the pawn"), int32(First->GetOutfitIndex()), Pick);
	PC->UnPossess();
	First->Destroy();
	W.Tick(0.1f);
	AShinobiPlayerCharacter* Second = W.Spawn<AShinobiPlayerCharacter>();
	PC->Possess(Second);
	TestEqual(TEXT("The respawned pawn keeps it"), int32(Second->GetOutfitIndex()), Pick);
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
