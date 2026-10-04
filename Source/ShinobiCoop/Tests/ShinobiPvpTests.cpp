#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Pvp/ShinobiPvpGameMode.h"
#include "Shinobi/Pvp/ShinobiPvpMatch.h"
#include "Shinobi/Abilities/GA_Revive.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTeamsHostileTest, "ShinobiCoop.Pvp.TeamsAreHostileAcrossAAndB", ShinobiTestFlags)
bool FShinobiPvpTeamsHostileTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* A2 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 500.f, 0.f));
	AShinobiPlayerCharacter* B1 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, -500.f, 0.f));
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(5000.f, 0.f, 0.f));
	A1->SetTeam(EShinobiTeam::TeamA);
	A2->SetTeam(EShinobiTeam::TeamA);
	B1->SetTeam(EShinobiTeam::TeamB);

	TestTrue(TEXT("A is hostile to B"), A1->IsHostileTo(B1));
	TestTrue(TEXT("B is hostile to A"), B1->IsHostileTo(A1));
	TestFalse(TEXT("Teammates are not hostile"), A1->IsHostileTo(A2) || A2->IsHostileTo(A1));
	TestTrue(TEXT("Team A is hostile to enemies"), A1->IsHostileTo(Ninja) && Ninja->IsHostileTo(A1));
	TestTrue(TEXT("Only the arena teams are pvp teams"), IsPvpTeam(EShinobiTeam::TeamA) && IsPvpTeam(EShinobiTeam::TeamB)
		&& !IsPvpTeam(EShinobiTeam::Players) && !IsPvpTeam(EShinobiTeam::Enemies));
	TestEqual(TEXT("A player starts on the players' side"), W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 1000.f, 0.f))->GetTeam(), EShinobiTeam::Players);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpAreaTest, "ShinobiCoop.Pvp.AreaJutsuHitsOpposingTeamOnly", ShinobiTestFlags)
bool FShinobiPvpAreaTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* A2 = W.Spawn<AShinobiPlayerCharacter>(FVector(200.f, 0.f, 0.f));
	AShinobiPlayerCharacter* B1 = W.Spawn<AShinobiPlayerCharacter>(FVector(-200.f, 0.f, 0.f));
	A1->SetTeam(EShinobiTeam::TeamA);
	A2->SetTeam(EShinobiTeam::TeamA);
	B1->SetTeam(EShinobiTeam::TeamB);
	ShinobiTest::Hover(A1);
	ShinobiTest::Hover(A2);
	ShinobiTest::Hover(B1);

	const TArray<AShinobiCharacterBase*> Hit = ShinobiCombat::FindHostilesInRadius(A1, FVector::ZeroVector, 1000.f);
	TestTrue(TEXT("The opposing player is hit"), Hit.Contains(B1));
	TestFalse(TEXT("The ally is spared"), Hit.Contains(A2));
	TestFalse(TEXT("The caster is spared"), Hit.Contains(A1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpSummonTeamTest, "ShinobiCoop.Pvp.SummonInheritsCasterTeam", ShinobiTestFlags)
bool FShinobiPvpSummonTeamTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Foe = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 3000.f, 0.f));
	Caster->SetTeam(EShinobiTeam::TeamB);
	Foe->SetTeam(EShinobiTeam::TeamA);
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Foe);

	const TArray<AShinobiSummon*> Pack = AShinobiSummon::Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	TestEqual(TEXT("One toad"), Pack.Num(), 1);
	if (Pack.Num() != 1)
	{
		return false;
	}
	ShinobiTest::Hover(Pack[0]);
	TestEqual(TEXT("The toad fights for team B"), Pack[0]->GetTeam(), EShinobiTeam::TeamB);
	TestTrue(TEXT("A team A player is hostile to it"), Foe->IsHostileTo(Pack[0]) && Pack[0]->IsHostileTo(Foe));
	TestFalse(TEXT("Its own caster is not"), Pack[0]->IsHostileTo(Caster));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpOutlineTest, "ShinobiCoop.Pvp.OutlineFollowsTheViewersSide", ShinobiTestFlags)
bool FShinobiPvpOutlineTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* B1 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 500.f, 0.f));
	A1->SetTeam(EShinobiTeam::TeamA);
	B1->SetTeam(EShinobiTeam::TeamB);

	// No local player controller in the test world: the old static rule (only Enemies is red) holds.
	TestEqual(TEXT("Without a viewer a team B player is friendly"), B1->GetBodyMesh()->CustomDepthStencilValue, int32(ShinobiLook::StencilFriendly));
	TestEqual(TEXT("Without a viewer a team A player is friendly"), A1->GetBodyMesh()->CustomDepthStencilValue, int32(ShinobiLook::StencilFriendly));

	// With a viewer, the rule is hostility.
	A1->ApplyOutlineFor(A1);
	B1->ApplyOutlineFor(A1);
	TestEqual(TEXT("The viewer's own side is friendly"), A1->GetBodyMesh()->CustomDepthStencilValue, int32(ShinobiLook::StencilFriendly));
	TestEqual(TEXT("The opposing side is outlined as an enemy"), B1->GetBodyMesh()->CustomDepthStencilValue, int32(ShinobiLook::StencilEnemy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpJudgeWipeTest, "ShinobiCoop.Pvp.Match.JudgeWinsWhenOneSideWiped", ShinobiTestFlags)
bool FShinobiPvpJudgeWipeTest::RunTest(const FString& Parameters)
{
	const FShinobiTeamStatus Dead{0, 0.f};
	const FShinobiTeamStatus Alive{1, 0.5f};
	TestEqual(TEXT("A wiped: B wins"), FShinobiPvpMatch::Judge(Dead, Alive, false), EShinobiRoundResult::TeamBWins);
	TestEqual(TEXT("B wiped: A wins"), FShinobiPvpMatch::Judge(Alive, Dead, false), EShinobiRoundResult::TeamAWins);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpJudgeTradeTest, "ShinobiCoop.Pvp.Match.JudgeSimultaneousWipeIsTie", ShinobiTestFlags)
bool FShinobiPvpJudgeTradeTest::RunTest(const FString& Parameters)
{
	const FShinobiTeamStatus Dead{0, 0.f};
	TestEqual(TEXT("Both wiped is a tie"), FShinobiPvpMatch::Judge(Dead, Dead, false), EShinobiRoundResult::Tie);
	TestEqual(TEXT("Both wiped at time up is still a tie"), FShinobiPvpMatch::Judge(Dead, Dead, true), EShinobiRoundResult::Tie);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpJudgeTimeUpTest, "ShinobiCoop.Pvp.Match.JudgeOngoingUntilTimeUp", ShinobiTestFlags)
bool FShinobiPvpJudgeTimeUpTest::RunTest(const FString& Parameters)
{
	const FShinobiTeamStatus Healthy{1, 0.9f};
	const FShinobiTeamStatus Hurt{1, 0.2f};
	TestEqual(TEXT("Both fighting before the cap"), FShinobiPvpMatch::Judge(Healthy, Hurt, false), EShinobiRoundResult::Ongoing);
	TestEqual(TEXT("Time up: more health wins"), FShinobiPvpMatch::Judge(Healthy, Hurt, true), EShinobiRoundResult::TeamAWins);
	TestEqual(TEXT("Time up mirrored"), FShinobiPvpMatch::Judge(Hurt, Healthy, true), EShinobiRoundResult::TeamBWins);
	TestEqual(TEXT("Time up with equal health is a tie"), FShinobiPvpMatch::Judge(Healthy, Healthy, true), EShinobiRoundResult::Tie);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpRecordFlowTest, "ShinobiCoop.Pvp.Match.RecordFlow", ShinobiTestFlags)
bool FShinobiPvpRecordFlowTest::RunTest(const FString& Parameters)
{
	FShinobiPvpMatch M;
	M.Record(EShinobiRoundResult::Ongoing);
	TestEqual(TEXT("Ongoing is ignored"), M.Round, 1);
	M.Record(EShinobiRoundResult::TeamAWins);
	TestEqual(TEXT("Round 2 after A wins"), M.Round, 2);
	TestFalse(TEXT("Not over at 1-0"), M.IsOver());
	TestEqual(TEXT("No result yet"), M.MatchResult(), EShinobiRoundResult::Ongoing);
	M.Record(EShinobiRoundResult::TeamBWins);
	TestEqual(TEXT("Round 3 after B wins"), M.Round, 3);
	TestFalse(TEXT("Not over at 1-1"), M.IsOver());
	M.Record(EShinobiRoundResult::TeamAWins);
	TestEqual(TEXT("A has two wins"), M.WinsA, 2);
	TestTrue(TEXT("Over at 2-1"), M.IsOver());
	TestEqual(TEXT("A wins the match"), M.MatchResult(), EShinobiRoundResult::TeamAWins);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTwoTiesTest, "ShinobiCoop.Pvp.Match.TwoTiesInARowIsADraw", ShinobiTestFlags)
bool FShinobiPvpTwoTiesTest::RunTest(const FString& Parameters)
{
	FShinobiPvpMatch Draw;
	Draw.Record(EShinobiRoundResult::Tie);
	TestFalse(TEXT("One tie replays the round"), Draw.IsOver());
	TestEqual(TEXT("A tie awards no round"), Draw.Round, 1);
	Draw.Record(EShinobiRoundResult::Tie);
	TestTrue(TEXT("Two ties end the match"), Draw.IsOver());
	TestEqual(TEXT("As a draw"), Draw.MatchResult(), EShinobiRoundResult::Tie);

	FShinobiPvpMatch Broken;
	Broken.Record(EShinobiRoundResult::Tie);
	Broken.Record(EShinobiRoundResult::TeamAWins);
	Broken.Record(EShinobiRoundResult::Tie);
	TestFalse(TEXT("A win in between resets the tie streak"), Broken.IsOver());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTeamForJoinTest, "ShinobiCoop.Pvp.Match.TeamForJoinFillsBalanced", ShinobiTestFlags)
bool FShinobiPvpTeamForJoinTest::RunTest(const FString& Parameters)
{
	using M = FShinobiPvpMatch;
	const EShinobiPvpFormat Two = EShinobiPvpFormat::TwoVsTwo;
	TestEqual(TEXT("2v2 empty -> A"), M::TeamForJoin(Two, 0, 0).Get(EShinobiTeam::Enemies), EShinobiTeam::TeamA);
	TestEqual(TEXT("2v2 1-0 -> B"), M::TeamForJoin(Two, 1, 0).Get(EShinobiTeam::Enemies), EShinobiTeam::TeamB);
	TestEqual(TEXT("2v2 1-1 -> A"), M::TeamForJoin(Two, 1, 1).Get(EShinobiTeam::Enemies), EShinobiTeam::TeamA);
	TestEqual(TEXT("2v2 2-1 -> B"), M::TeamForJoin(Two, 2, 1).Get(EShinobiTeam::Enemies), EShinobiTeam::TeamB);
	TestFalse(TEXT("2v2 full -> unset"), M::TeamForJoin(Two, 2, 2).IsSet());
	TestFalse(TEXT("1v1 full -> unset"), M::TeamForJoin(EShinobiPvpFormat::OneVsOne, 1, 1).IsSet());
	TestEqual(TEXT("1v1 slots"), M::SlotsPerTeam(EShinobiPvpFormat::OneVsOne), 1);
	TestEqual(TEXT("2v2 slots"), M::SlotsPerTeam(Two), 2);
	TestEqual(TEXT("None slots"), M::SlotsPerTeam(EShinobiPvpFormat::None), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpGatherStatusTest, "ShinobiCoop.Pvp.Mode.GatherStatusCountsDestroyedPawnAsDead", ShinobiTestFlags)
bool FShinobiPvpGatherStatusTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
	Mode->PvpFormat = EShinobiPvpFormat::TwoVsTwo;
	AShinobiPlayerCharacter* A1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* A2 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 500.f, 0.f));
	AShinobiPlayerCharacter* B1 = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, -3000.f, 0.f));
	A1->SetTeam(EShinobiTeam::TeamA);
	A2->SetTeam(EShinobiTeam::TeamA);
	B1->SetTeam(EShinobiTeam::TeamB);
	ShinobiTest::Hover(A1);
	ShinobiTest::Hover(A2);
	ShinobiTest::Hover(B1);
	// A team A summon shares the side but is not a fighter slot.
	const TArray<AShinobiSummon*> Toad = AShinobiSummon::Summon(A2, EShinobiSummonKind::Toad, FVector(0.f, 900.f, 0.f));
	TestEqual(TEXT("A toad on team A"), Toad.Num(), 1);

	const FShinobiTeamStatus Full = Mode->GatherStatus(EShinobiTeam::TeamA);
	TestEqual(TEXT("Both team A players alive (the toad is not counted)"), Full.Alive, 2);
	TestEqual(TEXT("Full health"), Full.HealthFraction, 1.f, 0.001f);

	const float MaxHealth = A2->GetAttributes()->GetMaxHealth();
	ShinobiTest::Set(A2, UShinobiAttributeSet::GetHealthAttribute(), MaxHealth * 0.5f);
	A1->Destroy(); // a human who left, or fell out of the world
	const FShinobiTeamStatus Status = Mode->GatherStatus(EShinobiTeam::TeamA);
	TestEqual(TEXT("The destroyed pawn counts as dead"), Status.Alive, 1);
	TestEqual(TEXT("Half the team's slots at half health"), Status.HealthFraction, 0.5f * 0.5f, 0.001f);
	TestEqual(TEXT("Team B is apart"), Mode->GatherStatus(EShinobiTeam::TeamB).Alive, 1);

	B1->Destroy();
	const FShinobiTeamStatus Empty = Mode->GatherStatus(EShinobiTeam::TeamB);
	TestEqual(TEXT("An empty team has nobody alive"), Empty.Alive, 0);
	TestEqual(TEXT("And no health"), Empty.HealthFraction, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpResetTest, "ShinobiCoop.Pvp.Mode.ResetClearsBoostsAndShield", ShinobiTestFlags)
bool FShinobiPvpResetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
	Mode->PvpFormat = EShinobiPvpFormat::OneVsOne;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	P->SetTeam(EShinobiTeam::TeamA);
	ShinobiTest::Hover(P);

	ShinobiTest::ApplyEffect(P, UGE_PvpHold::StaticClass());
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	ShinobiTest::ApplyEffect(P, UGE_Mounted::StaticClass());
	const float MaxHealth = P->GetAttributes()->GetMaxHealth();
	ShinobiTest::Set(P, UShinobiAttributeSet::GetHealthAttribute(), MaxHealth * 0.4f);
	TestTrue(TEXT("Held"), ShinobiTest::HasTag(P, ShinobiTags::State_PvpHold));
	TestFalse(TEXT("A held fighter can't move"), P->CanMove());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Nor act"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	ShinobiTest::Release(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Susanoo up"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));

	Mode->ResetCharacters();
	TestFalse(TEXT("Hold cleared"), ShinobiTest::HasTag(P, ShinobiTags::State_PvpHold));
	TestFalse(TEXT("Susanoo cleared"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	TestFalse(TEXT("Dismounted"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	TestEqual(TEXT("Health full"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), MaxHealth);
	TestTrue(TEXT("Free to move again"), P->CanMove());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpReviveOffTest, "ShinobiCoop.Pvp.Mode.ReviveIsOffInPvp", ShinobiTestFlags)
bool FShinobiPvpReviveOffTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiGameState* State = W.Spawn<AShinobiGameState>();
	W.World->SetGameState(State);
	AShinobiPlayerCharacter* Reviver = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Downed = W.Spawn<AShinobiPlayerCharacter>(FVector(100.f, 0.f, 0.f));
	Reviver->SetTeam(EShinobiTeam::TeamA);
	Downed->SetTeam(EShinobiTeam::TeamA);
	ShinobiTest::Hover(Reviver);
	ShinobiTest::Hover(Downed);
	ShinobiTest::ApplyEffect(Downed, UGE_Downed::StaticClass());

	TestTrue(TEXT("Outside a match the downed ally is found"), UGA_Revive::FindDownedAllyFor(*Reviver) == Downed);
	State->PvpPhase = EShinobiPvpPhase::Fighting;
	TestTrue(TEXT("The game state is in a match"), State->IsPvp());
	TestNull(TEXT("In a match nobody is revived"), UGA_Revive::FindDownedAllyFor(*Reviver));
	return true;
}

#endif
