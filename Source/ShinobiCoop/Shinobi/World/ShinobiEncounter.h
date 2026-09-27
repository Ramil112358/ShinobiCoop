#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class AShinobiEnemyCharacter;
class UObject;
class UWorld;

/**
 * A group of spawned enemies (an arena wave, a camp): spawns them with HP scaled by the player count, keeps track of the
 * living ones and reports each death. Server only. Owned by a UObject (the arena game mode, a camp), which death
 * callbacks are weakly bound to, so they stop once the owner is gone.
 */
struct SHINOBICOOP_API FShinobiEncounter
{
	/** Base x (1 + 0.5 x (players - 1)). */
	static float GetHealthScale(int32 NumPlayers);

	/** Point + 1 m on the floor under Point (a line trace), or Point itself when there's no floor. */
	static FVector DropToFloor(const UWorld* World, const FVector& Point);

	/** Called for each enemy of this encounter that dies. */
	TFunction<void(AShinobiEnemyCharacter*)> OnEnemyDied;

	AShinobiEnemyCharacter* Spawn(UObject* Owner, TSubclassOf<AShinobiEnemyCharacter> EnemyClass, const FVector& Location,
		const FVector& FaceTowards, int32 NumPlayers);

	/** Living, not yet destroyed enemies. */
	int32 Remaining() const;

	/** Forgets enemies destroyed without dying (fell out of the world); returns how many. */
	int32 Prune();

	/** Kills every living enemy (they die normally: ragdoll, OnDied). */
	void KillAll();

	/** Removes every enemy at once, no death (a reset or a wipe). */
	void DespawnAll();

	const TArray<TWeakObjectPtr<AShinobiEnemyCharacter>>& GetEnemies() const { return Enemies; }

private:
	TArray<TWeakObjectPtr<AShinobiEnemyCharacter>> Enemies;
};
