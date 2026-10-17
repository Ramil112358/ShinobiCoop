#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/Enemies/ShinobiOwl.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/Abilities/GA_Summons.h"
#include "Animation/AnimSequence.h"
#include "Shinobi/Enemies/ShinobiCreatureAnimInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	/** A summoned pack, hovering (the test world has no floor). */
	TArray<AShinobiSummon*> Summon(AShinobiCharacterBase* Caster, EShinobiSummonKind Kind, const FVector& Ground)
	{
		TArray<AShinobiSummon*> Pack = AShinobiSummon::Summon(Caster, Kind, Ground);
		for (AShinobiSummon* Each : Pack)
		{
			ShinobiTest::Hover(Each);
		}
		return Pack;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonTeamTest, "ShinobiCoop.Summons.FightForTheCasterNotAgainstThem", ShinobiTestFlags)
bool FShinobiSummonTeamTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(5000.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Ninja);

	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Misha, FVector(300.f, 0.f, 0.f));
	TestEqual(TEXT("One Misha"), Pack.Num(), 1);
	if (Pack.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("On the players' side"), Pack[0]->GetTeam(), EShinobiTeam::Players);
	TestTrue(TEXT("Hostile to enemies"), Pack[0]->IsHostileTo(Ninja) && Ninja->IsHostileTo(Pack[0]));
	TestFalse(TEXT("Not hostile to the caster"), Pack[0]->IsHostileTo(Caster) || Caster->IsHostileTo(Pack[0]));
	TestTrue(TEXT("Remembers its summoner"), Pack[0]->GetSummoner() == Caster);
	TestEqual(TEXT("Misha health"), ShinobiTest::Get(Pack[0], UShinobiAttributeSet::GetMaxHealthAttribute()), 500.f);

	// A jutsu that hits everything hostile around the caster leaves the summon alone.
	const TArray<AShinobiCharacterBase*> Hit = ShinobiCombat::FindHostilesInRadius(Caster, FVector::ZeroVector, 1000.f);
	TestFalse(TEXT("The caster's area jutsu skips the summon"), Hit.Contains(Pack[0]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonFightTest, "ShinobiCoop.Summons.AttackEnemiesAndEnemiesAttackThem", ShinobiTestFlags)
bool FShinobiSummonFightTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(-2500.f, 0.f, 0.f)); // near enough to guard, far enough not to be the nearest target
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>();
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Ninja);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Misha, FVector(400.f, 0.f, 0.f));
	if (Pack.Num() != 1)
	{
		return false;
	}

	W.Tick(0.6f);
	TestTrue(TEXT("The ninja heads for Misha, not the far-off caster"), Ninja->GetActorLocation().X > 100.f);
	W.Tick(3.f);
	TestTrue(TEXT("Misha hurt the ninja"), ShinobiTest::Get(Ninja, UShinobiAttributeSet::GetHealthAttribute()) < 200.f);
	TestEqual(TEXT("The caster was never touched"), ShinobiTest::Get(Caster, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonLifetimeTest, "ShinobiCoop.Summons.LeaveWhenTheirTimeIsUp", ShinobiTestFlags)
bool FShinobiSummonLifetimeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Misha, FVector(500.f, 0.f, 0.f));
	const float Lifetime = AShinobiSummon::GetLifetime(EShinobiSummonKind::Misha);

	W.Tick(Lifetime - 1.f);
	TestEqual(TEXT("Still here before its time"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);
	W.Tick(1.2f);
	TestEqual(TEXT("Gone after its time"), ShinobiTest::CountActors<AShinobiSummon>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonLimitTest, "ShinobiCoop.Summons.OneOfEachKindPerCaster", ShinobiTestFlags)
bool FShinobiSummonLimitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Other = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 3000.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Other);

	Summon(Caster, EShinobiSummonKind::Misha, FVector(300.f, 0.f, 0.f));
	W.Tick(0.1f);
	Summon(Caster, EShinobiSummonKind::Misha, FVector(300.f, 300.f, 0.f));
	TestEqual(TEXT("The second Misha replaced the first"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);

	Summon(Caster, EShinobiSummonKind::Pabu, FVector(-500.f, 0.f, 0.f));
	TestEqual(TEXT("Pabu joins Misha"), ShinobiTest::CountActors<AShinobiSummon>(W), 2);
	Summon(Other, EShinobiSummonKind::Misha, FVector(0.f, 3300.f, 0.f));
	TestEqual(TEXT("Another caster's Misha is not replaced"), ShinobiTest::CountActors<AShinobiSummon>(W), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonPabuTest, "ShinobiCoop.Summons.PabuComesAloneAndSturdy", ShinobiTestFlags)
bool FShinobiSummonPabuTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(400.f, 0.f, 0.f));
	TestEqual(TEXT("One Pabu"), Pack.Num(), 1);
	if (Pack.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("Pabu health"), ShinobiTest::Get(Pack[0], UShinobiAttributeSet::GetMaxHealthAttribute()), 450.f);

	Summon(Caster, EShinobiSummonKind::Pabu, FVector(400.f, 0.f, 0.f));
	TestEqual(TEXT("Calling Pabu again replaces the first"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonDeathTest, "ShinobiCoop.Summons.AFallenSummonIsNotCounted", ShinobiTestFlags)
bool FShinobiSummonDeathTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(400.f, 0.f, 0.f));
	ShinobiTest::Damage(Pack[0], 1000.f);
	TestFalse(TEXT("Pabu fell"), Pack[0]->IsAlive());

	// Calling again does not wait on the body: the new Pabu arrives.
	Summon(Caster, EShinobiSummonKind::Pabu, FVector(400.f, 0.f, 0.f));
	int32 Alive = 0;
	for (TActorIterator<AShinobiSummon> It(W.World); It; ++It)
	{
		Alive += It->IsAlive() ? 1 : 0;
	}
	TestEqual(TEXT("One living Pabu"), Alive, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonCastMishaAndPabuTest, "ShinobiCoop.Summons.MishaAndPabuJutsuCallTheirKind", ShinobiTestFlags)
bool FShinobiSummonCastMishaAndPabuTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Misha cost 50"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 50.f);
	W.Tick(1.2f);
	TestTrue(TEXT("Misha cooldown"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonMisha));
	TestEqual(TEXT("Misha alone"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	const float Left = ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("Pabu costs 45 out of the ~52 regenerated so far (%.1f left)"), Left), Left > 3.f && Left < 12.f);
	W.Tick(1.2f);
	TestTrue(TEXT("Pabu cooldown"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonPabu));
	TestEqual(TEXT("Misha and Pabu"), ShinobiTest::CountActors<AShinobiSummon>(W), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonPageTest, "ShinobiCoop.Summons.OnlyCallableFromPageFour", ShinobiTestFlags)
bool FShinobiSummonPageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2); // other jutsu live on these keys here: they may run, the summons must not
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	W.Tick(1.5f);
	TestEqual(TEXT("No summon appeared"), ShinobiTest::CountActors<AShinobiSummon>(W) + ShinobiTest::CountActors<AShinobiOwl>(W), 0);
	TestFalse(TEXT("No summon cooldown started"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonOwl) || ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonMisha) || ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonPabu));
	TestFalse(TEXT("The owl not on page 2"), ShinobiTest::IsActive(P, UGA_SummonOwl::StaticClass()));
	TestFalse(TEXT("Misha not on page 2"), ShinobiTest::IsActive(P, UGA_SummonMisha::StaticClass()));
	TestFalse(TEXT("Pabu not on page 2"), ShinobiTest::IsActive(P, UGA_SummonPabu::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMishaBarkTest, "ShinobiCoop.Summons.MishaBarksAConeFromFarOff", ShinobiTestFlags)
bool FShinobiMishaBarkTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(2500.f, 0.f, 0.f));
	AShinobiRogueNinja* Ahead = W.Spawn<AShinobiRogueNinja>(FVector(550.f, 0.f, 0.f));
	AShinobiRogueNinja* Beside = W.Spawn<AShinobiRogueNinja>(FVector(600.f, 150.f, 0.f));
	AShinobiRogueNinja* Aside = W.Spawn<AShinobiRogueNinja>(FVector(0.f, 650.f, 0.f));
	for (AShinobiCharacterBase* Each : { (AShinobiCharacterBase*)Caster, (AShinobiCharacterBase*)Ahead, (AShinobiCharacterBase*)Beside, (AShinobiCharacterBase*)Aside })
	{
		ShinobiTest::Hover(Each);
	}
	for (AShinobiRogueNinja* Each : { Ahead, Beside, Aside })
	{
		ShinobiTest::ApplyEffect(Each, UGE_Shock::StaticClass()); // rooted for 1.5 s: only the bark can hurt them
	}
	Summon(Caster, EShinobiSummonKind::Misha, FVector::ZeroVector);

	bool bStaggered = false;
	for (int32 Step = 0; Step < 24; ++Step)
	{
		W.Tick(0.05f);
		bStaggered |= ShinobiTest::HasTag(Ahead, ShinobiTags::State_Staggered);
	}
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	TestEqual(TEXT("Barked at once for 70 from 550 away"), ShinobiTest::Get(Ahead, Health), 130.f);
	TestEqual(TEXT("The bark spreads: the one beside it is hit too"), ShinobiTest::Get(Beside, Health), 130.f);
	TestEqual(TEXT("Off to the side: untouched"), ShinobiTest::Get(Aside, Health), 200.f);
	TestTrue(TEXT("and it staggers them"), bStaggered);
	return true;
}

namespace
{
	UShinobiCreatureAnimInstance* CreatureAnim(const AShinobiSummon* Summon)
	{
		return Cast<UShinobiCreatureAnimInstance>(Summon->GetBodyMesh()->GetAnimInstance());
	}

	FString PlayingClip(const AShinobiSummon* Summon)
	{
		const UShinobiCreatureAnimInstance* Instance = CreatureAnim(Summon);
		return Instance && Instance->GetCurrentClip() ? Instance->GetCurrentClip()->GetName() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPabuLookTest, "ShinobiCoop.Summons.PabuWearsTheCorgiFacingForward", ShinobiTestFlags)
bool FShinobiPabuLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(400.f, 0.f, 0.f));
	if (Pack.Num() != 1)
	{
		return false;
	}
	const AShinobiSummon* Pabu = Pack[0];
	const USkeletalMeshComponent* Body = Pabu->GetBodyMesh();
	TestTrue(TEXT("The corgi is the visible body"), Body != Pabu->GetMesh());
	if (!TestNotNull(TEXT("with a mesh (run Scripts/setup_creatures.py)"), Body->GetSkeletalMeshAsset()))
	{
		return false;
	}
	TestEqual(TEXT("Pabu's model"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Pabu")));
	TestFalse(TEXT("The mannequin under it is hidden"), Pabu->GetMesh()->IsVisible());
	TestTrue(TEXT("It faces the way Pabu does (the model faces +X)"), FVector::DotProduct(Body->GetComponentTransform().TransformVectorNoScale(FVector(1.f, 0.f, 0.f)), Pabu->GetActorForwardVector()) > 0.99f);
	TestTrue(TEXT("The capsule is dog-sized, not a person's"), Pabu->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() < 70.f);
	const float FeetGap = (Body->Bounds.Origin.Z - Body->Bounds.BoxExtent.Z) - (Pabu->GetActorLocation().Z - Pabu->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	TestTrue(FString::Printf(TEXT("Paws on the capsule's floor (off by %.0f)"), FeetGap), FMath::Abs(FeetGap) < 20.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPabuAnimTest, "ShinobiCoop.Summons.PabuIdlesRunsAndBites", ShinobiTestFlags)
bool FShinobiPabuAnimTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(300.f, 0.f, 0.f));
	const AShinobiSummon* Pabu = Pack[0];
	W.Tick(3.f);
	TestTrue(FString::Printf(TEXT("Idle with nothing to fight (%s)"), *PlayingClip(Pabu)), PlayingClip(Pabu).Contains(TEXT("Idle")));

	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(1500.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	W.Tick(0.5f);
	TestEqual(TEXT("Runs at the ninja"), PlayingClip(Pabu), FString(TEXT("ANIM_Pabu_Run")));

	bool bBit = false;
	for (int32 Step = 0; Step < 40 && !bBit; ++Step)
	{
		W.Tick(0.05f);
		bBit = ShinobiTest::HasTag(Pabu, ShinobiTags::State_SummonAttacking);
	}
	TestTrue(FString::Printf(TEXT("Bites once it arrives (%.0f from the ninja)"), FVector::Dist2D(Pabu->GetActorLocation(), Ninja->GetActorLocation())), bBit);
	TestEqual(TEXT("with the bite clip"), PlayingClip(Pabu), FString(TEXT("ANIM_Pabu_Bite")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMishaLookTest, "ShinobiCoop.Summons.MishaIsTheGiantPlushThatHopsBarksAndFalls", ShinobiTestFlags)
bool FShinobiMishaLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(-500.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Misha, FVector::ZeroVector);
	if (Pack.Num() != 1)
	{
		return false;
	}
	AShinobiSummon* Misha = Pack[0];
	TestEqual(TEXT("Misha health"), ShinobiTest::Get(Misha, UShinobiAttributeSet::GetMaxHealthAttribute()), 500.f);
	const USkeletalMeshComponent* Body = Misha->GetBodyMesh();
	if (!TestTrue(TEXT("The plush is the visible body (run Scripts/setup_creatures.py if not)"), Body != Misha->GetMesh() && Body->GetSkeletalMeshAsset()))
	{
		return false;
	}
	TestEqual(TEXT("Misha's model"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Misha")));
	TestTrue(TEXT("It faces the way Misha does (the model faces +X)"), FVector::DotProduct(Body->GetComponentTransform().TransformVectorNoScale(FVector(1.f, 0.f, 0.f)), Misha->GetActorForwardVector()) > 0.99f);
	const FBoxSphereBounds Bounds = Body->Bounds;
	TestTrue(FString::Printf(TEXT("a giant plush: %.0f tall"), Bounds.BoxExtent.Z * 2.f), Bounds.BoxExtent.Z * 2.f > 300.f);
	const float SeatGap = (Bounds.Origin.Z - Bounds.BoxExtent.Z) - (Misha->GetActorLocation().Z - Misha->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	TestTrue(FString::Printf(TEXT("sitting on the capsule's floor (off by %.0f)"), SeatGap), FMath::Abs(SeatGap) < 20.f);
	W.Tick(4.f); // settled at his post
	TestEqual(TEXT("He idles"), PlayingClip(Misha), FString(TEXT("ANIM_Misha_Idle")));

	// Something to bark at, out of reach: he hops over, then barks.
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(Misha->GetActorLocation() + FVector(1500.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	W.Tick(0.4f);
	TestEqual(TEXT("He hops at the ninja"), PlayingClip(Misha), FString(TEXT("ANIM_Misha_Move")));
	bool bBarking = false;
	for (int32 Step = 0; Step < 60 && !bBarking; ++Step)
	{
		W.Tick(0.05f);
		bBarking = ShinobiTest::HasTag(Misha, ShinobiTags::State_SummonAttacking);
	}
	TestTrue(TEXT("He barks once in range"), bBarking);
	TestEqual(TEXT("with the bark clip"), PlayingClip(Misha), FString(TEXT("ANIM_Misha_Bark")));

	ShinobiTest::Damage(Misha, 999.f);
	W.Tick(0.2f);
	TestEqual(TEXT("and plays his death clip when he falls"), PlayingClip(Misha), FString(TEXT("ANIM_Misha_Death")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonAssetsTest, "ShinobiCoop.Summons.JutsuHaveSealAnimationsAndIcons", ShinobiTestFlags)
bool FShinobiSummonAssetsTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py and Scripts/setup_hud.py if an asset is missing)
	for (const UGA_SummonBase* Jutsu : { (const UGA_SummonBase*)GetDefault<UGA_SummonMisha>(), (const UGA_SummonBase*)GetDefault<UGA_SummonPabu>() })
	{
		const FString Name = Jutsu->DisplayName.ToString();
		TestNotNull(Name + TEXT(" seal montage"), Jutsu->SealMontage.LoadSynchronous());
		TestNotNull(Name + TEXT(" release montage"), Jutsu->ReleaseMontage.LoadSynchronous());
		TestNotNull(Name + TEXT(" icon"), Jutsu->Icon.LoadSynchronous());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonSmoothingTest, "ShinobiCoop.Summons.CreatureRidesTheSmoothedMesh", ShinobiTestFlags)
bool FShinobiSummonSmoothingTest::RunTest(const FString& Parameters)
{
	// The movement component's client smoothing moves GetMesh() only; a creature attached to the capsule would snap to every update.
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(400.f, 0.f, 0.f));
	if (Pack.Num() != 1)
	{
		return false;
	}
	TestTrue(TEXT("The creature is attached to the character's mesh"), Pack[0]->GetBodyMesh()->GetAttachParent() == Pack[0]->GetMesh());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonCasterGoneTest, "ShinobiCoop.Summons.LeaveWhenTheirCasterFalls", ShinobiTestFlags)
bool FShinobiSummonCasterGoneTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Downed = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Leaver = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 4000.f, 0.f));
	AShinobiPlayerCharacter* Steady = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, -4000.f, 0.f));
	ShinobiTest::Hover(Downed);
	ShinobiTest::Hover(Leaver);
	ShinobiTest::Hover(Steady);
	Summon(Downed, EShinobiSummonKind::Misha, FVector(300.f, 0.f, 0.f));
	Summon(Leaver, EShinobiSummonKind::Misha, FVector(300.f, 4000.f, 0.f));
	Summon(Steady, EShinobiSummonKind::Misha, FVector(300.f, -4000.f, 0.f));
	TestEqual(TEXT("Three Mishas"), ShinobiTest::CountActors<AShinobiSummon>(W), 3);

	ShinobiTest::Damage(Downed, 600.f); // downed
	Leaver->Destroy(); // left the game
	W.Tick(0.2f);
	TestEqual(TEXT("Only the standing caster's Misha remains"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonLeashTest, "ShinobiCoop.Summons.OnlyGoAfterEnemiesNearTheirCaster", ShinobiTestFlags)
bool FShinobiSummonLeashTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(-9000.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(-8700.f, 0.f, 0.f));
	AShinobiRogueNinja* Far = W.Spawn<AShinobiRogueNinja>(FVector(0.f, 0.f, 0.f));
	AShinobiRogueNinja* Near = W.Spawn<AShinobiRogueNinja>(FVector(-8000.f, 0.f, 0.f));
	ShinobiTest::Hover(Far);
	ShinobiTest::Hover(Near);
	ShinobiTest::ApplyEffect(Far, UGE_Shock::StaticClass());
	ShinobiTest::ApplyEffect(Near, UGE_Shock::StaticClass());

	W.Tick(2.f); // a lone Pabu starts 7 m off and bites 0.4 s after he arrives
	TestTrue(FString::Printf(TEXT("The ninja 1 km from the caster is attacked (Pabu %.0f from it)"), FVector::Dist2D(Pack[0]->GetActorLocation(), Near->GetActorLocation())), ShinobiTest::Get(Near, UShinobiAttributeSet::GetHealthAttribute()) < 200.f);
	TestEqual(TEXT("The one 9 km away is left alone"), ShinobiTest::Get(Far, UShinobiAttributeSet::GetHealthAttribute()), 200.f);
	TestTrue(TEXT("and Pabu did not run off towards it"), Pack[0]->GetActorLocation().X < -4000.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonWallTest, "ShinobiCoop.Summons.AppearInFrontOfAWallNotInsideIt", ShinobiTestFlags)
bool FShinobiSummonWallTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("engine cube"), Cube))
	{
		return false;
	}
	// A wall across the way 3 m ahead: its near face is at X = 300.
	AStaticMeshActor* Wall = W.World->SpawnActor<AStaticMeshActor>(FVector(350.f, 0.f, 0.f), FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Wall->SetActorScale3D(FVector(1.f, 30.f, 30.f));
	Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	Caster->ApplyJutsuPage(4);
	ShinobiTest::Tap(Caster, EShinobiAbilityInputID::Jutsu3);
	W.Tick(1.2f);
	AShinobiSummon* Misha = nullptr;
	for (TActorIterator<AShinobiSummon> It(W.World); It; ++It)
	{
		Misha = *It;
	}
	if (!TestNotNull(TEXT("Misha came"), Misha))
	{
		return false;
	}
	TestTrue(FString::Printf(TEXT("and stands clear of the wall, his 1.1 m radius short of X = 300 (X %.0f)"), Misha->GetActorLocation().X), Misha->GetActorLocation().X < 300.f - 110.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonFollowTest, "ShinobiCoop.Summons.FollowTheirCasterWhenNothingIsToFight", ShinobiTestFlags)
bool FShinobiSummonFollowTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Misha = Summon(Caster, EShinobiSummonKind::Misha, FVector(400.f, 0.f, 0.f));
	const TArray<AShinobiSummon*> Pabu = Summon(Caster, EShinobiSummonKind::Pabu, FVector(-400.f, 0.f, 0.f));
	if (Misha.Num() != 1 || Pabu.Num() != 1)
	{
		return false;
	}

	Caster->SetActorLocation(FVector(1500.f, 800.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	float MishaTop = 0.f;
	for (int32 Step = 0; Step < 20; ++Step)
	{
		W.Tick(0.25f);
		MishaTop = FMath::Max(MishaTop, Misha[0]->GetVelocity().Size2D());
	}
	TestTrue(FString::Printf(TEXT("Misha hurried, faster than his usual 380 (%.0f)"), MishaTop), MishaTop > 500.f);
	const float MishaGap = FVector::Dist2D(Misha[0]->GetActorLocation(), Caster->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Misha caught up (%.0f from the caster)"), MishaGap), MishaGap < AShinobiSummon::FollowDistance + 300.f);
	const float PabuGap = FVector::Dist2D(Pabu[0]->GetActorLocation(), Caster->GetActorLocation());
	TestTrue(FString::Printf(TEXT("and so did Pabu (%.0f)"), PabuGap), PabuGap < AShinobiSummon::FollowDistance + 300.f);
	TestTrue(TEXT("They wait in different spots, not stacked"), FVector::Dist2D(Pabu[0]->GetActorLocation(), Misha[0]->GetActorLocation()) > 150.f);

	// Arrived: standing, in the idle clip, not shuffling about.
	W.Tick(2.f);
	TestTrue(TEXT("Misha has settled"), Misha[0]->GetVelocity().Size2D() < 30.f);
	TestTrue(FString::Printf(TEXT("and idles (%s)"), *PlayingClip(Misha[0])), PlayingClip(Misha[0]).Contains(TEXT("Idle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonCatchUpTest, "ShinobiCoop.Summons.JumpToTheirCasterWhenLeftFarBehind", ShinobiTestFlags)
bool FShinobiSummonCatchUpTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Misha = Summon(Caster, EShinobiSummonKind::Misha, FVector(500.f, 0.f, 0.f));
	if (Misha.Num() != 1)
	{
		return false;
	}
	Caster->SetActorLocation(FVector(9000.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics); // a teleport jutsu, a dragon ride
	W.Tick(0.5f);
	const float Gap = FVector::Dist2D(Misha[0]->GetActorLocation(), Caster->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Misha is back with his caster at once (%.0f away)"), Gap), Gap < AShinobiSummon::FollowDistance + 400.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonCrossfadeTest, "ShinobiCoop.Summons.AnimationsBlendInsteadOfCutting", ShinobiTestFlags)
bool FShinobiSummonCrossfadeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(300.f, 0.f, 0.f));
	const AShinobiSummon* Pabu = Pack[0];
	W.Tick(3.f); // settled at its post beside the caster
	UShinobiCreatureAnimInstance* Anim = CreatureAnim(Pabu);
	if (!TestNotNull(TEXT("The creature has its animation instance"), Anim))
	{
		return false;
	}
	TestEqual(TEXT("Settled in the idle: one clip playing"), Anim->GetLayerCount(), 1);

	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(1200.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	bool bBlending = false;
	for (int32 Step = 0; Step < 20 && !bBlending; ++Step)
	{
		W.Tick(0.05f);
		bBlending = Anim->GetLayerCount() == 2;
	}
	TestTrue(TEXT("Setting off blends the idle out and the walk or run in, two clips at once"), bBlending);
	W.Tick(0.05f); // one frame in, the new clip has its first share
	float Sum = 0.f;
	for (float Weight : Anim->GetLayerWeights())
	{
		TestTrue(TEXT("each with a share of the pose"), Weight > 0.f);
		Sum += Weight;
	}
	TestTrue(FString::Printf(TEXT("and the shares make a whole pose (%.2f)"), Sum), FMath::IsNearlyEqual(Sum, 1.f, 0.05f));
	W.Tick(0.4f);
	TestEqual(TEXT("A moment later the old clip is gone"), Anim->GetLayerCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonStrideTest, "ShinobiCoop.Summons.StrideMatchesSpeed", ShinobiTestFlags)
bool FShinobiSummonStrideTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(300.f, 0.f, 0.f));
	const AShinobiSummon* Pabu = Pack[0];
	W.Tick(3.f);
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(2600.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	W.Tick(0.7f);

	const UShinobiCreatureAnimInstance* Anim = CreatureAnim(Pabu);
	const float Speed = Pabu->GetVelocity().Size2D();
	TestEqual(TEXT("At speed it runs"), PlayingClip(Pabu), FString(TEXT("ANIM_Pabu_Run")));
	// Pabu's gallop covers 480 cm a second at play rate 1; he covers Speed, so the clip is sped up by the ratio.
	const float Expected = Speed / 480.f;
	TestTrue(FString::Printf(TEXT("and the clip is played at the speed it covers ground (rate %.2f, speed %.0f, wanted %.2f)"), Anim->GetCurrentRate(), Speed, Expected), FMath::Abs(Anim->GetCurrentRate() - Expected) < 0.12f);
	TestTrue(TEXT("but never absurdly fast"), Anim->GetCurrentRate() < 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonPoseTest, "ShinobiCoop.Summons.TheBodyReallyMoves", ShinobiTestFlags)
bool FShinobiSummonPoseTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(300.f, 0.f, 0.f));
	const AShinobiSummon* Pabu = Pack[0];
	W.Tick(3.f);
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(2800.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	W.Tick(0.5f);

	// A running paw goes up and down: sample its height over a second.
	const USkeletalMeshComponent* Body = Pabu->GetBodyMesh();
	float Lowest = FLT_MAX, Highest = -FLT_MAX;
	for (int32 Step = 0; Step < 20; ++Step)
	{
		W.Tick(0.05f);
		const float Height = Body->GetBoneLocation(TEXT("front_foot_L"), EBoneSpaces::ComponentSpace).Z;
		Lowest = FMath::Min(Lowest, Height);
		Highest = FMath::Max(Highest, Height);
	}
	TestTrue(FString::Printf(TEXT("A paw lifts and falls as it runs (%.1f to %.1f)"), Lowest, Highest), Highest - Lowest > 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonHitReactTest, "ShinobiCoop.Summons.FlinchWhenHurtThenCarryOn", ShinobiTestFlags)
bool FShinobiSummonHitReactTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Misha, FVector(400.f, 0.f, 0.f));
	const AShinobiSummon* Misha = Pack[0];
	W.Tick(4.f);
	TestTrue(TEXT("Idle first"), PlayingClip(Misha).Contains(TEXT("Idle")));

	ShinobiTest::Damage(const_cast<AShinobiSummon*>(Misha), 40.f);
	W.Tick(0.15f);
	TestEqual(TEXT("A blow makes it flinch"), PlayingClip(Misha), FString(TEXT("ANIM_Misha_Hit")));
	W.Tick(1.2f);
	TestTrue(FString::Printf(TEXT("and then it carries on (%s)"), *PlayingClip(Misha)), PlayingClip(Misha).Contains(TEXT("Idle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonAttackLengthTest, "ShinobiCoop.Summons.AttackClipSpansTheWholeAttack", ShinobiTestFlags)
bool FShinobiSummonAttackLengthTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(300.f, 0.f, 0.f));
	const AShinobiSummon* Pabu = Pack[0];
	W.Tick(3.f);
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(Pabu->GetActorLocation() + FVector(160.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	bool bBiting = false;
	for (int32 Step = 0; Step < 20 && !bBiting; ++Step)
	{
		W.Tick(0.05f);
		bBiting = ShinobiTest::HasTag(Pabu, ShinobiTags::State_SummonAttacking);
	}
	if (!TestTrue(TEXT("It bites"), bBiting))
	{
		return false;
	}
	W.Tick(0.1f);
	const UShinobiCreatureAnimInstance* Anim = CreatureAnim(Pabu);
	// The bite is 0.73 s long and the attack (wind-up and recovery) 0.75 s: the clip is stretched over the lot.
	TestEqual(TEXT("with the bite clip"), PlayingClip(Pabu), FString(TEXT("ANIM_Pabu_Bite")));
	TestTrue(FString::Printf(TEXT("slowed to fit the attack (rate %.2f)"), Anim->GetCurrentRate()), Anim->GetCurrentRate() > 0.93f && Anim->GetCurrentRate() < 0.995f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonIdleCatchUpTest, "ShinobiCoop.Summons.JumpToTheirCasterWhenBlockedFarOff", ShinobiTestFlags)
bool FShinobiSummonIdleCatchUpTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Misha = Summon(Caster, EShinobiSummonKind::Misha, FVector(400.f, 0.f, 0.f));
	if (Misha.Num() != 1)
	{
		return false;
	}
	Caster->SetActorLocation(FVector(2400.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics); // not far enough to jump if it were fighting
	W.Tick(0.5f);
	const float Gap = FVector::Dist2D(Misha[0]->GetActorLocation(), Caster->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Idle and 24 m off, Misha is back beside his caster (%.0f away)"), Gap), Gap < AShinobiSummon::FollowDistance + 400.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonFightsFirstTest, "ShinobiCoop.Summons.FightAnEnemyInsteadOfFollowing", ShinobiTestFlags)
bool FShinobiSummonFightsFirstTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(2500.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pabu, FVector(1500.f, 0.f, 0.f));
	W.Tick(1.2f);
	TestTrue(TEXT("With an enemy in reach it goes for it, not back to the caster"), Pack[0]->GetActorLocation().X > 1500.f);
	return true;
}

namespace
{
	/** The owl Caster has out, if any. */
	AShinobiOwl* OwlOf(const FShinobiTestWorld& W, const AShinobiCharacterBase* Caster)
	{
		for (TActorIterator<AShinobiOwl> It(W.World); It; ++It)
		{
			if (It->GetSummoner() == Caster)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** A foe that stays where it is put (no AI), on the enemies' side. */
	AShinobiCharacterBase* Foe(FShinobiTestWorld& W, const FVector& Where)
	{
		AShinobiCharacterBase* Foe = W.Spawn<AShinobiCharacterBase>(Where);
		ShinobiTest::Hover(Foe);
		return Foe;
	}

	FString OwlClip(const AShinobiOwl* Owl)
	{
		const UAnimSingleNodeInstance* Single = Owl && Owl->GetBody() ? Owl->GetBody()->GetSingleNodeInstance() : nullptr;
		return Single && Single->GetAnimationAsset() ? Single->GetAnimationAsset()->GetName() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlCastTest, "ShinobiCoop.Summons.OwlJutsuCostsChakraThenCallsTheOwl", ShinobiTestFlags)
bool FShinobiOwlCastTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	TestEqual(TEXT("Cost 60 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 40.f);
	TestEqual(TEXT("No owl during the seals"), ShinobiTest::CountActors<AShinobiOwl>(W), 0);
	W.Tick(1.2f);
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonOwl));
	TestFalse(TEXT("The jutsu itself is over"), ShinobiTest::IsActive(P, UGA_SummonOwl::StaticClass()));
	TestEqual(TEXT("One owl"), ShinobiTest::CountActors<AShinobiOwl>(W), 1);
	TestEqual(TEXT("and no ground summon"), ShinobiTest::CountActors<AShinobiSummon>(W), 0);
	TestNotNull(TEXT("She is the caster's"), OwlOf(W, P));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlReplaceTest, "ShinobiCoop.Summons.OwlReplacesHerselfOnly", ShinobiTestFlags)
bool FShinobiOwlReplaceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Other = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 3000.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Other);
	Summon(Caster, EShinobiSummonKind::Misha, FVector(400.f, 0.f, 0.f));
	Summon(Caster, EShinobiSummonKind::Pabu, FVector(-400.f, 0.f, 0.f));
	AShinobiOwl::Summon(Caster);
	W.Tick(0.1f);
	AShinobiOwl::Summon(Caster);
	TestEqual(TEXT("The second owl replaced the first"), ShinobiTest::CountActors<AShinobiOwl>(W), 1);
	TestEqual(TEXT("Misha and Pabu stay"), ShinobiTest::CountActors<AShinobiSummon>(W), 2);
	AShinobiOwl::Summon(Other);
	TestEqual(TEXT("Another caster's owl joins her"), ShinobiTest::CountActors<AShinobiOwl>(W), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlPostTest, "ShinobiCoop.Summons.OwlHoversAtTheLeftShoulder", ShinobiTestFlags)
bool FShinobiOwlPostTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const float Feet = -Caster->GetSimpleCollisionHalfHeight();
	const FVector Post = AShinobiOwl::GetPost(*Caster);
	TestTrue(FString::Printf(TEXT("The post is off the left shoulder (Y %.0f)"), Post.Y), Post.Y >= -70.f && Post.Y <= -50.f);
	TestTrue(FString::Printf(TEXT("a little behind (X %.0f)"), Post.X), Post.X >= -40.f && Post.X <= -20.f);
	TestTrue(FString::Printf(TEXT("at shoulder height (%.0f above the feet)"), Post.Z - Feet), FMath::Abs(Post.Z - Feet - 160.f) <= 5.f);

	AShinobiOwl* Owl = AShinobiOwl::Summon(Caster);
	if (!TestNotNull(TEXT("The owl came"), Owl))
	{
		return false;
	}
	W.Tick(1.f);
	TestTrue(TEXT("She hovers there"), FVector::Dist(Owl->GetActorLocation(), AShinobiOwl::GetPost(*Caster)) <= 20.f);

	Caster->SetActorLocationAndRotation(FVector(800.f, 0.f, 0.f), FRotator(0.f, 90.f, 0.f));
	W.Tick(1.f);
	const FVector Turned = AShinobiOwl::GetPost(*Caster);
	TestTrue(FString::Printf(TEXT("Facing +Y, the left shoulder is towards +X (X %.0f)"), Turned.X), Turned.X > 840.f && Turned.X < 880.f);
	TestTrue(TEXT("and she follows the caster there"), FVector::Dist(Owl->GetActorLocation(), Turned) <= 20.f);

	Caster->SetActorLocation(FVector(6000.f, 0.f, 0.f));
	W.Tick(0.05f);
	TestTrue(TEXT("Left far behind (a teleport), she snaps back to the shoulder"), FVector::Dist(Owl->GetActorLocation(), AShinobiOwl::GetPost(*Caster)) <= 20.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlBodyTest, "ShinobiCoop.Summons.OwlHasNoBodyToHit", ShinobiTestFlags)
bool FShinobiOwlBodyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	AShinobiOwl* Owl = AShinobiOwl::Summon(Caster);
	if (!TestNotNull(TEXT("The owl came"), Owl))
	{
		return false;
	}
	TestFalse(TEXT("She collides with nothing"), Owl->GetActorEnableCollision() && Owl->GetBody()->IsCollisionEnabled());
	AShinobiCharacterBase* Enemy = Foe(W, Owl->GetActorLocation() + FVector(200.f, 0.f, 0.f));
	const TArray<AShinobiCharacterBase*> Around = ShinobiCombat::FindHostilesInRadius(Enemy, Owl->GetActorLocation(), 500.f);
	TestFalse(TEXT("An enemy's area attack finds only the caster, never her"), Around.ContainsByPredicate([Owl](const AShinobiCharacterBase* C) { return static_cast<const AActor*>(C) == Owl; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlStrikeTest, "ShinobiCoop.Summons.OwlStrikesTheNearestEnemyInSight", ShinobiTestFlags)
bool FShinobiOwlStrikeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	AShinobiCharacterBase* Near = Foe(W, FVector(800.f, 0.f, 0.f));
	AShinobiCharacterBase* Far = Foe(W, FVector(1500.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(Near, ShinobiTags::Event_Hit_Stagger);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const float Full = ShinobiTest::Get(Near, Health);
	AShinobiOwl::Summon(Caster);

	W.Tick(1.f); // her first strike starts 0.5 s in and lands 0.4 s later
	TestEqual(TEXT("The nearer foe lost 55"), ShinobiTest::Get(Near, Health), Full - 55.f);
	TestEqual(TEXT("The farther one nothing"), ShinobiTest::Get(Far, Health), Full);
	TestEqual(TEXT("Her lightning does not stagger"), Staggers.Count, 0);
	W.Tick(2.2f); // the world steps 50 ms at a time, so each timer fires up to a step late
	TestEqual(TEXT("Two seconds later, a second bolt"), ShinobiTest::Get(Near, Health), Full - 110.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlSightTest, "ShinobiCoop.Summons.OwlIgnoresEnemiesOutOfRangeOrBehindWalls", ShinobiTestFlags)
bool FShinobiOwlSightTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("engine cube"), Cube))
	{
		return false;
	}
	// A wall across +Y, 4 m out: it hides the foe 8 m out on that side.
	AStaticMeshActor* Wall = W.World->SpawnActor<AStaticMeshActor>(FVector(0.f, 400.f, 0.f), FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Wall->SetActorScale3D(FVector(30.f, 1.f, 30.f));
	Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	AShinobiCharacterBase* Hidden = Foe(W, FVector(0.f, 800.f, 0.f));
	AShinobiCharacterBase* Distant = Foe(W, FVector(-2500.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const float Full = ShinobiTest::Get(Hidden, Health);
	AShinobiOwl::Summon(Caster);
	W.Tick(3.f);
	TestEqual(TEXT("Not through a wall"), ShinobiTest::Get(Hidden, Health), Full);
	TestEqual(TEXT("Not beyond 20 m"), ShinobiTest::Get(Distant, Health), Full);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlFallenTargetTest, "ShinobiCoop.Summons.OwlBoltSparesATargetThatFellMeanwhile", ShinobiTestFlags)
bool FShinobiOwlFallenTargetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	AShinobiCharacterBase* Target = Foe(W, FVector(800.f, 0.f, 0.f));
	AShinobiOwl* Owl = AShinobiOwl::Summon(Caster);
	W.Tick(0.6f); // the strike has started
	TestTrue(TEXT("She has picked her target"), Owl && Owl->GetTarget() == Target);
	ShinobiTest::ApplyEffect(Target, UGE_Dead::StaticClass());
	TestFalse(TEXT("It has fallen"), Target->IsAlive());
	W.Tick(0.5f);
	int32 Bolts = 0;
	for (TActorIterator<AShinobiBurst> It(W.World); It; ++It)
	{
		Bolts += It->GetKind() != EShinobiBurst::SummonPoof;
	}
	TestEqual(TEXT("and no bolt falls on it"), Bolts, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlLifetimeTest, "ShinobiCoop.Summons.OwlLeavesWhenHerTimeIsUp", ShinobiTestFlags)
bool FShinobiOwlLifetimeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	AShinobiOwl::Summon(Caster);
	W.Tick(AShinobiOwl::Lifetime - 0.5f);
	TestEqual(TEXT("Still here before her time"), ShinobiTest::CountActors<AShinobiOwl>(W), 1);
	W.Tick(1.f);
	TestEqual(TEXT("Gone after her 15 s"), ShinobiTest::CountActors<AShinobiOwl>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlCasterFallsTest, "ShinobiCoop.Summons.OwlLeavesWhenHerCasterFalls", ShinobiTestFlags)
bool FShinobiOwlCasterFallsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Leaver = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 4000.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Leaver);
	AShinobiCharacterBase* Target = Foe(W, FVector(800.f, 0.f, 0.f));
	const float Full = ShinobiTest::Get(Target, UShinobiAttributeSet::GetHealthAttribute());
	AShinobiOwl::Summon(Caster);
	AShinobiOwl::Summon(Leaver);
	W.Tick(0.6f); // a strike under way
	ShinobiTest::Damage(Caster, 600.f); // downed
	Leaver->Destroy(); // left the game
	W.Tick(0.5f);
	TestEqual(TEXT("Both owls are gone"), ShinobiTest::CountActors<AShinobiOwl>(W), 0);
	TestEqual(TEXT("and the bolt under way never landed"), ShinobiTest::Get(Target, UShinobiAttributeSet::GetHealthAttribute()), Full);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOwlLookTest, "ShinobiCoop.Summons.OwlWearsThePlushAndHovers", ShinobiTestFlags)
bool FShinobiOwlLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	AShinobiOwl* Owl = AShinobiOwl::Summon(Caster);
	if (!TestNotNull(TEXT("The owl came"), Owl))
	{
		return false;
	}
	const USkeletalMeshComponent* Body = Owl->GetBody();
	if (!TestTrue(TEXT("She wears the plush (run Scripts/setup_creatures.py if not)"), Body && Body->GetSkeletalMeshAsset()))
	{
		return false;
	}
	TestEqual(TEXT("the owl"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Owl")));
	TestTrue(TEXT("at plush size"), Body->GetComponentScale().Equals(FVector(AShinobiOwl::Scale)));
	W.Tick(0.1f);
	TestEqual(TEXT("She hovers"), OwlClip(Owl), FString(TEXT("ANIM_Owl_Hover")));
	Foe(W, FVector(800.f, 0.f, 0.f));
	W.Tick(0.5f); // her strike starts at 0.5 s
	TestEqual(TEXT("She strikes"), OwlClip(Owl), FString(TEXT("ANIM_Owl_Strike")));
	W.Tick(1.f);
	TestEqual(TEXT("then hovers again"), OwlClip(Owl), FString(TEXT("ANIM_Owl_Hover")));
	return true;
}

#endif
