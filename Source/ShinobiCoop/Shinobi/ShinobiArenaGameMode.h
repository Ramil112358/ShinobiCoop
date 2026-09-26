#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Shinobi/ShinobiGameState.h"
#include "ShinobiArenaGameMode.generated.h"

class AShinobiEnemyCharacter;
class AShinobiRogueJonin;

/** Enemies in one wave. */
struct FShinobiWave
{
	int32 RogueNinjas = 0;
	int32 KunaiThrowers = 0;
	bool bBoss = false;
};

/**
 * The M3 arena: a 10 s break, then waves of enemies spawned at the level's ShinobiEnemySpawn target points (or on a
 * ring around the PlayerStart), with a break (and a
 * full restore) between them; the last wave is the boss, which calls 2 Rogue Ninjas once below half health. If every
 * player is downed or dead, a 5 s banner, then the wave restarts. Beating the boss is victory. Dead players respawn
 * at the next wave start. Enemy HP scales with the player count.
 */
UCLASS()
class SHINOBICOOP_API AShinobiArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShinobiArenaGameMode();

	virtual void Tick(float DeltaSeconds) override;

	static constexpr float BreakSeconds = 10.f;
	static constexpr float WipeSeconds = 5.f;
	static constexpr float SpawnRadius = 1200.f;
	static constexpr int32 BossSummonCount = 2;
	/** Actor tag of the target points enemies spawn around. */
	static const FName SpawnPointTag;

	static int32 GetNumWaves();
	/** Wave is 1-based. */
	static FShinobiWave GetWave(int32 Wave);
	/** Base x (1 + 0.5 x (players - 1)). */
	static float GetHealthScale(int32 NumPlayers);

	/** Dev aid: kills every living enemy (Shinobi.KillEnemies). */
	void KillAllEnemies();

protected:
	virtual void BeginPlay() override;

private:
	AShinobiGameState* GetArenaState() const;
	void StartBreak();
	void StartWave();
	void Wipe();
	void Win();
	void SetPhase(EShinobiArenaPhase NewPhase, float Seconds);

	/** Revives the downed and refills every living player's health, chakra and stamina. */
	void RefillPlayers();
	/** Replaces dead players' bodies with fresh pawns at the PlayerStart. */
	void RespawnDeadPlayers();
	bool AnyPlayerAlive(bool& bOutAnyPlayer) const;

	FVector GetRingPoint(int32 Slot, int32 NumSlots) const;
	AShinobiEnemyCharacter* SpawnEnemy(TSubclassOf<AShinobiEnemyCharacter> EnemyClass, const FVector& Location);
	void OnEnemyDied(AShinobiEnemyCharacter* Enemy);
	void OnBossHealthChanged(const struct FOnAttributeChangeData& Data);

	FVector ArenaCenter = FVector::ZeroVector;
	TArray<FVector> SpawnPoints;
	TArray<TWeakObjectPtr<AShinobiEnemyCharacter>> Enemies;
	TWeakObjectPtr<AShinobiRogueJonin> Boss;
	bool bBossSummoned = false;
};
