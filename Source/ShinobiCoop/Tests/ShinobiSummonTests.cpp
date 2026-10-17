#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
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

	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	TestEqual(TEXT("One toad"), Pack.Num(), 1);
	if (Pack.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("On the players' side"), Pack[0]->GetTeam(), EShinobiTeam::Players);
	TestTrue(TEXT("Hostile to enemies"), Pack[0]->IsHostileTo(Ninja) && Ninja->IsHostileTo(Pack[0]));
	TestFalse(TEXT("Not hostile to the caster"), Pack[0]->IsHostileTo(Caster) || Caster->IsHostileTo(Pack[0]));
	TestTrue(TEXT("Remembers its summoner"), Pack[0]->GetSummoner() == Caster);
	TestEqual(TEXT("Toad health"), ShinobiTest::Get(Pack[0], UShinobiAttributeSet::GetMaxHealthAttribute()), 600.f);

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
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector(400.f, 0.f, 0.f));
	if (Pack.Num() != 1)
	{
		return false;
	}

	W.Tick(0.6f);
	TestTrue(TEXT("The ninja heads for the toad, not the far-off caster"), Ninja->GetActorLocation().X > 100.f);
	W.Tick(3.f);
	TestTrue(TEXT("The toad hurt the ninja"), ShinobiTest::Get(Ninja, UShinobiAttributeSet::GetHealthAttribute()) < 200.f);
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

	Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	W.Tick(0.1f);
	Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 300.f, 0.f));
	TestEqual(TEXT("The second toad replaced the first"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);

	Summon(Caster, EShinobiSummonKind::Misha, FVector(-500.f, 0.f, 0.f));
	TestEqual(TEXT("Misha joins the toad"), ShinobiTest::CountActors<AShinobiSummon>(W), 2);
	Summon(Other, EShinobiSummonKind::Toad, FVector(0.f, 3300.f, 0.f));
	TestEqual(TEXT("Another caster's toad is not replaced"), ShinobiTest::CountActors<AShinobiSummon>(W), 3);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonCastToadTest, "ShinobiCoop.Summons.ToadJutsuCostsChakraThenCallsTheToad", ShinobiTestFlags)
bool FShinobiSummonCastToadTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	TestEqual(TEXT("Cost 60 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 40.f);
	TestEqual(TEXT("No toad during the seals"), ShinobiTest::CountActors<AShinobiSummon>(W), 0);
	W.Tick(1.2f);
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonToad));
	TestFalse(TEXT("The jutsu itself is over"), ShinobiTest::IsActive(P, UGA_SummonToad::StaticClass()));

	AShinobiSummon* Toad = nullptr;
	for (TActorIterator<AShinobiSummon> It(W.World); It; ++It)
	{
		Toad = *It;
	}
	if (!TestNotNull(TEXT("The toad is here"), Toad))
	{
		return false;
	}
	TestEqual(TEXT("It is a toad"), Toad->GetKind(), EShinobiSummonKind::Toad);
	TestTrue(TEXT("It is the caster's"), Toad->GetSummoner() == P);
	TestTrue(FString::Printf(TEXT("and stands in front of the caster (X %.0f)"), Toad->GetActorLocation().X), Toad->GetActorLocation().X > 200.f && Toad->GetActorLocation().X < 600.f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonCastPushoTest, "ShinobiCoop.Summons.PushoJutsuIsPageFourSlotOne", ShinobiTestFlags)
bool FShinobiSummonCastPushoTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestEqual(TEXT("Pusho costs 45"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 55.f);
	W.Tick(1.2f);
	TestTrue(TEXT("Pusho cooldown"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonPusho));
	TArray<AShinobiSummon*> Called;
	for (TActorIterator<AShinobiSummon> It(W.World); It; ++It)
	{
		Called.Add(*It);
	}
	TestTrue(TEXT("Pusho alone, of his kind"), Called.Num() == 1 && Called[0]->GetKind() == EShinobiSummonKind::Pusho);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonPageTest, "ShinobiCoop.Summons.OnlyCallableFromPageFour", ShinobiTestFlags)
bool FShinobiSummonPageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1); // other jutsu live on these keys here: they may run, the summons must not
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	W.Tick(1.5f);
	TestEqual(TEXT("No summon appeared"), ShinobiTest::CountActors<AShinobiSummon>(W), 0);
	TestFalse(TEXT("No summon cooldown started"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonToad) || ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonMisha)
		|| ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonPabu) || ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonPusho));
	TestFalse(TEXT("Pusho not on page 2"), ShinobiTest::IsActive(P, UGA_SummonPusho::StaticClass()));
	TestFalse(TEXT("Toad not on page 2"), ShinobiTest::IsActive(P, UGA_SummonToad::StaticClass()));
	TestFalse(TEXT("Misha not on page 2"), ShinobiTest::IsActive(P, UGA_SummonMisha::StaticClass()));
	TestFalse(TEXT("Pabu not on page 2"), ShinobiTest::IsActive(P, UGA_SummonPabu::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiToadSlamTest, "ShinobiCoop.Summons.ToadSlamHitsEveryoneAroundIt", ShinobiTestFlags)
bool FShinobiToadSlamTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(2500.f, 0.f, 0.f));
	AShinobiRogueNinja* InFront = W.Spawn<AShinobiRogueNinja>(FVector(350.f, 0.f, 0.f));
	AShinobiRogueNinja* Behind = W.Spawn<AShinobiRogueNinja>(FVector(-300.f, 0.f, 0.f));
	AShinobiRogueNinja* Far = W.Spawn<AShinobiRogueNinja>(FVector(0.f, 1500.f, 0.f));
	for (AShinobiCharacterBase* Each : { (AShinobiCharacterBase*)Caster, (AShinobiCharacterBase*)InFront, (AShinobiCharacterBase*)Behind, (AShinobiCharacterBase*)Far })
	{
		ShinobiTest::Hover(Each);
	}
	// Held still, so only the slam can have hurt them.
	for (AShinobiRogueNinja* Each : { InFront, Behind, Far })
	{
		ShinobiTest::ApplyEffect(Each, UGE_Shock::StaticClass());
	}
	Summon(Caster, EShinobiSummonKind::Toad, FVector::ZeroVector);

	W.Tick(1.2f);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	TestEqual(TEXT("In front: one slam of 60"), ShinobiTest::Get(InFront, Health), 140.f);
	TestEqual(TEXT("Behind: the slam is all around"), ShinobiTest::Get(Behind, Health), 140.f);
	TestEqual(TEXT("Out of reach: untouched"), ShinobiTest::Get(Far, Health), 200.f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiToadLookTest, "ShinobiCoop.Summons.ToadIsTheRiggedToadThatHopsAndSlams", ShinobiTestFlags)
bool FShinobiToadLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector(500.f, 0.f, 0.f));
	if (Pack.Num() != 1)
	{
		return false;
	}
	AShinobiSummon* Toad = Pack[0];
	const USkeletalMeshComponent* Body = Toad->GetBodyMesh();
	if (!TestTrue(TEXT("The toad model is the visible body (run Scripts/setup_creatures.py if not)"), Body != Toad->GetMesh() && Body->GetSkeletalMeshAsset()))
	{
		return false;
	}
	TestEqual(TEXT("the toad"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Toad")));
	const FBoxSphereBounds Bounds = Body->Bounds;
	TestTrue(FString::Printf(TEXT("a giant: %.0f long, %.0f tall"), Bounds.BoxExtent.X * 2.f, Bounds.BoxExtent.Z * 2.f), Bounds.BoxExtent.X * 2.f > 400.f && Bounds.BoxExtent.Z * 2.f > 300.f);
	const float FeetGap = (Bounds.Origin.Z - Bounds.BoxExtent.Z) - (Toad->GetActorLocation().Z - Toad->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	TestTrue(FString::Printf(TEXT("standing on the capsule's floor (off by %.0f)"), FeetGap), FMath::Abs(FeetGap) < 20.f);
	W.Tick(4.f); // settled at its post
	TestEqual(TEXT("It idles"), PlayingClip(Toad), FString(TEXT("ANIM_Toad_Idle")));

	// Something to slam: held still next to it.
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(Toad->GetActorLocation() + FVector(300.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	bool bSlamming = false;
	for (int32 Step = 0; Step < 30 && !bSlamming; ++Step)
	{
		W.Tick(0.05f);
		bSlamming = ShinobiTest::HasTag(Toad, ShinobiTags::State_SummonAttacking);
	}
	TestTrue(TEXT("It rears up to slam"), bSlamming);
	TestEqual(TEXT("with the slam clip"), PlayingClip(Toad), FString(TEXT("ANIM_Toad_Slam")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonAssetsTest, "ShinobiCoop.Summons.JutsuHaveSealAnimationsAndIcons", ShinobiTestFlags)
bool FShinobiSummonAssetsTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py and Scripts/setup_hud.py if an asset is missing)
	for (const UGA_SummonBase* Jutsu : { (const UGA_SummonBase*)GetDefault<UGA_SummonToad>(), (const UGA_SummonBase*)GetDefault<UGA_SummonMisha>(), (const UGA_SummonBase*)GetDefault<UGA_SummonPabu>(),
		(const UGA_SummonBase*)GetDefault<UGA_SummonPusho>() })
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
	Summon(Downed, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	Summon(Leaver, EShinobiSummonKind::Toad, FVector(300.f, 4000.f, 0.f));
	Summon(Steady, EShinobiSummonKind::Toad, FVector(300.f, -4000.f, 0.f));
	TestEqual(TEXT("Three toads"), ShinobiTest::CountActors<AShinobiSummon>(W), 3);

	ShinobiTest::Damage(Downed, 600.f); // downed
	Leaver->Destroy(); // left the game
	W.Tick(0.2f);
	TestEqual(TEXT("Only the standing caster's toad remains"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);
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
	ShinobiTest::Tap(Caster, EShinobiAbilityInputID::Jutsu2);
	W.Tick(1.2f);
	AShinobiSummon* Toad = nullptr;
	for (TActorIterator<AShinobiSummon> It(W.World); It; ++It)
	{
		Toad = *It;
	}
	if (!TestNotNull(TEXT("The toad came"), Toad))
	{
		return false;
	}
	TestTrue(FString::Printf(TEXT("and stands clear of the wall, its 1.7 m radius short of X = 300 (X %.0f)"), Toad->GetActorLocation().X), Toad->GetActorLocation().X < 300.f - 150.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonFollowTest, "ShinobiCoop.Summons.FollowTheirCasterWhenNothingIsToFight", ShinobiTestFlags)
bool FShinobiSummonFollowTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Toad = Summon(Caster, EShinobiSummonKind::Toad, FVector(400.f, 0.f, 0.f));
	const TArray<AShinobiSummon*> Pabu = Summon(Caster, EShinobiSummonKind::Pabu, FVector(-400.f, 0.f, 0.f));
	if (Toad.Num() != 1 || Pabu.Num() != 1)
	{
		return false;
	}

	Caster->SetActorLocation(FVector(1500.f, 800.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	float ToadTop = 0.f;
	for (int32 Step = 0; Step < 20; ++Step)
	{
		W.Tick(0.25f);
		ToadTop = FMath::Max(ToadTop, Toad[0]->GetVelocity().Size2D());
	}
	TestTrue(FString::Printf(TEXT("The toad hurried, faster than its usual 350 (%.0f)"), ToadTop), ToadTop > 500.f);
	const float ToadGap = FVector::Dist2D(Toad[0]->GetActorLocation(), Caster->GetActorLocation());
	TestTrue(FString::Printf(TEXT("The toad caught up (%.0f from the caster)"), ToadGap), ToadGap < AShinobiSummon::FollowDistance + 300.f);
	const float PabuGap = FVector::Dist2D(Pabu[0]->GetActorLocation(), Caster->GetActorLocation());
	TestTrue(FString::Printf(TEXT("and so did Pabu (%.0f)"), PabuGap), PabuGap < AShinobiSummon::FollowDistance + 300.f);
	TestTrue(TEXT("They wait in different spots, not stacked"), FVector::Dist2D(Pabu[0]->GetActorLocation(), Toad[0]->GetActorLocation()) > 150.f);

	// Arrived: standing, in the idle clip, not shuffling about.
	W.Tick(2.f);
	TestTrue(TEXT("The toad has settled"), Toad[0]->GetVelocity().Size2D() < 30.f);
	TestTrue(FString::Printf(TEXT("and idles (%s)"), *PlayingClip(Toad[0])), PlayingClip(Toad[0]).Contains(TEXT("Idle")));
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
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector(400.f, 0.f, 0.f));
	const AShinobiSummon* Toad = Pack[0];
	W.Tick(4.f);
	TestTrue(TEXT("Idle first"), PlayingClip(Toad).Contains(TEXT("Idle")));

	ShinobiTest::Damage(const_cast<AShinobiSummon*>(Toad), 40.f);
	W.Tick(0.15f);
	TestEqual(TEXT("A blow makes it flinch"), PlayingClip(Toad), FString(TEXT("ANIM_Toad_Hit")));
	W.Tick(1.2f);
	TestTrue(FString::Printf(TEXT("and then it carries on (%s)"), *PlayingClip(Toad)), PlayingClip(Toad).Contains(TEXT("Idle")));
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
	const TArray<AShinobiSummon*> Toad = Summon(Caster, EShinobiSummonKind::Toad, FVector(400.f, 0.f, 0.f));
	if (Toad.Num() != 1)
	{
		return false;
	}
	Caster->SetActorLocation(FVector(2400.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics); // not far enough to jump if it were fighting
	W.Tick(0.5f);
	const float Gap = FVector::Dist2D(Toad[0]->GetActorLocation(), Caster->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Idle and 24 m off, the toad is back beside its caster (%.0f away)"), Gap), Gap < AShinobiSummon::FollowDistance + 400.f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPushoPounceTest, "ShinobiCoop.Summons.PushoPouncesFromAfarAndClaws", ShinobiTestFlags)
bool FShinobiPushoPounceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("engine cube"), Cube))
	{
		return false;
	}
	// A floor (top at Z = 0): the leap is a real jump under gravity.
	AStaticMeshActor* Floor = W.World->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator);
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Floor->SetActorScale3D(FVector(100.f, 100.f, 1.f));
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(-2000.f, 0.f, 100.f));
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(600.f, 0.f, 100.f));
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass()); // rooted: only the pounce can reach it
	const TArray<AShinobiSummon*> Pack = AShinobiSummon::Summon(Caster, EShinobiSummonKind::Pusho, FVector::ZeroVector);
	if (Pack.Num() != 1)
	{
		return false;
	}
	const AShinobiSummon* Pusho = Pack[0];
	const float StartZ = Pusho->GetActorLocation().Z;
	const float StartGap = FVector::Dist2D(Pusho->GetActorLocation(), Ninja->GetActorLocation());
	float Highest = StartZ;
	bool bClip = false;
	for (int32 Step = 0; Step < 40; ++Step)
	{
		W.Tick(0.05f);
		Highest = FMath::Max(Highest, Pusho->GetActorLocation().Z);
		bClip |= PlayingClip(Pusho) == TEXT("ANIM_Pusho_Pounce");
	}
	const float EndGap = FVector::Dist2D(Pusho->GetActorLocation(), Ninja->GetActorLocation());
	TestTrue(FString::Printf(TEXT("He leaps (rose %.0f; the arc tops out about 44 up)"), Highest - StartZ), Highest - StartZ > 30.f);
	TestTrue(FString::Printf(TEXT("onto the ninja: from %.0f away to %.0f"), StartGap, EndGap), StartGap > 500.f && EndGap < 200.f);
	TestEqual(TEXT("and claws it once, for 65"), ShinobiTest::Get(Ninja, UShinobiAttributeSet::GetHealthAttribute()), 135.f);
	TestTrue(TEXT("with the pounce clip (run Scripts/setup_creatures.py)"), bClip);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPushoLookTest, "ShinobiCoop.Summons.PushoWearsTheCatFacingForward", ShinobiTestFlags)
bool FShinobiPushoLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Pusho, FVector(400.f, 0.f, 0.f));
	if (Pack.Num() != 1)
	{
		return false;
	}
	const AShinobiSummon* Pusho = Pack[0];
	const USkeletalMeshComponent* Body = Pusho->GetBodyMesh();
	if (!TestNotNull(TEXT("The cat's mesh (run Scripts/setup_creatures.py)"), Body->GetSkeletalMeshAsset()))
	{
		return false;
	}
	TestEqual(TEXT("Pusho's model"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Pusho")));
	TestFalse(TEXT("The mannequin under it is hidden"), Pusho->GetMesh()->IsVisible());
	TestTrue(TEXT("It faces the way Pusho does (the model faces +X)"), FVector::DotProduct(Body->GetComponentTransform().TransformVectorNoScale(FVector(1.f, 0.f, 0.f)), Pusho->GetActorForwardVector()) > 0.99f);
	const float Height = Body->Bounds.BoxExtent.Z * 2.f;
	TestTrue(FString::Printf(TEXT("Panther-sized, about 1 m tall (%.0f cm)"), Height), Height > 85.f && Height < 120.f);
	const float FeetGap = (Body->Bounds.Origin.Z - Body->Bounds.BoxExtent.Z) - (Pusho->GetActorLocation().Z - Pusho->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	TestTrue(FString::Printf(TEXT("Paws on the capsule's floor (off by %.0f)"), FeetGap), FMath::Abs(FeetGap) < 20.f);
	W.Tick(3.f);
	TestTrue(FString::Printf(TEXT("Idle with nothing to fight (%s)"), *PlayingClip(Pusho)), PlayingClip(Pusho).Contains(TEXT("Idle")));
	return true;
}

#endif
