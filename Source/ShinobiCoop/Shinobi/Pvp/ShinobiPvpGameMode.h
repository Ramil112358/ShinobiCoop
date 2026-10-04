#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Pvp/ShinobiPvpMatch.h"
#include "ShinobiPvpGameMode.generated.h"

class AShinobiGameState;
class APlayerStart;

/**
 * A PvP arena match (URL option Format=1v1|2v2): humans join A, B, A, B; a 15 s warmup, then best-of-three rounds, each a
 * 3 s countdown with everyone held at their team's starts, then a fight until one side has nobody standing (downed is out,
 * revive is off) or the 120 s cap (more team health wins). A 5 s banner and a full reset between rounds, a 10 s result,
 * then the same map reloads for a rematch. The rules live in FShinobiPvpMatch; this drives them and the world.
 * PlayerStarts tagged Team:A / Team:B are the sides' spawns.
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

	/** Resets everyone (ResetCharacters, OnRoundReset), holds them and starts the round's countdown. */
	void StartRound();

	/** Respawns dead or missing player pawns; revives, refills and moves every fighter to its team's starts, and clears the
	 * round's leftovers (mount, summons, boosts, shields, the hold, statuses, running abilities). */
	void ResetCharacters();

protected:
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Hook at each round's reset, after ResetCharacters (pickups, bots). */
	virtual void OnRoundReset() {}

	/** End of warmup: a bot for each empty slot. */
	// ponytail: stub until the bot fighters exist (bots task)
	virtual void FillBots() {}

	/** The fighters on Team, in world order: players (and, later, bots), never summons. */
	TArray<AShinobiCharacterBase*> GetTeamMembers(EShinobiTeam Team) const;

	/** Team's tagged PlayerStarts, by name. */
	TArray<APlayerStart*> GetTeamStarts(EShinobiTeam Team) const;

	/** The side a controller plays on (picked at login); Players when it has none. */
	EShinobiTeam GetTeamOf(AController* Controller) const;

	/** Humans per side among the logged-in players. */
	int32 CountHumans(EShinobiTeam Team) const;

private:
	AShinobiGameState* GetPvpState() const;
	void SetPhase(EShinobiPvpPhase NewPhase, float Seconds);
	void StartWarmup();
	void StartFight();
	void EndRound(EShinobiRoundResult Result);
	void EndMatch();
	void Rematch();
	/** Gives Controller a side if it has none (TeamForJoin), and puts its pawn on it. */
	void AssignTeam(AController* Controller);
	void SetHold(bool bHold);

	FShinobiPvpMatch Match;
	TMap<TWeakObjectPtr<AController>, EShinobiTeam> Teams;
	bool bTravelling = false;
};
