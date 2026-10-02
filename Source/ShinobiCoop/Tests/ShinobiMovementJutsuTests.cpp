#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Fireball.h"
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
	P->ApplyJutsuPage(2);
	TestFalse(TEXT("Page 1 tag removed"), ShinobiTest::HasTag(P, ShinobiTags::JutsuPage(1)));
	TestEqual(TEXT("On page 2"), P->GetJutsuPage(), 2);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Fireball is on page 1: key 1 does nothing on page 2"), ShinobiTest::IsActive(P, UGA_Fireball::StaticClass()));
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
	TestEqual(TEXT("pages 4 and 5 are empty: wraps to 1"), P->GetJutsuPage(), 1);
	TestTrue(TEXT("Back from 1"), P->TryCycleJutsuPage(-1));
	TestEqual(TEXT("wraps to 3"), P->GetJutsuPage(), 3);

	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestFalse(TEXT("No page switch mid-cast"), P->TryCycleJutsuPage(1));
	TestEqual(TEXT("still page 2"), P->GetJutsuPage(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
