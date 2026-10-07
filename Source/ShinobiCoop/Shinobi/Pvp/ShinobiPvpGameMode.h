#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Pvp/ShinobiPvpMatch.h"
#include "Shinobi/World/ShinobiEncounter.h"
#include "ShinobiPvpGameMode.generated.h"

class AShinobiBotFighter;
class AShinobiGameState;
class APlayerStart;

/**
 * A PvP arena match (URL option Format=1v1|2v2): humans join A, B, A, B; a 15 s warmup, then best-of-three rounds, each a
 * 3 s countdown with everyone held at their team's starts, then a fight until one side has nobody standing (downed is out,
 * revive is off) or the 120 s cap (more team health wins). A 5 s banner and a full reset between rounds, a 10 s result,
 * then the same map reloads for a rematch. The rules live in FShinobiPvpMatch; this drives them and the world.
 * PlayerStarts tagged Team:A / Team:B are the sides' spawns; TargetPoints tagged Pickup:<Type> are the power-ups' spots.
 * Test hook for Scripts/smoke_pvp.ps1 (not in Shipping): -ShinobiPvpSmokeKill=<A|B>@<seconds> kills that team that long after each fight starts.
 */
UCLASS()
class SHINOBICOOP_API AShinobiPvpGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShinobiPvpGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Actor tags of each side's PlayerStarts. */
	static const FName TeamStartTagA;
	static const FName TeamStartTagB;

	/** 1v1 or 2v2 (InitGame's Format option); None until set, and the mode idles while it is. */
	EShinobiPvpFormat PvpFormat = EShinobiPvpFormat::None;

	/** Team's fighters standing: players (and bots) on that side, found in the world, so a destroyed pawn simply counts as
	 * dead and summons are never counted. HealthFraction is over all the side's slots (an empty slot counts 0). */
	FShinobiTeamStatus GatherStatus(EShinobiTeam Team) const;

	/** Team's tagged PlayerStarts (Team:A / Team:B), by name; none for a team that is neither side. */
	TArray<APlayerStart*> GetTeamStarts(EShinobiTeam Team) const;

	/** Gives Controller a side if it has none (TeamForJoin), and puts its pawn on it. */
	void AssignTeam(AController* Controller);

	/** The side a controller plays on (picked at login); Players when it has none. */
	EShinobiTeam GetTeamOf(AController* Controller) const;

	/** Always false: Login caches a start before the side is picked, so the start is chosen afresh at each spawn. */
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

	/** A pickup (AShinobiPickup) at each TargetPoint tagged Pickup:<Type>. Once, at BeginPlay; the points are map data. */
	void SpawnPickups();

	/** Dev aid (Shinobi.KillTeam A|B, server only): zeroes the health of every living fighter on Team, through the normal
	 * out-of-health path, so the round judge sees the side out. */
	void KillTeam(EShinobiTeam Team);

	/** Resets everyone (ResetCharacters, OnRoundReset), holds them and starts the round's countdown. */
	void StartRound();

	/** Ends the round with Result (scores it and starts the banner). Public for the tests; the tick calls it. */
	void EndRound(EShinobiRoundResult Result);

	/** True while a human who just joined should get the round restarted: in the countdown or the fight, with nothing scored
	 * or tied yet. Then a bot that took the empty slot at the end of warmup is replaced by the human and the round still counts. */
	bool ShouldRestartForLateJoiner() const;

	/** StartRound for a late joiner, if ShouldRestartForLateJoiner. */
	void RestartRoundForLateJoiner();

	/** Respawns dead or missing player pawns; revives, refills and moves every fighter to its team's starts, and clears the
	 * round's leftovers (mount, summons, boosts, shields, the hold, statuses, running abilities). */
	void ResetCharacters();

	/** A bot (AShinobiBotFighter) for each empty slot: per side, SlotsPerTeam minus the side's humans, less the bots it already
	 * has, each at a free start of its side. Each round's reset (OnRoundReset) calls it, the first round's included. */
	virtual void FillBots();

protected:
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Hook at each round's reset, after ResetCharacters: every pickup is back, and the bots are replaced by fresh ones (the
	 * dead are destroyed 3 s after dying; a human who joined since takes a bot's slot). */
	virtual void OnRoundReset();

	/** The fighters on Team, in world order: players and bots, never summons or ordinary enemies. */
	TArray<AShinobiCharacterBase*> GetTeamMembers(EShinobiTeam Team) const;

	/** The first of Starts nobody stands on, else any of them; null when there are none. */
	APlayerStart* PickFreeStart(const TArray<APlayerStart*>& Starts) const;

	/** Humans per side among the logged-in players. */
	int32 CountHumans(EShinobiTeam Team) const;

private:
	AShinobiGameState* GetPvpState() const;
	void SetPhase(EShinobiPvpPhase NewPhase, float Seconds);
	void StartWarmup();
	void StartFight();
	void EndMatch();
	void Rematch();
	void SetHold(bool bHold);
	void DespawnBots();

	FShinobiPvpMatch Match;
	TMap<TWeakObjectPtr<AController>, EShinobiTeam> Teams;
	/** Spawns the bots (HP scale of one player, so a bot has a player's health). */
	FShinobiEncounter BotSpawner;
	TArray<TWeakObjectPtr<AShinobiBotFighter>> Bots;
	bool bTravelling = false;
};
