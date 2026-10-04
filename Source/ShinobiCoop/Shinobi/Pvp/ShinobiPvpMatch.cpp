#include "Shinobi/Pvp/ShinobiPvpMatch.h"

int32 FShinobiPvpMatch::SlotsPerTeam(EShinobiPvpFormat Format)
{
	switch (Format)
	{
	case EShinobiPvpFormat::OneVsOne: return 1;
	case EShinobiPvpFormat::TwoVsTwo: return 2;
	default: return 0;
	}
}

TOptional<EShinobiTeam> FShinobiPvpMatch::TeamForJoin(EShinobiPvpFormat Format, int32 HumansA, int32 HumansB)
{
	const int32 Slots = SlotsPerTeam(Format);
	const bool bAOpen = HumansA < Slots;
	const bool bBOpen = HumansB < Slots;
	if (!bAOpen && !bBOpen)
	{
		return TOptional<EShinobiTeam>();
	}
	if (bAOpen && (!bBOpen || HumansA <= HumansB))
	{
		return EShinobiTeam::TeamA;
	}
	return EShinobiTeam::TeamB;
}

EShinobiRoundResult FShinobiPvpMatch::Judge(const FShinobiTeamStatus& A, const FShinobiTeamStatus& B, bool bTimeUp)
{
	if (A.Alive <= 0 && B.Alive <= 0)
	{
		return EShinobiRoundResult::Tie;
	}
	if (A.Alive <= 0)
	{
		return EShinobiRoundResult::TeamBWins;
	}
	if (B.Alive <= 0)
	{
		return EShinobiRoundResult::TeamAWins;
	}
	if (!bTimeUp)
	{
		return EShinobiRoundResult::Ongoing;
	}
	if (A.HealthFraction > B.HealthFraction)
	{
		return EShinobiRoundResult::TeamAWins;
	}
	return A.HealthFraction < B.HealthFraction ? EShinobiRoundResult::TeamBWins : EShinobiRoundResult::Tie;
}

void FShinobiPvpMatch::Record(EShinobiRoundResult Result)
{
	switch (Result)
	{
	case EShinobiRoundResult::TeamAWins:
		++WinsA;
		++Round;
		ConsecutiveTies = 0;
		break;
	case EShinobiRoundResult::TeamBWins:
		++WinsB;
		++Round;
		ConsecutiveTies = 0;
		break;
	case EShinobiRoundResult::Tie:
		++ConsecutiveTies;
		break;
	default:
		break;
	}
}

bool FShinobiPvpMatch::IsOver() const
{
	return WinsA >= RoundsToWin || WinsB >= RoundsToWin || ConsecutiveTies >= 2;
}

EShinobiRoundResult FShinobiPvpMatch::MatchResult() const
{
	if (WinsA >= RoundsToWin)
	{
		return EShinobiRoundResult::TeamAWins;
	}
	if (WinsB >= RoundsToWin)
	{
		return EShinobiRoundResult::TeamBWins;
	}
	return ConsecutiveTies >= 2 ? EShinobiRoundResult::Tie : EShinobiRoundResult::Ongoing;
}
