#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shinobi/World/ShinobiEncounter.h"
#include "ShinobiCamp.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class EShinobiCampState : uint8
{
	Idle,
	Active,
	Cleared
};

/** One wave of a camp. */
USTRUCT(BlueprintType)
struct FShinobiCampWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Camp")
	int32 Ninjas = 0;

	UPROPERTY(EditAnywhere, Category = "Camp")
	int32 Throwers = 0;
};

/**
 * An enemy camp of the open world. Server: when a living player comes within Radius, its waves spawn one after another
 * (HP scaled by the player count); when the last dies it's Cleared for the rest of the session (OnCleared, the flame
 * over the campfire turns blue). If no living player is within Radius x LeashFactor for ResetSeconds, the enemies
 * despawn and it's Idle again. A boss camp spawns the Rogue Jonin instead, who calls 2 Rogue Ninjas below half health.
 * Spawned at runtime by AShinobiWorldGameMode from the level's camp markers.
 */
UCLASS()
class SHINOBICOOP_API AShinobiCamp : public AActor
{
	GENERATED_BODY()

public:
	AShinobiCamp();

	static constexpr float ResetSeconds = 30.f;
	static constexpr float LeashFactor = 1.5f;
	static constexpr float ThinkInterval = 0.25f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: back to Idle at once, enemies removed (a wipe). Cleared camps stay cleared. */
	void ResetEncounter();

	EShinobiCampState GetState() const { return State; }
	int32 GetRemaining() const { return Encounter.Remaining(); }

	/** Server: fires once, when the camp is cleared. */
	FSimpleMulticastDelegate OnCleared;

	UPROPERTY(EditAnywhere, Category = "Camp")
	FName CampId;

	/** Trigger radius (cm) around the camp centre. */
	UPROPERTY(EditAnywhere, Category = "Camp")
	float Radius = 3500.f;

	UPROPERTY(EditAnywhere, Category = "Camp")
	int32 Difficulty = 1;

	UPROPERTY(EditAnywhere, Category = "Camp")
	TArray<FShinobiCampWave> Waves;

	UPROPERTY(EditAnywhere, Category = "Camp")
	bool bBoss = false;

private:
	/** Living players (downed and dead ones don't count) within Distance (2D, cm) of the camp. */
	int32 PlayersWithin(float Distance) const;
	int32 NumPlayers() const;
	void Activate();
	void SpawnWave(int32 Index);
	void Clear();
	FVector SpawnPoint(int32 Slot, int32 NumSlots) const;
	void OnBossHealthChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void OnRep_State();
	void UpdateFlame();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	EShinobiCampState State = EShinobiCampState::Idle;

	UPROPERTY(VisibleAnywhere, Category = "Camp")
	TObjectPtr<UStaticMeshComponent> Flame;

	UPROPERTY(VisibleAnywhere, Category = "Camp")
	TObjectPtr<UPointLightComponent> Light;

	FShinobiEncounter Encounter;
	int32 WaveIndex = 0;
	float EmptyTime = 0.f;
	float ThinkClock = 0.f;
	bool bBossSummoned = false;
};
