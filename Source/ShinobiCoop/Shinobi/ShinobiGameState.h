#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Shinobi/Pvp/ShinobiPvpMatch.h"
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
	Victory,
	/** The open world (AShinobiWorldGameMode): free roaming, camps cleared n of NumCamps. */
	OpenWorld
};

/** Arena progress for every machine's HUD, set by AShinobiArenaGameMode (or AShinobiPvpGameMode) on the server. */
UCLASS()
class SHINOBICOOP_API AShinobiGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seconds until the current phase ends (Break, Wiped; a PvP phase); 0 for open-ended phases. */
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

	/** Open world: camps cleared so far and in total (the boss camp not counted). */
	UPROPERTY(Replicated)
	int32 CampsCleared = 0;

	UPROPERTY(Replicated)
	int32 NumCamps = 0;

	/** Open world: where the village is, for the compass. */
	UPROPERTY(Replicated)
	FVector VillageLocation = FVector::ZeroVector;

	/** Open world: the hub's compass label (the PlayerStart's Hub:<name> tag; London: "Leicester Square"). */
	UPROPERTY(Replicated)
	FString HubName = TEXT("Village");

	/** Open world: the banner when the boss camp falls (the boss marker's Victory:<text> tag). */
	UPROPERTY(Replicated)
	FString VictoryText = TEXT("VALLEY CLEARED");

	/** A PvP arena match (AShinobiPvpGameMode) is on: teams fight rounds, nobody is revived. */
	bool IsPvp() const { return PvpPhase != EShinobiPvpPhase::None; }

	/** PvP: where the match stands; PhaseEndTime is when this phase ends. */
	UPROPERTY(Replicated)
	EShinobiPvpPhase PvpPhase = EShinobiPvpPhase::None;

	UPROPERTY(Replicated)
	EShinobiPvpFormat PvpFormat = EShinobiPvpFormat::None;

	/** PvP: 1-based; the round being fought (or just fought, during its RoundEnd banner). */
	UPROPERTY(Replicated)
	int32 PvpRound = 0;

	UPROPERTY(Replicated)
	int32 PvpWinsA = 0;

	UPROPERTY(Replicated)
	int32 PvpWinsB = 0;

	/** PvP: the round's result for the RoundEnd banner; at MatchEnd, the match's. Ongoing otherwise. */
	UPROPERTY(Replicated)
	EShinobiRoundResult PvpRoundResult = EShinobiRoundResult::Ongoing;
};
