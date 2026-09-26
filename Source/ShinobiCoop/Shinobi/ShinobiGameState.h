#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ShinobiGameState.generated.h"

UENUM(BlueprintType)
enum class EShinobiArenaPhase : uint8
{
	/** The dummy training mode (AShinobiGameMode): no waves. */
	None,
	/** Countdown to the next wave. */
	Break,
	Fighting,
	/** Everyone went down: banner, then the wave restarts. */
	Wiped,
	Victory
};

/** Arena progress for every machine's HUD, set by AShinobiArenaGameMode on the server. */
UCLASS()
class SHINOBICOOP_API AShinobiGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seconds until the current phase ends (Break, Wiped); 0 for open-ended phases. */
	float GetPhaseTimeRemaining() const;

	/** 1-based; the wave being fought, or the next one during a break. */
	UPROPERTY(Replicated)
	int32 Wave = 0;

	UPROPERTY(Replicated)
	int32 NumWaves = 0;

	UPROPERTY(Replicated)
	EShinobiArenaPhase Phase = EShinobiArenaPhase::None;

	UPROPERTY(Replicated)
	int32 EnemiesRemaining = 0;

	/** Server world time the phase ends (compare with GetServerWorldTimeSeconds); 0 = open-ended. */
	UPROPERTY(Replicated)
	float PhaseEndTime = 0.f;
};
