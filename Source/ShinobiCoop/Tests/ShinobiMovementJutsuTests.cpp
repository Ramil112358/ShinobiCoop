#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiMarkerKunai.h"
#include "Shinobi/Abilities/GA_Teleport.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Animation/AnimMontage.h"
#include "Engine/Texture2D.h"
#include "Shinobi/Abilities/GA_AlmightyPush.h"
#include "Shinobi/Abilities/GA_EarthWall.h"
#include "Shinobi/Abilities/GA_Kamui.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_WaterWall.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "Shinobi/ShinobiBarrier.h"
#include "Shinobi/ShinobiProjectile.h"
#include "Shinobi/VFX/ShinobiBurst.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOffPageJutsuTest, "ShinobiCoop.JutsuPages.OffPageJutsuDoesNotFire", ShinobiTestFlags)
bool FShinobiOffPageJutsuTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	TestTrue(TEXT("Starts on page 1"), ShinobiTest::HasTag(P, ShinobiTags::JutsuPage(1)));
	P->ApplyJutsuPage(5); // empty: nothing lives there
	TestFalse(TEXT("Page 1 tag removed"), ShinobiTest::HasTag(P, ShinobiTags::JutsuPage(1)));
	TestEqual(TEXT("On page 5"), P->GetJutsuPage(), 5);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Fireball is on page 1: key 1 does nothing on page 5"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestEqual(TEXT("Nothing spent"), ShinobiTest::Get(P, Chakra), 100.f);

	P->ApplyJutsuPage(1);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestTrue(TEXT("Back on page 1 the Fireball fires"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	TestEqual(TEXT("Cost paid"), ShinobiTest::Get(P, Chakra), 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiEnemyJutsuIgnoresPagesTest, "ShinobiCoop.JutsuPages.EnemyJutsuIgnoresPages", ShinobiTestFlags)
bool FShinobiEnemyJutsuIgnoresPagesTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>();
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	ASC->GiveAbility(FGameplayAbilitySpec(UGA_EnemyFireball::StaticClass(), 1, INDEX_NONE));
	TestTrue(TEXT("An enemy has no page tag and still casts"), ASC->TryActivateAbilityByClass(UGA_EnemyFireball::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiServerRejectsPageTest, "ShinobiCoop.JutsuPages.ServerRejectsOutOfRangePage", ShinobiTestFlags)
bool FShinobiServerRejectsPageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	P->ServerSetJutsuPage_Implementation(9);
	TestEqual(TEXT("A client's out-of-range page is ignored"), P->GetJutsuPage(), 1);
	P->ServerSetJutsuPage_Implementation(0);
	TestEqual(TEXT("Page 0 is ignored too"), P->GetJutsuPage(), 1);
	P->ServerSetJutsuPage_Implementation(3);
	TestEqual(TEXT("A valid page is taken"), P->GetJutsuPage(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiNewBurstKindsTest, "ShinobiCoop.Burst.NewKindsSpawnAndKeepKindAndScale", ShinobiTestFlags)
bool FShinobiNewBurstKindsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	for (const EShinobiBurst Kind : { EShinobiBurst::Shockwave, EShinobiBurst::Swirl, EShinobiBurst::TeleportFlash, EShinobiBurst::Crumble, EShinobiBurst::Splash })
	{
		const AShinobiBurst* Burst = AShinobiBurst::Spawn(W.World, Kind, FVector::ZeroVector, 1.5f);
		if (TestNotNull(TEXT("Burst spawns"), Burst))
		{
			TestEqual(TEXT("Kind kept"), Burst->GetKind(), Kind);
			TestEqual(TEXT("Scale kept"), Burst->GetScale(), 1.5f);
		}
	}
	W.Tick(0.2f); // every kind ticks without a crash (sound and look tables cover the new kinds)
	return true;
}

namespace
{
	/** A projectile of Class flying along Rotation from Start, thrown by Caster. */
	AShinobiProjectile* Throw(const FShinobiTestWorld& W, TSubclassOf<AShinobiProjectile> Class, AShinobiCharacterBase* Caster, const FVector& Start, const FRotator& Rotation, float Damage)
	{
		const FTransform Launch(Rotation, Start);
		AShinobiProjectile* Shot = W.World->SpawnActorDeferred<AShinobiProjectile>(Class, Launch, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Shot->DirectDamage = Damage;
		Shot->FinishSpawning(Launch);
		return Shot;
	}

	const AShinobiBarrier* LastBarrier(const FShinobiTestWorld& W)
	{
		const AShinobiBarrier* Wall = nullptr;
		for (TActorIterator<AShinobiBarrier> It(W.World); It; ++It)
		{
			Wall = *It;
		}
		return Wall;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBarrierBlocksEnemyShotTest, "ShinobiCoop.Barrier.EnemyProjectileDamagesAndStopsAtWall", ShinobiTestFlags)
bool FShinobiBarrierBlocksEnemyShotTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(900.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(E);
	AShinobiBarrier* Wall = AShinobiBarrier::Spawn(W.World, FTransform(FRotator::ZeroRotator, FVector(300.f, 0.f, -100.f)), P, EShinobiBarrierKind::Earth);
	const TWeakObjectPtr<AShinobiProjectile> Kunai(Throw(W, AShinobiKunai::StaticClass(), E, FVector(860.f, 0.f, 40.f), FRotator(0.f, 180.f, 0.f), 30.f));

	W.Tick(0.6f);
	TestFalse(TEXT("The kunai is spent on the wall"), Kunai.IsValid());
	TestEqual(TEXT("The wall took 30"), Wall->GetHealth(), 370.f);
	TestEqual(TEXT("Nothing got past"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBarrierFriendlyShotTest, "ShinobiCoop.Barrier.FriendlyProjectileFliesThrough", ShinobiTestFlags)
bool FShinobiBarrierFriendlyShotTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Target = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(Target);
	AShinobiBarrier* Wall = AShinobiBarrier::Spawn(W.World, FTransform(FRotator::ZeroRotator, FVector(300.f, 0.f, -100.f)), P, EShinobiBarrierKind::Earth);
	Throw(W, AShinobiProjectile::StaticClass(), P, FVector(60.f, 0.f, 40.f), FRotator::ZeroRotator, 60.f);

	W.Tick(0.6f);
	TestEqual(TEXT("The shot reached the enemy past the caster's own wall"), ShinobiTest::Get(Target, UShinobiAttributeSet::GetHealthAttribute()), 440.f);
	TestEqual(TEXT("The wall is untouched"), Wall->GetHealth(), 400.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBarrierLifeTest, "ShinobiCoop.Barrier.BreaksAtZeroAndExpires", ShinobiTestFlags)
bool FShinobiBarrierLifeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FTransform Base(FRotator::ZeroRotator, FVector(300.f, 0.f, -100.f));

	TWeakObjectPtr<AShinobiBarrier> Broken(AShinobiBarrier::Spawn(W.World, Base, P, EShinobiBarrierKind::Earth));
	Broken->ReceiveShinobiDamage(400.f, nullptr);
	W.Tick(0.1f);
	TestFalse(TEXT("Broken at 0 HP"), Broken.IsValid());

	TWeakObjectPtr<AShinobiBarrier> Earth(AShinobiBarrier::Spawn(W.World, Base, P, EShinobiBarrierKind::Earth));
	W.Tick(9.8f);
	TestTrue(TEXT("An earth wall stands for ten seconds"), Earth.IsValid());
	W.Tick(0.5f);
	TestFalse(TEXT("then crumbles"), Earth.IsValid());

	TWeakObjectPtr<AShinobiBarrier> Water(AShinobiBarrier::Spawn(W.World, Base, P, EShinobiBarrierKind::Water));
	TestEqual(TEXT("A water wall is weaker"), Water->GetHealth(), 250.f);
	W.Tick(5.8f);
	TestTrue(TEXT("and lasts six seconds"), Water.IsValid());
	W.Tick(0.5f);
	TestFalse(TEXT("then goes"), Water.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBarrierBurstTest, "ShinobiCoop.Barrier.BreakingAndExpiringLeaveABurst", ShinobiTestFlags)
bool FShinobiBarrierBurstTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FTransform Base(FRotator::ZeroRotator, FVector(300.f, 0.f, -100.f));
	const auto FindBurst = [&W](EShinobiBurst Kind) -> const AShinobiBurst*
	{
		for (TActorIterator<AShinobiBurst> It(W.World); It; ++It)
		{
			if (It->GetKind() == Kind)
			{
				return *It;
			}
		}
		return nullptr;
	};

	AShinobiBarrier::Spawn(W.World, Base, P, EShinobiBarrierKind::Earth)->ReceiveShinobiDamage(400.f, nullptr);
	W.Tick(0.1f);
	const AShinobiBurst* Crumble = FindBurst(EShinobiBurst::Crumble);
	if (TestNotNull(TEXT("A broken earth wall crumbles"), Crumble))
	{
		TestEqual(TEXT("at full size"), Crumble->GetScale(), 1.f);
	}

	AShinobiBarrier::Spawn(W.World, Base, P, EShinobiBarrierKind::Water);
	W.Tick(6.15f);
	const AShinobiBurst* Splash = FindBurst(EShinobiBurst::Splash);
	if (TestNotNull(TEXT("A water wall that runs out splashes"), Splash))
	{
		TestEqual(TEXT("at half size"), Splash->GetScale(), 0.5f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBarrierReplaceTest, "ShinobiCoop.Barrier.NewWallReplacesOnlyOwnersOldWall", ShinobiTestFlags)
bool FShinobiBarrierReplaceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* P2 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 500.f, 0.f));
	const FTransform Base(FRotator::ZeroRotator, FVector(300.f, 0.f, -100.f));

	const TWeakObjectPtr<AShinobiBarrier> First(AShinobiBarrier::Spawn(W.World, Base, P1, EShinobiBarrierKind::Earth));
	const TWeakObjectPtr<AShinobiBarrier> Other(AShinobiBarrier::Spawn(W.World, Base, P2, EShinobiBarrierKind::Water));
	const TWeakObjectPtr<AShinobiBarrier> Second(AShinobiBarrier::Spawn(W.World, Base, P1, EShinobiBarrierKind::Water));
	W.Tick(0.1f);
	TestFalse(TEXT("P1's first wall broke"), First.IsValid());
	TestTrue(TEXT("P1's new wall stands"), Second.IsValid());
	TestTrue(TEXT("P2's wall is not P1's to replace"), Other.IsValid());
	TestEqual(TEXT("Two walls"), ShinobiTest::CountActors<AShinobiBarrier>(W), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiEarthWallCastTest, "ShinobiCoop.Wall.EarthWallCast", ShinobiTestFlags)
bool FShinobiEarthWallCastTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestEqual(TEXT("Cost 35 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 65.f);
	W.Tick(0.9f);

	const AShinobiBarrier* Wall = LastBarrier(W);
	if (TestNotNull(TEXT("The wall rose after the seals"), Wall))
	{
		TestEqual(TEXT("Earth"), Wall->GetKind(), EShinobiBarrierKind::Earth);
		TestEqual(TEXT("400 HP"), Wall->GetHealth(), 400.f);
		TestTrue(FString::Printf(TEXT("350 ahead of the caster (X=%.0f)"), Wall->GetActorLocation().X), FMath::IsNearlyEqual(Wall->GetActorLocation().X, 350.f, 30.f));
	}
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_EarthWall));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWaterWallCastTest, "ShinobiCoop.Wall.WaterWallCastOnPage3", ShinobiTestFlags)
bool FShinobiWaterWallCastTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestEqual(TEXT("Cost 25 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 75.f);
	W.Tick(0.8f);

	const AShinobiBarrier* Wall = LastBarrier(W);
	if (TestNotNull(TEXT("The wall rose"), Wall))
	{
		TestEqual(TEXT("Water"), Wall->GetKind(), EShinobiBarrierKind::Water);
		TestEqual(TEXT("250 HP"), Wall->GetHealth(), 250.f);
	}
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_WaterWall));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPageCycleTest, "ShinobiCoop.JutsuPages.CycleSkipsEmptyAndBlocksWhileCasting", ShinobiTestFlags)
bool FShinobiPageCycleTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	TestTrue(TEXT("Forward from 1"), P->TryCycleJutsuPage(1));
	TestEqual(TEXT("to 2"), P->GetJutsuPage(), 2);
	TestTrue(TEXT("Forward from 2"), P->TryCycleJutsuPage(1));
	TestEqual(TEXT("to 3"), P->GetJutsuPage(), 3);
	TestTrue(TEXT("Forward from 3"), P->TryCycleJutsuPage(1));
	TestEqual(TEXT("to 4 (Susanoo)"), P->GetJutsuPage(), 4);
	TestTrue(TEXT("Forward from 4"), P->TryCycleJutsuPage(1));
	TestEqual(TEXT("page 5 is empty: wraps to 1"), P->GetJutsuPage(), 1);
	TestTrue(TEXT("Back from 1"), P->TryCycleJutsuPage(-1));
	TestEqual(TEXT("wraps to 4"), P->GetJutsuPage(), 4);

	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestFalse(TEXT("No page switch mid-cast"), P->TryCycleJutsuPage(1));
	TestEqual(TEXT("still page 2"), P->GetJutsuPage(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPushTest, "ShinobiCoop.Push.LaunchesOnlyHostilesInRadius", ShinobiTestFlags)
bool FShinobiPushTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Near = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	AShinobiCharacterBase* Far = W.Spawn<AShinobiCharacterBase>(FVector(0.f, 900.f, 0.f));
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 300.f, 0.f));
	for (AShinobiCharacterBase* Each : { static_cast<AShinobiCharacterBase*>(P), Near, Far, static_cast<AShinobiCharacterBase*>(Ally) })
	{
		ShinobiTest::Hover(Each);
	}
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Cost 30 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 70.f);
	W.Tick(0.9f);
	W.Tick(0.3f);

	TestEqual(TEXT("Enemy in range takes 25"), ShinobiTest::Get(Near, Health), 475.f);
	TestTrue(FString::Printf(TEXT("and is thrown away from the caster (X=%.0f)"), Near->GetActorLocation().X), Near->GetActorLocation().X > 400.f);
	TestEqual(TEXT("Enemy out of range untouched"), ShinobiTest::Get(Far, Health), 500.f);
	TestEqual(TEXT("and not moved"), Far->GetActorLocation().Y, 900.0);
	TestEqual(TEXT("Ally untouched"), ShinobiTest::Get(Ally, Health), 500.f);
	TestEqual(TEXT("and not moved"), Ally->GetActorLocation().Y, 300.0);
	TestEqual(TEXT("Caster not moved"), P->GetActorLocation().X, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKamuiDamageTest, "ShinobiCoop.Kamui.IgnoresAllDamageThenExpires", ShinobiTestFlags)
bool FShinobiKamuiDamageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(E);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	ShinobiTest::FEventCounter Substitutions(P, ShinobiTags::Event_Substitution);

	ShinobiTest::ApplyEffect(P, UGE_Kamui::StaticClass());
	TestTrue(TEXT("Phased"), ShinobiTest::HasTag(P, ShinobiTags::State_Phased));
	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("A direct hit does nothing"), ShinobiTest::Get(P, Health), 500.f);
	TestFalse(TEXT("and does not count as landing"), ShinobiCombat::ApplyDamage(E->GetAbilitySystemComponent(), P, 50.f));
	TestEqual(TEXT("Phasing is not a dodge: no Substitution is asked for"), Substitutions.Count, 0);
	ShinobiTest::ApplySetByCaller(P, UGE_BlackFlame::StaticClass(), ShinobiTags::Data_Damage, 15.f);
	W.Tick(2.5f);
	TestEqual(TEXT("Burning damage over time does nothing either"), ShinobiTest::Get(P, Health), 500.f);

	P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Burning_Black));
	W.Tick(1.0f);
	TestFalse(TEXT("Over after 3 s"), ShinobiTest::HasTag(P, ShinobiTags::State_Phased));
	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("Hurt again"), ShinobiTest::Get(P, Health), 450.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKamuiBodyTest, "ShinobiCoop.Kamui.PassesEnemiesBlocksCastingAndIsNotTargetable", ShinobiTestFlags)
bool FShinobiKamuiBodyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const ECollisionResponse Before = P->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);

	ShinobiTest::ApplyEffect(P, UGE_Kamui::StaticClass());
	TestEqual(TEXT("Walks through pawns"), P->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestFalse(TEXT("The AI cannot target it"), P->IsTargetable());
	TestTrue(TEXT("Phased"), P->IsPhased());
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("No jutsu while phased"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("No melee while phased"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));

	W.Tick(3.2f);
	TestEqual(TEXT("Collision restored"), P->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), Before);
	TestTrue(TEXT("Targetable again"), P->IsTargetable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKamuiCastTest, "ShinobiCoop.Kamui.CastThenExitEarly", ShinobiTestFlags)
bool FShinobiKamuiCastTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	TestEqual(TEXT("Cost 30 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 70.f);
	W.Tick(0.6f);
	TestTrue(TEXT("Phased after the seals"), ShinobiTest::HasTag(P, ShinobiTags::State_Phased));
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Kamui));

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	W.Tick(0.2f);
	TestFalse(TEXT("The same key steps back out"), ShinobiTest::HasTag(P, ShinobiTags::State_Phased));
	TestTrue(TEXT("The cooldown still runs"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Kamui));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKamuiNudgeTest, "ShinobiCoop.Kamui.EndingInsideAnEnemyNudgesOut", ShinobiTestFlags)
bool FShinobiKamuiNudgeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(E);
	ShinobiTest::ApplyEffect(P, UGE_Kamui::StaticClass());
	W.Tick(3.3f);
	TestTrue(FString::Printf(TEXT("Not left inside the enemy (%.0f apart)"), FVector::Dist2D(P->GetActorLocation(), E->GetActorLocation())),
		FVector::Dist2D(P->GetActorLocation(), E->GetActorLocation()) > 20.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKamuiProjectileTest, "ShinobiCoop.Kamui.ProjectileFliesThroughPhasedPlayer", ShinobiTestFlags)
bool FShinobiKamuiProjectileTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f));
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(1000.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(E);
	ShinobiTest::ApplyEffect(P, UGE_Kamui::StaticClass());
	const TWeakObjectPtr<AShinobiProjectile> Kunai(Throw(W, AShinobiKunai::StaticClass(), E, FVector(900.f, 0.f, 40.f), FRotator(0.f, 180.f, 0.f), 30.f));

	W.Tick(0.5f); // 3000 u/s: through P's spot at 300 after ~0.2 s
	TestTrue(TEXT("The kunai passed through instead of bursting"), Kunai.IsValid());
	TestEqual(TEXT("Unhurt"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}

namespace
{
	/** A 100-unit cube scaled into a wall for the marker kunai to stick in (the test world is empty). */
	AStaticMeshActor* SpawnWallBlock(FShinobiTestWorld& W, const FVector& Location, const FVector& Scale)
	{
		AStaticMeshActor* Block = W.World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Block->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Block->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Block->SetActorScale3D(Scale);
		return Block;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTeleportThrowTest, "ShinobiCoop.Teleport.ThrowsThreeKunaiAndMarks", ShinobiTestFlags)
bool FShinobiTeleportThrowTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnWallBlock(W, FVector(1600.f, 0.f, 0.f), FVector(1.f, 30.f, 30.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestEqual(TEXT("Cost 25 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 75.f);
	W.Tick(1.2f);

	TestEqual(TEXT("Three kunai"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W), 3);
	for (TActorIterator<AShinobiMarkerKunai> It(W.World); It; ++It)
	{
		TestTrue(TEXT("each stuck in the wall"), It->IsStuck());
	}
	TestTrue(TEXT("The jump window is open"), ShinobiTest::HasTag(P, ShinobiTags::State_TeleportMarked));
	TestTrue(TEXT("Cooldown started at the throw"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Teleport));
	TestFalse(TEXT("The jutsu itself is over"), ShinobiTest::IsActive(P, UGA_Teleport::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiNewJutsuAssetsTest, "ShinobiCoop.JutsuPages.NewJutsuHaveAnimationsAndIcons", ShinobiTestFlags)
bool FShinobiNewJutsuAssetsTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py and Scripts/setup_hud.py if an asset is missing)
	const auto Check = [this](const UShinobiJutsuAbility* Jutsu, bool bCharge)
	{
		const FString Name = Jutsu->DisplayName.ToString();
		TestNotNull(*(Name + TEXT(" seal montage")), Jutsu->SealMontage.LoadSynchronous());
		TestNotNull(*(Name + TEXT(" release montage")), Jutsu->ReleaseMontage.LoadSynchronous());
		if (bCharge)
		{
			TestNotNull(*(Name + TEXT(" charge montage")), Jutsu->ChargeMontage.LoadSynchronous());
		}
		TestNotNull(*(Name + TEXT(" icon")), Jutsu->Icon.LoadSynchronous());
	};
	Check(GetDefault<UGA_Teleport>(), false);
	Check(GetDefault<UGA_Kamui>(), false);
	Check(GetDefault<UGA_AlmightyPush>(), true);
	Check(GetDefault<UGA_EarthWall>(), false);
	Check(GetDefault<UGA_WaterWall>(), false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTeleportKunaiModelTest, "ShinobiCoop.Teleport.KunaiWearsTheImportedModel", ShinobiTestFlags)
bool FShinobiTeleportKunaiModelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const AShinobiMarkerKunai* Kunai = AShinobiMarkerKunai::Spawn(W.World, P, FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator);
	const UStaticMeshComponent* Mesh = Kunai ? Kunai->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	const UStaticMesh* Asset = Mesh ? Mesh->GetStaticMesh() : nullptr;
	if (TestNotNull(TEXT("A kunai with a mesh asset"), Asset))
	{
		TestEqual(TEXT("The Hiraishin model (run Scripts/setup_marker_kunai.py if this is the cone)"), Asset->GetName(), FString(TEXT("SM_MarkerKunai")));
		// Tip at the origin pointing +X: the mesh extends back along -X, about 49 cm.
		const FBox Bounds = Asset->GetBoundingBox();
		TestTrue(FString::Printf(TEXT("tip forward (X from %.0f to %.0f)"), Bounds.Min.X, Bounds.Max.X), Bounds.Max.X < 5.0 && Bounds.Min.X < -40.0 && Bounds.Min.X > -60.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTeleportJumpTest, "ShinobiCoop.Teleport.RepressJumpsToEachKunaiThenEnds", ShinobiTestFlags)
bool FShinobiTeleportJumpTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnWallBlock(W, FVector(1600.f, 0.f, 0.f), FVector(1.f, 30.f, 30.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.5f);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.2f);
	TestTrue(FString::Printf(TEXT("Jumped to the wall (X=%.0f)"), P->GetActorLocation().X), P->GetActorLocation().X > 1000.0);
	TestEqual(TEXT("one kunai used"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W), 2);
	TestTrue(TEXT("window still open"), ShinobiTest::HasTag(P, ShinobiTags::State_TeleportMarked));

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.2f);
	TestEqual(TEXT("second kunai used"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W), 1);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.2f);
	TestEqual(TEXT("third kunai used"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W), 0);
	TestFalse(TEXT("window closed with the last one"), ShinobiTest::HasTag(P, ShinobiTags::State_TeleportMarked));
	TestTrue(TEXT("The jumps cost nothing"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()) >= 75.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTeleportStrikeTest, "ShinobiCoop.Teleport.ArrivalStrikesNearbyHostiles", ShinobiTestFlags)
bool FShinobiTeleportStrikeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnWallBlock(W, FVector(1600.f, 0.f, 0.f), FVector(1.f, 30.f, 30.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(1400.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(E);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.5f);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1); // aimed along +X: the middle kunai, 100 from the enemy
	W.Tick(0.2f);
	TestEqual(TEXT("The arrival hits for 20"), ShinobiTest::Get(E, UShinobiAttributeSet::GetHealthAttribute()), 480.f);
	TestTrue(TEXT("and staggers"), ShinobiTest::HasTag(E, ShinobiTags::State_Staggered));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTeleportBlockedTest, "ShinobiCoop.Teleport.BlockedArrivalStillGetsTheCasterThere", ShinobiTestFlags)
bool FShinobiTeleportBlockedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnWallBlock(W, FVector(1600.f, 0.f, 0.f), FVector(1.f, 30.f, 30.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.5f);
	// Something solid fills the spot beside the wall where the middle kunai (the one under the crosshair) would put them.
	SpawnWallBlock(W, FVector(1500.f, 0.f, 30.f), FVector(6.f, 6.f, 6.f));

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.2f);
	TestTrue(FString::Printf(TEXT("The caster still arrives by the wall (X=%.0f)"), P->GetActorLocation().X), P->GetActorLocation().X > 1000.0);
	TestTrue(TEXT("and a kunai was used up"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W) <= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTeleportExpiryTest, "ShinobiCoop.Teleport.WindowExpiresAndRemovesKunai", ShinobiTestFlags)
bool FShinobiTeleportExpiryTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnWallBlock(W, FVector(1600.f, 0.f, 0.f), FVector(1.f, 30.f, 30.f));
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.5f);
	W.Tick(5.2f); // 6.7 s after the cast, the window is long shut

	TestEqual(TEXT("Kunai gone"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W), 0);
	TestFalse(TEXT("Window closed"), ShinobiTest::HasTag(P, ShinobiTags::State_TeleportMarked));
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Nothing to jump to"), ShinobiTest::IsActive(P, UGA_TeleportJump::StaticClass()));
	TestFalse(TEXT("and the jutsu is still cooling down"), ShinobiTest::IsActive(P, UGA_Teleport::StaticClass()));
	TestEqual(TEXT("Not moved"), P->GetActorLocation().X, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTeleportSkyTest, "ShinobiCoop.Teleport.OpenSkyThrowLeavesNothingToJumpTo", ShinobiTestFlags)
bool FShinobiTeleportSkyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W; // no wall: the kunai never stick
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.5f);
	TestEqual(TEXT("The unstuck kunai are gone"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W), 0);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.1f);
	TestEqual(TEXT("A re-press goes nowhere"), P->GetActorLocation().X, 0.0);
	TestFalse(TEXT("and nothing is left running"), ShinobiTest::IsActive(P, UGA_TeleportJump::StaticClass()));
	TestFalse(TEXT("the empty window closes"), ShinobiTest::HasTag(P, ShinobiTags::State_TeleportMarked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiTeleportForeignTest, "ShinobiCoop.Teleport.ForeignKunaiAreNotUsed", ShinobiTestFlags)
bool FShinobiTeleportForeignTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	SpawnWallBlock(W, FVector(1600.f, 0.f, 0.f), FVector(1.f, 100.f, 30.f));
	AShinobiPlayerCharacter* P1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* P2 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 2000.f, 0.f));
	ShinobiTest::Hover(P1);
	ShinobiTest::Hover(P2);
	P1->ApplyJutsuPage(2);
	P2->ApplyJutsuPage(2);
	ShinobiTest::Tap(P2, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.5f);
	TestEqual(TEXT("P2's three kunai stand"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W), 3);

	ShinobiTest::ApplyEffect(P1, UGE_TeleportMarked::StaticClass()); // P1 has an open window but threw nothing
	ShinobiTest::Tap(P1, EShinobiAbilityInputID::Jutsu1);
	W.Tick(0.2f);
	TestEqual(TEXT("P1 did not jump to P2's kunai"), P1->GetActorLocation().X, 0.0);
	TestEqual(TEXT("and P2's kunai are all still there"), ShinobiTest::CountActors<AShinobiMarkerKunai>(W), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKamuiGhostTest, "ShinobiCoop.Kamui.GhostMaterialSwapsAndRestores", ShinobiTestFlags)
bool FShinobiKamuiGhostTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	USkeletalMeshComponent* Body = P->GetBodyMesh();
	UMaterialInterface* Original = Body->GetMaterial(0);

	ShinobiTest::ApplyEffect(P, UGE_Kamui::StaticClass());
	W.Tick(0.1f);
	const UMaterialInstanceDynamic* Ghost = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(0));
	if (TestNotNull(TEXT("A ghost material while phased (run Scripts/setup_vfx.py if M_Phased is missing)"), Ghost))
	{
		float Opacity = 0.f;
		Ghost->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Opacity")), Opacity);
		TestEqual(TEXT("at 30% opacity"), Opacity, 0.3f);
	}

	W.Tick(3.2f);
	TestTrue(TEXT("The body's own material is back"), Body->GetMaterial(0) == Original);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
