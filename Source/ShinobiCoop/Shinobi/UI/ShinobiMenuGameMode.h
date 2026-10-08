#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "ShinobiMenuGameMode.generated.h"

/** Lvl_MainMenu's mode: no pawn, just the main menu. */
UCLASS()
class SHINOBICOOP_API AShinobiMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShinobiMenuGameMode();

protected:
	/** No pawn for anyone. */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override {}
};

/**
 * Shows the main menu with the cursor. Test hooks for Scripts/smoke_m4.ps1: -ShinobiAutoHost hosts at once;
 * -ShinobiAutoJoin searches every 2 s and joins the first game found. For Scripts/smoke_pvp.ps1, -ShinobiAutoHost also
 * takes -ShinobiAutoHostArena=<WestEnd|Camden|Westminster|Tower> (hosts that PvP arena) and -ShinobiAutoHostFormat=1v1|2v2
 * (the arena's format, 1v1 if absent); without the arena switch it hosts the default map exactly as before.
 */
UCLASS()
class SHINOBICOOP_API AShinobiMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	void AutoJoinStep();

	FTimerHandle AutoTimer;
};
