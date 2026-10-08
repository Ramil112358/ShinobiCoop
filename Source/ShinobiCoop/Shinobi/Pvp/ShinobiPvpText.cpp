#include "Shinobi/Pvp/ShinobiPvpText.h"
#include "Shinobi/ShinobiGameState.h"

namespace
{
	/** Whether the result is a win for LocalTeam; Tie and Ongoing are neither. */
	bool IsWinFor(EShinobiRoundResult Result, EShinobiTeam LocalTeam)
	{
		return (Result == EShinobiRoundResult::TeamAWins && LocalTeam == EShinobiTeam::TeamA)
			|| (Result == EShinobiRoundResult::TeamBWins && LocalTeam == EShinobiTeam::TeamB);
	}
}

FString ShinobiPvp::BannerText(const AShinobiGameState& State, EShinobiTeam LocalTeam, float PhaseSecondsRemaining)
{
	const EShinobiRoundResult Result = State.PvpRoundResult;
	switch (State.PvpPhase)
	{
	case EShinobiPvpPhase::Warmup:
		return TEXT("WARMUP");
	case EShinobiPvpPhase::Countdown:
		return FString::FromInt(FMath::Max(1, FMath::CeilToInt(PhaseSecondsRemaining)));
	case EShinobiPvpPhase::Fighting:
		return FShinobiPvpMatch::RoundSeconds - PhaseSecondsRemaining < 1.f ? TEXT("FIGHT") : TEXT("");
	case EShinobiPvpPhase::RoundEnd:
		return Result == EShinobiRoundResult::Tie ? TEXT("ROUND DRAWN") : IsWinFor(Result, LocalTeam) ? TEXT("ROUND WON") : TEXT("ROUND LOST");
	case EShinobiPvpPhase::MatchEnd:
		return Result == EShinobiRoundResult::Tie ? TEXT("DRAW") : IsWinFor(Result, LocalTeam) ? TEXT("VICTORY") : TEXT("DEFEAT");
	default:
		return FString();
	}
}

FString ShinobiPvp::ScoreText(const AShinobiGameState& State, float PhaseSecondsRemaining)
{
	FString Timer = TEXT("--:--");
	if (State.PvpPhase == EShinobiPvpPhase::Fighting)
	{
		const int32 Seconds = FMath::Max(0, FMath::CeilToInt(PhaseSecondsRemaining));
		Timer = FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60);
	}
	return FString::Printf(TEXT("A %d  -  Round %d  -  B %d  -  %s"), State.PvpWinsA, State.PvpRound, State.PvpWinsB, *Timer);
}
