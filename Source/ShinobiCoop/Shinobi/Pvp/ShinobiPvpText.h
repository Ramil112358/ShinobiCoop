#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"

class AShinobiGameState;

/** The words of the PvP HUD, pure so the tests need no widget and no server clock (the HUD passes State.GetPhaseTimeRemaining()). */
namespace ShinobiPvp
{
	/** The centre banner: the countdown digit, FIGHT, the round's winner, the match result; empty when nothing is to be said. */
	SHINOBICOOP_API FString BannerText(const AShinobiGameState& State, EShinobiTeam LocalTeam, float PhaseSecondsRemaining);

	/** The top strip: "A 1  -  Round 2  -  B 0  -  1:42"; the timer reads --:-- outside the Fighting phase. */
	SHINOBICOOP_API FString ScoreText(const AShinobiGameState& State, float PhaseSecondsRemaining);
}
