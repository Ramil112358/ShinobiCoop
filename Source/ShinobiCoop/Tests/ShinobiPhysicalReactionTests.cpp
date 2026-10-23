#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "PhysicsControlComponent.h"
#include "Retargeter/IKRetargeter.h"
#include "Shinobi/Hero/ShinobiPhysicalReactionComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

namespace
{
	/** A player standing on a floor, possessed so its movement runs. */
	AShinobiPlayerCharacter* ReactionTestPlayer(FShinobiTestWorld& W)
	{
		AStaticMeshActor* Floor = W.Spawn<AStaticMeshActor>(FVector(0.f, 0.f, -10.f));
		Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); // a Static one refuses a mesh after spawning
		Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor->SetActorScale3D(FVector(40.f, 40.f, 0.2f));
		AShinobiPlayerCharacter* Player = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 100.f));
		W.Spawn<AAIController>()->Possess(Player);
		W.Tick(0.5f);
		return Player;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsHitTest, "ShinobiCoop.Reactions.HitShovesTheUpperBody", ShinobiTestFlags)
bool FShinobiReactionsHitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	UShinobiPhysicalReactionComponent* Reactions = P->GetPhysicalReactions();
	if (!TestTrue(TEXT("Players react physically"), Reactions && Reactions->IsPhysical()))
	{
		return false;
	}
	auto Head = [P]() { return P->GetMesh()->GetBodyInstance(TEXT("head"))->GetUnrealWorldTransform().GetLocation(); };
	const FVector Before = Head();
	Reactions->Hit(FVector(1.f, 0.f, 0.f), 30.f);
	W.Tick(0.1f);
	const float Shoved = Head().X - Before.X;
	TestTrue(FString::Printf(TEXT("The upper body gives along the hit (head %.1f cm)"), Shoved), Shoved > 3.f);
	W.Tick(1.f);
	const FVector Offset = P->GetMesh()->GetRelativeLocation();
	TestTrue(FString::Printf(TEXT("The body settles back on the capsule (mesh offset %s)"), *Offset.ToCompactString()),
		FVector::Dist(Offset, FVector(0.f, 0.f, -96.f)) < 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsVisibleHitTest, "ShinobiCoop.Reactions.HitShowsOnTheVisibleBody", ShinobiTestFlags)
bool FShinobiReactionsVisibleHitTest::RunTest(const FString& Parameters)
{
	// The physics runs on the hidden Manny; what the player sees is the roster body retargeted from it. A bot's punch (25)
	// used to sway the visible head ~4 cm: the world-space controls stayed at full strength and pinned every body to the
	// animation.
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	int32 Checked = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Index)
	{
		const FShinobiRosterEntry& Entry = AShinobiPlayerCharacter::Roster[Index];
		if (!LoadObject<USkeletalMesh>(nullptr, Entry.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet) ||
			!LoadObject<UIKRetargeter>(nullptr, Entry.Retargeter, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			continue; // not imported on this machine
		}
		P->SetCharacterIndex(uint8(Index));
		W.Tick(1.f); // settled from the last one's hit
		USkeletalMeshComponent* Body = P->GetBodyMesh();
		FName Head;
		for (const FMeshBoneInfo& Bone : Body->GetSkinnedAsset()->GetRefSkeleton().GetRefBoneInfo())
		{
			if (Bone.Name.ToString().EndsWith(TEXT("head"), ESearchCase::IgnoreCase))
			{
				Head = Bone.Name;
				break;
			}
		}
		const FString Name = Entry.Name;
		if (!TestTrue(Name + TEXT(": wears its body, with a head"), Body != P->GetMesh() && !Head.IsNone()))
		{
			continue;
		}
		auto HeadOffset = [Body, Head]() { return Body->GetSocketLocation(Head) - Body->GetComponentLocation(); }; // the capsule's drift aside
		const FVector Before = HeadOffset();
		P->GetPhysicalReactions()->Hit(FVector(1.f, 0.f, 0.f), 25.f);
		float Peak = 0.f;
		for (int32 Frame = 0; Frame < 30; ++Frame)
		{
			W.Tick(1.f / 60.f);
			Peak = FMath::Max(Peak, static_cast<float>(FVector::Dist(HeadOffset(), Before)));
		}
		TestTrue(FString::Printf(TEXT("%s: the visible head sways with the hit (%.1f cm)"), *Name, Peak), Peak > 12.f);
		W.Tick(1.f);
		const float Settled = static_cast<float>(FVector::Dist(HeadOffset(), Before));
		TestTrue(FString::Printf(TEXT("%s: and comes back onto the animation (%.1f cm off)"), *Name, Settled), Settled < 4.f);
		AddInfo(FString::Printf(TEXT("%s: head sway %.1f cm, settled %.1f cm"), *Name, Peak, Settled));
		++Checked;
	}
	TestTrue(TEXT("At least the default hero was checked"), Checked > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsCastAfterHitTest, "ShinobiCoop.Reactions.CastDuringHitRecoveryRestoresStrength", ShinobiTestFlags)
bool FShinobiReactionsCastAfterHitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	UPhysicsControlComponent* Physics = P->FindComponentByClass<UPhysicsControlComponent>();
	UAnimMontage* Seal = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Shinobi/Anims/AM_Fireball_Seal.AM_Fireball_Seal"));
	if (!TestNotNull(TEXT("Physics control"), Physics) || !TestNotNull(TEXT("Seal montage"), Seal))
	{
		return false;
	}
	P->GetPhysicalReactions()->Hit(FVector(1.f, 0.f, 0.f), 30.f);
	W.Tick(0.05f);
	P->GetMesh()->GetAnimInstance()->Montage_Play(Seal); // casting: animation only, mid-recovery
	W.Tick(0.6f);
	P->GetMesh()->GetAnimInstance()->Montage_Stop(0.f);
	W.Tick(0.1f);
	FPhysicsControlMultiplier Multiplier;
	Physics->GetControlMultiplier(Physics->GetControlNamesInSet(TEXT("ParentSpace"))[0], Multiplier);
	TestEqual(TEXT("Full strength once the cast ends"), Multiplier.AngularStrengthMultiplier, 1.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsKnockdownTest, "ShinobiCoop.Reactions.HardLaunchKnocksDownThenRecovers", ShinobiTestFlags)
bool FShinobiReactionsKnockdownTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	UShinobiPhysicalReactionComponent* Reactions = P->GetPhysicalReactions();

	P->LaunchCharacter(FVector(300.f, 0.f, 200.f), true, true); // a shove, not a throw
	TestFalse(TEXT("A soft launch doesn't knock down"), Reactions->IsKnockedDown());

	P->LaunchCharacter(FVector(1200.f, 0.f, 300.f), true, true);
	TestTrue(TEXT("A hard launch knocks down"), Reactions->IsKnockedDown());
	TestTrue(TEXT("Held while down"), ShinobiTest::HasTag(P, ShinobiTags::State_Staggered));
	W.Tick(0.05f); // the physics control applies the ragdoll profile on its next tick
	TestTrue(TEXT("The driver tumbles as a ragdoll"), P->GetMesh()->IsSimulatingPhysics(TEXT("pelvis")));

	// It topples and lands lying before the getup starts (one shared launch velocity used to fly it upright, and the getup
	// began before it had come down).
	float HeadAtGetUp = -1.f;
	for (float T = 0.05f; T < Reactions->KnockdownHoldSeconds + 0.15f; T += 0.05f)
	{
		W.Tick(0.05f);
		const UAnimMontage* Montage = P->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage();
		if (HeadAtGetUp < 0.f && Montage && Montage->GetName().Contains(TEXT("getup")))
		{
			HeadAtGetUp = static_cast<float>(P->GetMesh()->GetBoneLocation(TEXT("head")).Z);
		}
	}
	TestTrue(FString::Printf(TEXT("Lying when the getup starts (head %.0f cm up)"), HeadAtGetUp), HeadAtGetUp >= 0.f && HeadAtGetUp < 70.f);
	TestFalse(TEXT("Back up"), Reactions->IsKnockedDown());
	const UAnimMontage* GetUp = P->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage();
	TestTrue(TEXT("Getting up with a getup animation"), GetUp && GetUp->GetName().Contains(TEXT("getup")));
	TestFalse(TEXT("Free to act again"), ShinobiTest::HasTag(P, ShinobiTags::State_Staggered));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsDeathTest, "ShinobiCoop.Reactions.DeathRagdollsTheDriver", ShinobiTestFlags)
bool FShinobiReactionsDeathTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	ShinobiTest::ApplyEffect(P, UGE_Dead::StaticClass());
	TestTrue(TEXT("The driver falls as a ragdoll"), P->GetMesh()->IsSimulatingPhysics(TEXT("pelvis")));
	W.Tick(1.25f);
	const float Head = static_cast<float>(P->GetMesh()->GetBoneLocation(TEXT("head")).Z);
	TestTrue(FString::Printf(TEXT("Lying on the floor, not sat on its legs (head %.0f cm up)"), Head), Head < 40.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsDownedTest, "ShinobiCoop.Reactions.DownedCollapsesThenRevivesUp", ShinobiTestFlags)
bool FShinobiReactionsDownedTest::RunTest(const FString& Parameters)
{
	// AM_Downed (the template's MM_Death_Front_01) ends hunched, hips at standing height: downed players stood bent over.
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	USkeletalMeshComponent* M = P->GetMesh();
	ShinobiTest::ApplyEffect(P, UGE_Downed::StaticClass());
	W.Tick(1.25f);
	TestTrue(FString::Printf(TEXT("Downed lies on the floor (head %.0f cm up)"), M->GetBoneLocation(TEXT("head")).Z), M->GetBoneLocation(TEXT("head")).Z < 40.f);
	TestTrue(TEXT("Over its capsule, where the reviver kneels"), FVector::Dist2D(M->GetBoneLocation(TEXT("pelvis")), P->GetActorLocation()) < 30.f);
	P->Revive();
	W.Tick(0.1f);
	const UAnimMontage* GetUp = M->GetAnimInstance()->GetCurrentActiveMontage();
	TestTrue(TEXT("Revived: getting up"), GetUp && GetUp->GetName().Contains(TEXT("getup")));
	W.Tick(3.f);
	TestTrue(FString::Printf(TEXT("Standing again (head %.0f cm up)"), M->GetBoneLocation(TEXT("head")).Z), M->GetBoneLocation(TEXT("head")).Z > 140.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiReactionsLaunchWhileDownedTest, "ShinobiCoop.Reactions.LaunchWhileDownedStaysDown", ShinobiTestFlags)
bool FShinobiReactionsLaunchWhileDownedTest::RunTest(const FString& Parameters)
{
	// A finisher's launch on the blow that downs: the knockdown's getup stood the downed body back up.
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = ReactionTestPlayer(W);
	ShinobiTest::ApplyEffect(P, UGE_Downed::StaticClass());
	P->LaunchCharacter(FVector(1200.f, 0.f, 300.f), true, true);
	TestFalse(TEXT("No knockdown on a downed body"), P->GetPhysicalReactions()->IsKnockedDown());
	W.Tick(4.f);
	const float Head = static_cast<float>(P->GetMesh()->GetBoneLocation(TEXT("head")).Z);
	TestTrue(FString::Printf(TEXT("Still lying (head %.0f cm up)"), Head), Head < 40.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
