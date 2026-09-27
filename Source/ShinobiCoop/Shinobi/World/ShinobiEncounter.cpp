#include "Shinobi/World/ShinobiEncounter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"

float FShinobiEncounter::GetHealthScale(int32 NumPlayers)
{
	return 1.f + 0.5f * FMath::Max(0, NumPlayers - 1);
}

FVector FShinobiEncounter::DropToFloor(const UWorld* World, const FVector& Point)
{
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiSpawn));
	if (World && World->LineTraceSingleByChannel(Hit, Point + FVector(0.f, 0.f, 1000.f), Point - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, 100.f);
	}
	return Point;
}

AShinobiEnemyCharacter* FShinobiEncounter::Spawn(UObject* Owner, TSubclassOf<AShinobiEnemyCharacter> EnemyClass, const FVector& Location,
	const FVector& FaceTowards, int32 NumPlayers)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || !EnemyClass)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.f, (FaceTowards - Location).Rotation().Yaw, 0.f), Location);
	AShinobiEnemyCharacter* Enemy = World->SpawnActorDeferred<AShinobiEnemyCharacter>(EnemyClass, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}
	Enemy->HealthScale = GetHealthScale(NumPlayers);
	Enemy->FinishSpawning(Transform);
	Enemy->OnDied.AddWeakLambda(Owner, [this](AShinobiEnemyCharacter* Dead)
	{
		Enemies.Remove(Dead);
		if (OnEnemyDied)
		{
			OnEnemyDied(Dead);
		}
	});
	Enemies.Add(Enemy);
	return Enemy;
}

int32 FShinobiEncounter::Remaining() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AShinobiEnemyCharacter>& Enemy : Enemies)
	{
		Count += Enemy.IsValid() && Enemy->IsAlive() ? 1 : 0;
	}
	return Count;
}

int32 FShinobiEncounter::Prune()
{
	return Enemies.RemoveAll([](const TWeakObjectPtr<AShinobiEnemyCharacter>& Enemy) { return !Enemy.IsValid(); });
}

void FShinobiEncounter::KillAll()
{
	// Copy: each death edits Enemies.
	for (const TWeakObjectPtr<AShinobiEnemyCharacter>& Enemy : TArray<TWeakObjectPtr<AShinobiEnemyCharacter>>(Enemies))
	{
		if (Enemy.IsValid() && Enemy->IsAlive())
		{
			Enemy->GetAbilitySystemComponent()->SetNumericAttributeBase(UShinobiAttributeSet::GetHealthAttribute(), 0.f);
		}
	}
}

void FShinobiEncounter::DespawnAll()
{
	for (const TWeakObjectPtr<AShinobiEnemyCharacter>& Enemy : Enemies)
	{
		if (Enemy.IsValid())
		{
			Enemy->Destroy();
		}
	}
	Enemies.Empty();
}
