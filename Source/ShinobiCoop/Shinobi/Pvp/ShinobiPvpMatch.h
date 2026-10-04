#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"

/** How many fighters each side fields in a PvP arena match. */
enum class EShinobiPvpFormat : uint8 { None, OneVsOne, TwoVsTwo };

enum class EShinobiPvpPhase : uint8 { None, Warmup, Countdown, Fighting, RoundEnd, MatchEnd };

enum class EShinobiRoundResult : uint8 { Ongoing, TeamAWins, TeamBWins, Tie };

/** One side's standing in a round. HealthFraction = sum HP / sum max HP of the team's slots (a missing or dead slot counts 0). */
struct FShinobiTeamStatus
{
	int32 Alive = 0;
	float HealthFraction = 0.f;
};

/**
 * Pure rules of a PvP arena match: timings, best of three, round judging and team balancing. No world, no UObjects,
 * so the game mode drives it and the tests run it directly.
 */
struct SHINOBICOOP_API FShinobiPvpMatch
{
	static constexpr float WarmupSeconds = 15.f;
	static constexpr float CountdownSeconds = 3.f;
	static constexpr float RoundSeconds = 120.f;
	static constexpr float RoundEndSeconds = 5.f;
	static constexpr float ResultSeconds = 10.f;
	static constexpr int32 RoundsToWin = 2;

	/** 1 or 2 fighters per side; 0 for None. */
	static int32 SlotsPerTeam(EShinobiPvpFormat Format);

	/** The team a joining human should take: the one with fewer humans, ties to A; unset when both teams are full. */
	static TOptional<EShinobiTeam> TeamForJoin(EShinobiPvpFormat Format, int32 HumansA, int32 HumansB);

	/** A wiped side loses; both wiped is a tie. Once bTimeUp, the higher team health fraction wins (equal is a tie). */
	static EShinobiRoundResult Judge(const FShinobiTeamStatus& A, const FShinobiTeamStatus& B, bool bTimeUp);

	int32 Round = 1;
	int32 WinsA = 0;
	int32 WinsB = 0;
	/** Ties since the last decided round. */
	int32 ConsecutiveTies = 0;

	/** Ongoing is ignored; a win bumps that side's wins and the round and resets the tie streak; a tie replays the round (no point). */
	void Record(EShinobiRoundResult Result);

	/** A team has RoundsToWin, or two ties in a row. */
	bool IsOver() const;

	/** TeamAWins / TeamBWins, Tie for a draw, Ongoing while the match is still being played. */
	EShinobiRoundResult MatchResult() const;
};
