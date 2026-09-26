#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiProjectile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiProjectileTest, "ShinobiCoop.Jutsu.ProjectileHitsFirstHostileAndSplashes", ShinobiTestFlags)
bool FShinobiProjectileTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f)); // in the flight path
	AShinobiCharacterBase* Target = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	AShinobiCharacterBase* Nearby = W.Spawn<AShinobiCharacterBase>(FVector(650.f, 150.f, 0.f));
	AShinobiCharacterBase* Far = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 500.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	const FTransform Launch(FRotator::ZeroRotator, FVector(60.f, 0.f, 40.f)); // flying along +X
	AShinobiProjectile* Ball = W.World->SpawnActorDeferred<AShinobiProjectile>(AShinobiProjectile::StaticClass(), Launch, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Ball->DirectDamage = 60.f;
	Ball->SplashDamage = 40.f;
	Ball->SplashRadius = 250.f;
	Ball->StatusEffect = UGE_Burn::StaticClass();
	Ball->StatusDamagePerTick = UGE_Burn::DamagePerTick;
	Ball->FinishSpawning(Launch);
	const TWeakObjectPtr<AShinobiProjectile> WeakBall(Ball);

	W.Tick(0.5f); // 2500 u/s: reaches the target ~500 units away in ~0.2 s
	TestFalse(TEXT("Projectile destroyed on impact"), WeakBall.IsValid());
	TestEqual(TEXT("Ally in the path is flown through, unharmed"), ShinobiTest::Get(Ally, Health), 500.f);
	TestEqual(TEXT("Direct hit: 60"), ShinobiTest::Get(Target, Health), 440.f);
	TestTrue(TEXT("Direct target burning"), ShinobiTest::HasTag(Target, ShinobiTags::State_Burning_Normal));
	TestEqual(TEXT("Splash: 40"), ShinobiTest::Get(Nearby, Health), 460.f);
	TestFalse(TEXT("Splash doesn't burn"), ShinobiTest::HasTag(Nearby, ShinobiTags::State_Burning_Normal));
	TestEqual(TEXT("Outside the splash radius"), ShinobiTest::Get(Far, Health), 500.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
