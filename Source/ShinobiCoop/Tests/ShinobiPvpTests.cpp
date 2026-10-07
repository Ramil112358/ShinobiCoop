#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Pvp/ShinobiPickup.h"
#include "Shinobi/Pvp/ShinobiPvpGameMode.h"
#include "Shinobi/Pvp/ShinobiPvpMatch.h"
#include "Shinobi/Pvp/ShinobiPvpText.h"
#include "Shinobi/Abilities/GA_Revive.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiOverheadTextComponent.h"
#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerStart.h"

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

	// Downed is out, whatever health it still shows (here full).
	ShinobiTest::ApplyEffect(A1, UGE_Downed::StaticClass());
	const FShinobiTeamStatus OneDown = Mode->GatherStatus(EShinobiTeam::TeamA);
	TestEqual(TEXT("The downed player is not alive"), OneDown.Alive, 1);
	TestEqual(TEXT("And adds no health"), OneDown.HealthFraction, 0.5f, 0.001f);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpKillTeamTest, "ShinobiCoop.Pvp.Mode.KillTeamDownsOnlyThatSide", ShinobiTestFlags)
bool FShinobiPvpKillTeamTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
	Mode->PvpFormat = EShinobiPvpFormat::OneVsOne;
	AShinobiPlayerCharacter* A = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* B = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, -3000.f, 0.f));
	A->SetTeam(EShinobiTeam::TeamA);
	B->SetTeam(EShinobiTeam::TeamB);
	ShinobiTest::Hover(A);
	ShinobiTest::Hover(B);

	Mode->KillTeam(EShinobiTeam::TeamB);
	TestEqual(TEXT("Team B is out"), Mode->GatherStatus(EShinobiTeam::TeamB).Alive, 0);
	TestEqual(TEXT("Team A is untouched"), Mode->GatherStatus(EShinobiTeam::TeamA).Alive, 1);
	TestEqual(TEXT("The round judge sees team A the winner"),
		FShinobiPvpMatch::Judge(Mode->GatherStatus(EShinobiTeam::TeamA), Mode->GatherStatus(EShinobiTeam::TeamB), false), EShinobiRoundResult::TeamAWins);
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

	for (const EShinobiPickupType Type : { EShinobiPickupType::Attack, EShinobiPickupType::Speed, EShinobiPickupType::Shield })
	{
		AShinobiPickup::Apply(*P, Type);
	}
	TestTrue(TEXT("Boosted and shielded"), ShinobiTest::HasTag(P, ShinobiTags::State_AttackBoost) && ShinobiTest::HasTag(P, ShinobiTags::State_SpeedBoost)
		&& ShinobiTest::HasTag(P, ShinobiTags::State_Shield));

	Mode->ResetCharacters();
	TestFalse(TEXT("Hold cleared"), ShinobiTest::HasTag(P, ShinobiTags::State_PvpHold));
	TestFalse(TEXT("Susanoo cleared"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	TestFalse(TEXT("Dismounted"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	TestFalse(TEXT("Boosts cleared"), ShinobiTest::HasTag(P, ShinobiTags::State_AttackBoost) || ShinobiTest::HasTag(P, ShinobiTags::State_SpeedBoost));
	TestFalse(TEXT("Shield cleared"), ShinobiTest::HasTag(P, ShinobiTags::State_Shield));
	TestEqual(TEXT("No shield left over"), ShinobiTest::Get(P, UShinobiAttributeSet::GetShieldAttribute()), 0.f);
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

namespace
{
	APlayerStart* SpawnStart(FShinobiTestWorld& W, const FVector& Where, FName Tag)
	{
		APlayerStart* Start = W.Spawn<APlayerStart>(Where);
		if (!Tag.IsNone())
		{
			Start->Tags.Add(Tag);
		}
		return Start;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpStartsTest, "ShinobiCoop.Pvp.Mode.PlayersSpawnAtTheirSidesStarts", ShinobiTestFlags)
bool FShinobiPvpStartsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
	Mode->PvpFormat = EShinobiPvpFormat::TwoVsTwo;
	APlayerStart* StartB = SpawnStart(W, FVector(0.f, -5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagB);
	APlayerStart* StartA = SpawnStart(W, FVector(0.f, 5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagA);

	TestEqual(TEXT("One team A start"), Mode->GetTeamStarts(EShinobiTeam::TeamA).Num(), 1);
	TestTrue(TEXT("It is the tagged one"), Mode->GetTeamStarts(EShinobiTeam::TeamA).Contains(StartA));
	TestTrue(TEXT("Team B's is its own"), Mode->GetTeamStarts(EShinobiTeam::TeamB).Contains(StartB));
	TestEqual(TEXT("No starts for a side-less team"), Mode->GetTeamStarts(EShinobiTeam::Players).Num(), 0);

	AAIController* First = W.Spawn<AAIController>();
	AAIController* Second = W.Spawn<AAIController>();
	// Login caches a start before the side is known (here team B's for the team A joiner): it must not stick.
	First->StartSpot = StartB;
	Mode->AssignTeam(First);
	Mode->AssignTeam(Second);
	TestFalse(TEXT("The start Login cached is not reused"), Mode->ShouldSpawnAtStartSpot(First));
	TestTrue(TEXT("A spawn (FindPlayerStart) picks team A's start"), Mode->FindPlayerStart(First) == StartA);
	TestEqual(TEXT("First joiner on A"), Mode->GetTeamOf(First), EShinobiTeam::TeamA);
	TestEqual(TEXT("Second on B"), Mode->GetTeamOf(Second), EShinobiTeam::TeamB);
	TestTrue(TEXT("A spawns at the team A start"), Mode->ChoosePlayerStart(First) == StartA);
	TestTrue(TEXT("B spawns at the team B start"), Mode->ChoosePlayerStart(Second) == StartB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpFallTest, "ShinobiCoop.Pvp.Mode.FallingOutOfTheWorldIsDeathInARound", ShinobiTestFlags)
bool FShinobiPvpFallTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>(); // also spawns and registers the game state
	Mode->PvpFormat = EShinobiPvpFormat::TwoVsTwo;
	AShinobiGameState* State = Mode->GetGameState<AShinobiGameState>();
	TestNotNull(TEXT("The mode brought its game state"), State);
	if (!State)
	{
		return false;
	}
	SpawnStart(W, FVector(0.f, -5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagB);
	APlayerStart* StartA = SpawnStart(W, FVector(0.f, 5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagA);
	AShinobiPlayerCharacter* A1 = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* A2 = W.Spawn<AShinobiPlayerCharacter>(FVector(500.f, 0.f, 0.f));
	A1->SetTeam(EShinobiTeam::TeamA);
	A2->SetTeam(EShinobiTeam::TeamA);
	ShinobiTest::Hover(A1);
	ShinobiTest::Hover(A2);

	// Warmup: back to the faller's own side.
	State->PvpPhase = EShinobiPvpPhase::Warmup;
	static_cast<AActor*>(A2)->FellOutOfWorld(*GetDefault<UDamageType>()); // public on AActor
	TestTrue(TEXT("In warmup the faller survives"), IsValid(A2));
	TestTrue(TEXT("At its own team's start"), FVector::Dist2D(A2->GetActorLocation(), StartA->GetActorLocation()) < 1.f);

	// A round: out for it.
	State->PvpPhase = EShinobiPvpPhase::Fighting;
	TestEqual(TEXT("Both standing"), Mode->GatherStatus(EShinobiTeam::TeamA).Alive, 2);
	static_cast<AActor*>(A1)->FellOutOfWorld(*GetDefault<UDamageType>()); // public on AActor
	TestEqual(TEXT("The faller is dead for the round"), Mode->GatherStatus(EShinobiTeam::TeamA).Alive, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpFallPveTest, "ShinobiCoop.Pvp.Mode.FallingOutOfTheWorldOutsidePvpTeleports", ShinobiTestFlags)
bool FShinobiPvpFallPveTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiGameState* State = W.Spawn<AShinobiGameState>();
	W.World->SetGameState(State);
	APlayerStart* Start = SpawnStart(W, FVector(3000.f, 0.f, 0.f), NAME_None);
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, -5000.f));
	ShinobiTest::Hover(P);

	static_cast<AActor*>(P)->FellOutOfWorld(*GetDefault<UDamageType>()); // public on AActor
	TestTrue(TEXT("Co-op: the faller survives"), IsValid(P) && !P->IsActorBeingDestroyed());
	TestTrue(TEXT("Back at the PlayerStart"), FVector::Dist2D(P->GetActorLocation(), Start->GetActorLocation()) < 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTextCountdownTest, "ShinobiCoop.Pvp.Text.CountdownBannerIsTheCeilingOfTheTimeLeft", ShinobiTestFlags)
bool FShinobiPvpTextCountdownTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiGameState* State = W.Spawn<AShinobiGameState>();
	State->PvpPhase = EShinobiPvpPhase::Countdown;
	TestEqual(TEXT("2.4 s left shows 3"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, 2.4f), FString(TEXT("3")));
	TestEqual(TEXT("0.5 s left shows 1"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, 0.5f), FString(TEXT("1")));
	TestEqual(TEXT("3 s left shows 3"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamB, 3.f), FString(TEXT("3")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTextFightTest, "ShinobiCoop.Pvp.Text.FightShowsForTheFirstSecondOnly", ShinobiTestFlags)
bool FShinobiPvpTextFightTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiGameState* State = W.Spawn<AShinobiGameState>();
	State->PvpPhase = EShinobiPvpPhase::Fighting;
	TestEqual(TEXT("The round just began"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, FShinobiPvpMatch::RoundSeconds - 0.4f), FString(TEXT("FIGHT")));
	TestEqual(TEXT("A second in, nothing"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, FShinobiPvpMatch::RoundSeconds - 1.5f), FString());
	TestEqual(TEXT("Mid-round, nothing"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, 60.f), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTextResultTest, "ShinobiCoop.Pvp.Text.RoundAndMatchBannersReadFromTheLocalSide", ShinobiTestFlags)
bool FShinobiPvpTextResultTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiGameState* State = W.Spawn<AShinobiGameState>();
	State->PvpPhase = EShinobiPvpPhase::RoundEnd;
	State->PvpRoundResult = EShinobiRoundResult::TeamAWins;
	TestEqual(TEXT("A won, seen from A"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, 3.f), FString(TEXT("ROUND WON")));
	TestEqual(TEXT("A won, seen from B"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamB, 3.f), FString(TEXT("ROUND LOST")));
	State->PvpRoundResult = EShinobiRoundResult::TeamBWins;
	TestEqual(TEXT("B won, seen from B"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamB, 3.f), FString(TEXT("ROUND WON")));
	State->PvpRoundResult = EShinobiRoundResult::Tie;
	TestEqual(TEXT("A tied round"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, 3.f), FString(TEXT("ROUND DRAWN")));

	State->PvpPhase = EShinobiPvpPhase::MatchEnd;
	State->PvpRoundResult = EShinobiRoundResult::TeamAWins;
	TestEqual(TEXT("Match won"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, 8.f), FString(TEXT("VICTORY")));
	TestEqual(TEXT("Match lost"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamB, 8.f), FString(TEXT("DEFEAT")));
	State->PvpRoundResult = EShinobiRoundResult::Tie;
	TestEqual(TEXT("Match drawn"), ShinobiPvp::BannerText(*State, EShinobiTeam::TeamA, 8.f), FString(TEXT("DRAW")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTextScoreTest, "ShinobiCoop.Pvp.Text.ScoreStripShowsWinsRoundAndTimer", ShinobiTestFlags)
bool FShinobiPvpTextScoreTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiGameState* State = W.Spawn<AShinobiGameState>();
	State->PvpPhase = EShinobiPvpPhase::Fighting;
	State->PvpRound = 2;
	State->PvpWinsA = 1;
	State->PvpWinsB = 0;
	TestEqual(TEXT("Round 2, 1-0, 102 s left"), ShinobiPvp::ScoreText(*State, 102.f), FString(TEXT("A 1  -  Round 2  -  B 0  -  1:42")));
	TestEqual(TEXT("Seconds are padded"), ShinobiPvp::ScoreText(*State, 65.f), FString(TEXT("A 1  -  Round 2  -  B 0  -  1:05")));
	State->PvpPhase = EShinobiPvpPhase::Countdown;
	TestEqual(TEXT("No timer outside the fight"), ShinobiPvp::ScoreText(*State, 2.f), FString(TEXT("A 1  -  Round 2  -  B 0  -  --:--")));
	return true;
}

namespace
{
	/** What the HUD's boost list shows: the longest time left on an effect granting Tag. */
	float BoostSecondsLeft(const AShinobiCharacterBase* Character, const FGameplayTag& Tag)
	{
		float Left = 0.f;
		for (const float Seconds : Character->GetAbilitySystemComponent()->GetActiveEffectsTimeRemaining(
			FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag))))
		{
			Left = FMath::Max(Left, Seconds);
		}
		return Left;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickupHealthTest, "ShinobiCoop.Pvp.Pickup.HealthRestoresFortyPercentAndClamps", ShinobiTestFlags)
bool FShinobiPickupHealthTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const float MaxHealth = P->GetAttributes()->GetMaxHealth();

	ShinobiTest::Set(P, Health, MaxHealth * 0.5f);
	TestTrue(TEXT("Taken"), AShinobiPickup::Apply(*P, EShinobiPickupType::Health));
	TestEqual(TEXT("Half health becomes 90%"), ShinobiTest::Get(P, Health), MaxHealth * 0.9f, 0.01f);
	ShinobiTest::Set(P, Health, MaxHealth * 0.8f);
	AShinobiPickup::Apply(*P, EShinobiPickupType::Health);
	TestEqual(TEXT("80% is capped at full"), ShinobiTest::Get(P, Health), MaxHealth, 0.01f);

	ShinobiTest::Set(P, Health, MaxHealth * 0.1f);
	ShinobiTest::ApplyEffect(P, UGE_Downed::StaticClass());
	TestFalse(TEXT("The downed can't take it"), AShinobiPickup::Apply(*P, EShinobiPickupType::Health));
	TestEqual(TEXT("And are not healed"), ShinobiTest::Get(P, Health), MaxHealth * 0.1f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickupAttackTest, "ShinobiCoop.Pvp.Pickup.AttackBoostScalesOutgoingDamage", ShinobiTestFlags)
bool FShinobiPickupAttackTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Target = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(Target);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	TestTrue(TEXT("Taken"), AShinobiPickup::Apply(*P, EShinobiPickupType::Attack));
	TestTrue(TEXT("Boosted"), ShinobiTest::HasTag(P, ShinobiTags::State_AttackBoost));
	TestEqual(TEXT("The HUD shows 15 s left"), BoostSecondsLeft(P, ShinobiTags::State_AttackBoost), UGE_AttackBoost::Seconds, 0.1f);
	ShinobiCombat::ApplyDamage(P->GetAbilitySystemComponent(), Target, 100.f);
	TestEqual(TEXT("100 hits for 150"), ShinobiTest::Get(Target, Health), 350.f);
	ShinobiCombat::ApplyDamage(Target->GetAbilitySystemComponent(), P, 100.f);
	TestEqual(TEXT("Damage taken is not boosted"), ShinobiTest::Get(P, Health), 400.f);

	W.Tick(15.1f);
	TestFalse(TEXT("Over after 15 s"), ShinobiTest::HasTag(P, ShinobiTags::State_AttackBoost));
	ShinobiCombat::ApplyDamage(P->GetAbilitySystemComponent(), Target, 100.f);
	TestEqual(TEXT("Then 100 is 100"), ShinobiTest::Get(Target, Health), 250.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickupSpeedTest, "ShinobiCoop.Pvp.Pickup.SpeedBoostScalesMaxSpeed", ShinobiTestFlags)
bool FShinobiPickupSpeedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	const float Base = P->GetCharacterMovement()->GetMaxSpeed();
	TestTrue(TEXT("A speed to scale"), Base > 0.f);

	TestTrue(TEXT("Taken"), AShinobiPickup::Apply(*P, EShinobiPickupType::Speed));
	TestEqual(TEXT("35% faster"), P->GetCharacterMovement()->GetMaxSpeed(), Base * 1.35f, 0.01f);
	TestEqual(TEXT("The HUD shows 12 s left"), BoostSecondsLeft(P, ShinobiTags::State_SpeedBoost), UGE_SpeedBoost::Seconds, 0.1f);
	W.Tick(12.1f);
	TestFalse(TEXT("Over after 12 s"), ShinobiTest::HasTag(P, ShinobiTags::State_SpeedBoost));
	TestEqual(TEXT("Back to normal"), P->GetCharacterMovement()->GetMaxSpeed(), Base, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickupShieldTest, "ShinobiCoop.Pvp.Pickup.ShieldAbsorbsExactlySixtyThenEnds", ShinobiTestFlags)
bool FShinobiPickupShieldTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::Hover(P);
	ShinobiTest::Hover(Enemy);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const FGameplayAttribute Shield = UShinobiAttributeSet::GetShieldAttribute();
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);

	ShinobiTest::Set(P, Chakra, 10.f);
	TestTrue(TEXT("Taken"), AShinobiPickup::Apply(*P, EShinobiPickupType::Shield));
	TestEqual(TEXT("Chakra refilled"), ShinobiTest::Get(P, Chakra), P->GetAttributes()->GetMaxChakra());
	TestEqual(TEXT("60 of shield"), ShinobiTest::Get(P, Shield), UGE_Shield::Absorb);
	TestEqual(TEXT("The HUD shows 10 s left"), BoostSecondsLeft(P, ShinobiTags::State_Shield), UGE_Shield::Seconds, 0.1f);

	// A 30 hit: all absorbed, and no stagger (30 would stagger an unshielded target).
	ShinobiCombat::ApplyDamage(EnemyASC, P, 30.f);
	TestEqual(TEXT("No damage"), ShinobiTest::Get(P, Health), 500.f);
	TestEqual(TEXT("30 shield left"), ShinobiTest::Get(P, Shield), 30.f);
	TestTrue(TEXT("Still shielded"), ShinobiTest::HasTag(P, ShinobiTags::State_Shield));
	TestEqual(TEXT("A fully absorbed hit doesn't stagger"), Staggers.Count, 0);

	// Fresh shield, a 100 hit: 60 absorbed, 40 through, and the shield is gone.
	AShinobiPickup::Apply(*P, EShinobiPickupType::Shield);
	TestEqual(TEXT("Refilled to 60"), ShinobiTest::Get(P, Shield), UGE_Shield::Absorb);
	ShinobiCombat::ApplyDamage(EnemyASC, P, 100.f);
	TestEqual(TEXT("40 taken"), ShinobiTest::Get(P, Health), 460.f);
	TestEqual(TEXT("Shield spent"), ShinobiTest::Get(P, Shield), 0.f);
	TestFalse(TEXT("Shield ended"), ShinobiTest::HasTag(P, ShinobiTags::State_Shield));
	TestEqual(TEXT("What got through staggers"), Staggers.Count, 1);
	ShinobiCombat::ApplyDamage(EnemyASC, P, 100.f);
	TestEqual(TEXT("Then hits land in full"), ShinobiTest::Get(P, Health), 360.f);

	// Burning eats the shield too; unspent, it runs out after 10 s and leaves nothing behind.
	AShinobiPickup::Apply(*P, EShinobiPickupType::Shield);
	ShinobiTest::ApplySetByCaller(P, UGE_Burn::StaticClass(), ShinobiTags::Data_Damage, 10.f);
	W.Tick(1.5f); // one tick, at 1 s
	TestEqual(TEXT("A burn tick is absorbed"), ShinobiTest::Get(P, Health), 360.f);
	TestEqual(TEXT("And spends 10 of shield"), ShinobiTest::Get(P, Shield), 50.f);
	P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Burning));
	W.Tick(8.6f);
	TestFalse(TEXT("Over after 10 s"), ShinobiTest::HasTag(P, ShinobiTags::State_Shield));
	TestEqual(TEXT("No shield value left behind"), ShinobiTest::Get(P, Shield), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickupOnceTest, "ShinobiCoop.Pvp.Pickup.PickupAppliesOnce", ShinobiTestFlags)
bool FShinobiPickupOnceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	// Far from the pickup, so only the direct calls below touch it.
	AShinobiPlayerCharacter* A = W.Spawn<AShinobiPlayerCharacter>(FVector(2000.f, 0.f, 0.f));
	AShinobiPlayerCharacter* B = W.Spawn<AShinobiPlayerCharacter>(FVector(-2000.f, 0.f, 0.f));
	ShinobiTest::Hover(A);
	ShinobiTest::Hover(B);
	AShinobiPickup* Pickup = AShinobiPickup::Spawn(W.World, EShinobiPickupType::Attack, FVector::ZeroVector);
	TestNotNull(TEXT("Spawned"), Pickup);
	if (!Pickup)
	{
		return false;
	}
	TestEqual(TEXT("Of its type"), Pickup->GetType(), EShinobiPickupType::Attack);
	TestFalse(TEXT("Available"), Pickup->IsTaken());

	// Both reach it in the same frame: the first one wins.
	TestTrue(TEXT("A takes it"), Pickup->TryTake(A));
	TestFalse(TEXT("B is too late"), Pickup->TryTake(B));
	TestTrue(TEXT("A is boosted"), ShinobiTest::HasTag(A, ShinobiTags::State_AttackBoost));
	TestFalse(TEXT("B is not"), ShinobiTest::HasTag(B, ShinobiTags::State_AttackBoost));
	TestTrue(TEXT("Taken"), Pickup->IsTaken());
	TestTrue(TEXT("Hidden while taken"), Pickup->IsHidden());
	TestFalse(TEXT("And untouchable"), Pickup->GetActorEnableCollision());

	Pickup->ResetNow();
	TestFalse(TEXT("Available again"), Pickup->IsTaken());
	TestFalse(TEXT("Shown"), Pickup->IsHidden());

	// Only the living take it, and a refused taker leaves it there.
	ShinobiTest::ApplyEffect(B, UGE_Downed::StaticClass());
	TestFalse(TEXT("The downed can't"), Pickup->TryTake(B));
	TestFalse(TEXT("Still available"), Pickup->IsTaken());
	TestFalse(TEXT("Nor can a non-character"), Pickup->TryTake(Pickup));

	// Taken, it comes back by itself after 25 s.
	Pickup->TryTake(A);
	W.Tick(AShinobiPickup::RespawnSeconds - 0.2f);
	TestTrue(TEXT("Still gone just before 25 s"), Pickup->IsTaken());
	W.Tick(0.3f);
	TestFalse(TEXT("Back after 25 s"), Pickup->IsTaken());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickupResetTest, "ShinobiCoop.Pvp.Pickup.GameModeResetRestoresPickups", ShinobiTestFlags)
bool FShinobiPickupResetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
	Mode->PvpFormat = EShinobiPvpFormat::OneVsOne;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(FVector(2000.f, 0.f, 0.f));
	P->SetTeam(EShinobiTeam::TeamA);
	ShinobiTest::Hover(P);
	AShinobiPickup* Speed = AShinobiPickup::Spawn(W.World, EShinobiPickupType::Speed, FVector::ZeroVector);
	AShinobiPickup* Shield = AShinobiPickup::Spawn(W.World, EShinobiPickupType::Shield, FVector(0.f, 1000.f, 0.f));
	TestTrue(TEXT("Spawned"), Speed && Shield);
	if (!Speed || !Shield)
	{
		return false;
	}
	Speed->TryTake(P);
	Shield->TryTake(P);
	TestTrue(TEXT("Both taken"), Speed->IsTaken() && Shield->IsTaken());

	Mode->StartRound(); // ResetCharacters, then OnRoundReset
	TestFalse(TEXT("Speed pickup back"), Speed->IsTaken());
	TestFalse(TEXT("Shield pickup back"), Shield->IsTaken());
	TestFalse(TEXT("Boost cleared"), ShinobiTest::HasTag(P, ShinobiTags::State_SpeedBoost));
	TestFalse(TEXT("Shield cleared"), ShinobiTest::HasTag(P, ShinobiTags::State_Shield));
	TestEqual(TEXT("Shield value cleared"), ShinobiTest::Get(P, UShinobiAttributeSet::GetShieldAttribute()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPickupSpawnTest, "ShinobiCoop.Pvp.Pickup.GameModeSpawnsAtTaggedTargetPoints", ShinobiTestFlags)
bool FShinobiPickupSpawnTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>(); // its BeginPlay finds no points yet
	const FName Tags[] = { TEXT("Pickup:Health"), TEXT("Pickup:Attack"), TEXT("Pickup:Speed"), TEXT("Pickup:Shield"), TEXT("Pickup:Health"), NAME_None };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tags); ++Index)
	{
		ATargetPoint* Point = W.Spawn<ATargetPoint>(FVector(1000.f * Index, 5000.f, 0.f));
		if (!Tags[Index].IsNone())
		{
			Point->Tags.Add(Tags[Index]);
		}
	}
	Mode->SpawnPickups();

	TMap<EShinobiPickupType, int32> Counts;
	int32 Total = 0;
	for (TActorIterator<AShinobiPickup> It(W.World); It; ++It)
	{
		++Counts.FindOrAdd(It->GetType());
		++Total;
		TestEqual(TEXT("At its point"), It->GetActorLocation().Y, 5000.0, 1.0);
	}
	TestEqual(TEXT("One per tagged point"), Total, 5);
	TestEqual(TEXT("Two health"), Counts.FindRef(EShinobiPickupType::Health), 2);
	TestEqual(TEXT("One attack"), Counts.FindRef(EShinobiPickupType::Attack), 1);
	TestEqual(TEXT("One speed"), Counts.FindRef(EShinobiPickupType::Speed), 1);
	TestEqual(TEXT("One shield"), Counts.FindRef(EShinobiPickupType::Shield), 1);
	return true;
}

namespace
{
	TArray<AShinobiBotFighter*> BotsOn(const FShinobiTestWorld& W, EShinobiTeam Team)
	{
		TArray<AShinobiBotFighter*> Found;
		for (TActorIterator<AShinobiBotFighter> It(W.World); It; ++It)
		{
			if (It->GetTeam() == Team)
			{
				Found.Add(*It);
			}
		}
		return Found;
	}

	AActor* FocusOf(const AShinobiCharacterBase* Character)
	{
		const AAIController* AI = Cast<AAIController>(Character->GetController());
		return AI ? AI->GetFocusActor() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpBotHealthTest, "ShinobiCoop.Pvp.Bot.BotHasPlayerHealth", ShinobiTestFlags)
bool FShinobiPvpBotHealthTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
	Mode->PvpFormat = EShinobiPvpFormat::OneVsOne;
	SpawnStart(W, FVector(0.f, 5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagA);
	SpawnStart(W, FVector(0.f, -5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagB);
	Mode->AssignTeam(W.Spawn<AAIController>()); // the one human, on A
	Mode->FillBots();

	const TArray<AShinobiBotFighter*> Bots = BotsOn(W, EShinobiTeam::TeamB);
	TestEqual(TEXT("One bot, on B"), Bots.Num(), 1);
	if (Bots.Num() != 1)
	{
		return false;
	}
	const AShinobiPlayerCharacter* Player = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 0.f, 3000.f));
	const float PlayerMax = Player->GetAttributes()->GetMaxHealth();
	TestEqual(TEXT("A player's max health"), Bots[0]->GetAttributes()->GetMaxHealth(), PlayerMax);
	TestEqual(TEXT("Starts full"), Bots[0]->GetAttributes()->GetHealth(), PlayerMax);
	const UShinobiOverheadTextComponent* Label = Bots[0]->FindComponentByClass<UShinobiOverheadTextComponent>();
	TestTrue(TEXT("Labelled Bot"), Label && Label->Text.ToString() == TEXT("Bot"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpBotTargetTest, "ShinobiCoop.Pvp.Bot.BotTargetsOpposingPlayerNotTeammate", ShinobiTestFlags)
bool FShinobiPvpBotTargetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiBotFighter* Bot = W.Spawn<AShinobiBotFighter>();
	AShinobiPlayerCharacter* Mate = W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f));
	AShinobiPlayerCharacter* Foe = W.Spawn<AShinobiPlayerCharacter>(FVector(-900.f, 0.f, 0.f));
	Bot->SetTeam(EShinobiTeam::TeamA);
	Mate->SetTeam(EShinobiTeam::TeamA);
	Foe->SetTeam(EShinobiTeam::TeamB);
	ShinobiTest::Hover(Bot);
	ShinobiTest::Hover(Mate);
	ShinobiTest::Hover(Foe);
	TestNotNull(TEXT("AI controller spawned"), Bot->GetController());

	W.Tick(0.1f);
	TestTrue(TEXT("The opposing player is the target, though the teammate is closer"), FocusOf(Bot) == Foe);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpBotKunaiTest, "ShinobiCoop.Pvp.Bot.ThrowsKunaiAtRangeThenClosesIn", ShinobiTestFlags)
bool FShinobiPvpBotKunaiTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiBotFighter* Bot = W.Spawn<AShinobiBotFighter>();
	AShinobiPlayerCharacter* Foe = W.Spawn<AShinobiPlayerCharacter>(FVector(1000.f, 0.f, 0.f));
	Bot->SetTeam(EShinobiTeam::TeamA);
	Foe->SetTeam(EShinobiTeam::TeamB);
	ShinobiTest::Hover(Bot);
	ShinobiTest::Hover(Foe);
	const float Full = Foe->GetAttributes()->GetMaxHealth();

	W.Tick(1.2f); // 0.6 s wind-up + ~0.35 s flight, from 1000 away: no melee could have reached yet
	TestEqual(TEXT("A kunai for the bot's attack damage"), ShinobiTest::Get(Foe, UShinobiAttributeSet::GetHealthAttribute()), Full - Bot->AttackDamage);
	TestTrue(TEXT("Thrown from range"), FVector::Dist2D(Bot->GetActorLocation(), Foe->GetActorLocation()) > 700.f);

	W.Tick(1.3f); // the next throw is not before SpecialInterval: the bot walks in meanwhile instead of standing and throwing
	const float Distance = FVector::Dist2D(Bot->GetActorLocation(), Foe->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Closed in between throws (%.0f)"), Distance), Distance < 600.f);
	TestEqual(TEXT("Still just the one kunai"), ShinobiTest::Get(Foe, UShinobiAttributeSet::GetHealthAttribute()), Full - Bot->AttackDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpBotHoldTest, "ShinobiCoop.Pvp.Bot.HeldBotNeitherMovesNorAttacks", ShinobiTestFlags)
bool FShinobiPvpBotHoldTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiBotFighter* Bot = W.Spawn<AShinobiBotFighter>();
	AShinobiPlayerCharacter* Foe = W.Spawn<AShinobiPlayerCharacter>(FVector(600.f, 0.f, 0.f));
	Bot->SetTeam(EShinobiTeam::TeamA);
	Foe->SetTeam(EShinobiTeam::TeamB);
	ShinobiTest::Hover(Bot);
	ShinobiTest::Hover(Foe);
	ShinobiTest::ApplyEffect(Bot, UGE_PvpHold::StaticClass());
	const FVector Start = Bot->GetActorLocation();
	const float Full = Foe->GetAttributes()->GetMaxHealth();

	W.Tick(1.5f);
	TestTrue(FString::Printf(TEXT("Held in place (moved %.0f)"), FVector::Dist2D(Start, Bot->GetActorLocation())), FVector::Dist2D(Start, Bot->GetActorLocation()) < 5.f);
	TestEqual(TEXT("No attack landed"), ShinobiTest::Get(Foe, UShinobiAttributeSet::GetHealthAttribute()), Full);
	TestNull(TEXT("Nobody picked as a target"), FocusOf(Bot));

	Bot->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_PvpHold));
	W.Tick(1.2f);
	TestTrue(TEXT("Released, it goes for the foe"), FocusOf(Bot) == Foe);
	TestTrue(TEXT("And hits it"), ShinobiTest::Get(Foe, UShinobiAttributeSet::GetHealthAttribute()) < Full);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpBotStatusTest, "ShinobiCoop.Pvp.Bot.GatherStatusCountsBotsNeverSummonsOrEnemies", ShinobiTestFlags)
bool FShinobiPvpBotStatusTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
	Mode->PvpFormat = EShinobiPvpFormat::TwoVsTwo;
	AShinobiPlayerCharacter* Human = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiBotFighter* Bot = W.Spawn<AShinobiBotFighter>(FVector(0.f, 500.f, 0.f));
	AShinobiBotFighter* OtherSide = W.Spawn<AShinobiBotFighter>(FVector(0.f, -3000.f, 0.f));
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(0.f, 3000.f, 0.f));
	Human->SetTeam(EShinobiTeam::TeamA);
	Bot->SetTeam(EShinobiTeam::TeamA);
	OtherSide->SetTeam(EShinobiTeam::TeamB);
	Ninja->SetTeam(EShinobiTeam::TeamA); // an ordinary enemy on the side is still not a fighter slot
	for (AShinobiCharacterBase* Body : TArray<AShinobiCharacterBase*>{ Human, Bot, OtherSide, Ninja })
	{
		ShinobiTest::Hover(Body);
	}
	const TArray<AShinobiSummon*> Toad = AShinobiSummon::Summon(Human, EShinobiSummonKind::Toad, FVector(0.f, 900.f, 0.f));
	TestEqual(TEXT("A toad on team A"), Toad.Num(), 1);

	const FShinobiTeamStatus Full = Mode->GatherStatus(EShinobiTeam::TeamA);
	TestEqual(TEXT("The human and the bot (not the toad, not the ninja)"), Full.Alive, 2);
	TestEqual(TEXT("Full health"), Full.HealthFraction, 1.f, 0.001f);
	TestEqual(TEXT("Team B's bot counts for B"), Mode->GatherStatus(EShinobiTeam::TeamB).Alive, 1);

	ShinobiTest::Damage(Bot, Bot->GetAttributes()->GetMaxHealth() + 10.f);
	TestFalse(TEXT("The bot died"), Bot->IsAlive());
	const FShinobiTeamStatus OneDead = Mode->GatherStatus(EShinobiTeam::TeamA);
	TestEqual(TEXT("The dead bot is out"), OneDead.Alive, 1);
	TestEqual(TEXT("Half the slots' health left"), OneDead.HealthFraction, 0.5f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpFillBotsTest, "ShinobiCoop.Pvp.Bot.FillBotsTopsUpEmptySlots", ShinobiTestFlags)
bool FShinobiPvpFillBotsTest::RunTest(const FString& Parameters)
{
	{
		FShinobiTestWorld W;
		AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
		Mode->PvpFormat = EShinobiPvpFormat::TwoVsTwo;
		const APlayerStart* StartA = SpawnStart(W, FVector(0.f, 5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagA);
		const APlayerStart* StartB = SpawnStart(W, FVector(0.f, -5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagB);
		Mode->AssignTeam(W.Spawn<AAIController>()); // a human on A
		Mode->AssignTeam(W.Spawn<AAIController>()); // and one on B

		Mode->FillBots();
		TestEqual(TEXT("2v2, one human each: one bot on A"), BotsOn(W, EShinobiTeam::TeamA).Num(), 1);
		TestEqual(TEXT("And one on B"), BotsOn(W, EShinobiTeam::TeamB).Num(), 1);
		if (BotsOn(W, EShinobiTeam::TeamA).Num() == 1 && BotsOn(W, EShinobiTeam::TeamB).Num() == 1)
		{
			TestTrue(TEXT("A's bot at A's start"), FVector::Dist2D(BotsOn(W, EShinobiTeam::TeamA)[0]->GetActorLocation(), StartA->GetActorLocation()) < 1.f);
			TestTrue(TEXT("B's bot at B's start"), FVector::Dist2D(BotsOn(W, EShinobiTeam::TeamB)[0]->GetActorLocation(), StartB->GetActorLocation()) < 1.f);
		}
		Mode->FillBots();
		TestEqual(TEXT("Filling again adds nobody"), BotsOn(W, EShinobiTeam::TeamA).Num() + BotsOn(W, EShinobiTeam::TeamB).Num(), 2);

		Mode->StartRound(); // ResetCharacters, then OnRoundReset: bots despawned and respawned
		TestEqual(TEXT("After a round reset: still one on A"), BotsOn(W, EShinobiTeam::TeamA).Num(), 1);
		TestEqual(TEXT("Still one on B"), BotsOn(W, EShinobiTeam::TeamB).Num(), 1);
		const TArray<AShinobiBotFighter*> Respawned = BotsOn(W, EShinobiTeam::TeamB);
		TestTrue(TEXT("Respawned at B's start, held for the countdown"), Respawned.Num() == 1
			&& FVector::Dist2D(Respawned[0]->GetActorLocation(), StartB->GetActorLocation()) < 1.f && ShinobiTest::HasTag(Respawned[0], ShinobiTags::State_PvpHold));

		// A dead bot is replaced at the next reset (it would be destroyed 3 s after dying anyway).
		if (Respawned.Num() == 1)
		{
			ShinobiTest::Damage(Respawned[0], 10000.f);
		}
		Mode->AssignTeam(W.Spawn<AAIController>()); // a human joins mid-round, on A
		Mode->StartRound();
		TestEqual(TEXT("The joiner took A's bot slot"), BotsOn(W, EShinobiTeam::TeamA).Num(), 0);
		const TArray<AShinobiBotFighter*> Fresh = BotsOn(W, EShinobiTeam::TeamB);
		TestTrue(TEXT("B has one fresh, living bot"), Fresh.Num() == 1 && Fresh[0]->IsAlive());
	}
	{
		FShinobiTestWorld W;
		AShinobiPvpGameMode* Mode = W.Spawn<AShinobiPvpGameMode>();
		Mode->PvpFormat = EShinobiPvpFormat::OneVsOne;
		SpawnStart(W, FVector(0.f, 5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagA);
		SpawnStart(W, FVector(0.f, -5000.f, 0.f), AShinobiPvpGameMode::TeamStartTagB);
		Mode->AssignTeam(W.Spawn<AAIController>());
		Mode->FillBots();
		TestEqual(TEXT("1v1, one human: no bot on the human's side"), BotsOn(W, EShinobiTeam::TeamA).Num(), 0);
		TestEqual(TEXT("One bot on the other"), BotsOn(W, EShinobiTeam::TeamB).Num(), 1);
		Mode->StartRound();
		TestEqual(TEXT("Not doubled by the reset"), BotsOn(W, EShinobiTeam::TeamB).Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPvpTravelOptionsTest, "ShinobiCoop.Pvp.Session.TravelOptions", ShinobiTestFlags)
bool FShinobiPvpTravelOptionsTest::RunTest(const FString& Parameters)
{
	using S = UShinobiSessionSubsystem;
	const TArray<FShinobiMap>& Maps = S::GetMaps();
	TestEqual(TEXT("A co-op map just listens"), S::MakeTravelOptions(Maps[0], EShinobiPvpFormat::None), FString(TEXT("listen")));
	TestEqual(TEXT("The default map is still Hidden Leaf Valley"), FString(Maps[0].DisplayName), FString(TEXT("Hidden Leaf Valley")));

	const FShinobiMap* Arena = Maps.FindByPredicate([](const FShinobiMap& M) { return M.bArena; });
	if (!TestNotNull(TEXT("There is an arena entry"), Arena))
	{
		return false;
	}
	const FString OneVsOne = S::MakeTravelOptions(*Arena, EShinobiPvpFormat::OneVsOne);
	TestTrue(TEXT("1v1 carries the format"), OneVsOne.Contains(TEXT("?Format=1v1")));
	TestTrue(TEXT("1v1 selects the PvP game mode"), OneVsOne.Contains(TEXT("/Script/ShinobiCoop.ShinobiPvpGameMode")));
	TestTrue(TEXT("1v1 still listens"), OneVsOne.StartsWith(TEXT("listen")));
	TestTrue(TEXT("2v2 carries the format"), S::MakeTravelOptions(*Arena, EShinobiPvpFormat::TwoVsTwo).Contains(TEXT("?Format=2v2")));

	for (const TCHAR* Name : { TEXT("Arena: West End"), TEXT("Arena: Camden"), TEXT("Arena: Westminster"), TEXT("Arena: Tower") })
	{
		const FShinobiMap* Found = Maps.FindByPredicate([Name](const FShinobiMap& M) { return FCString::Strcmp(M.DisplayName, Name) == 0; });
		TestTrue(FString::Printf(TEXT("%s is listed as an arena"), Name), Found && Found->bArena);
	}
	int32 ArenaCount = 0;
	for (const FShinobiMap& M : Maps)
	{
		ArenaCount += M.bArena ? 1 : 0;
		if (!M.bArena)
		{
			TestEqual(FString::Printf(TEXT("%s (co-op) listens"), M.DisplayName), S::MakeTravelOptions(M, EShinobiPvpFormat::None), FString(TEXT("listen")));
		}
	}
	TestEqual(TEXT("Exactly four arenas"), ArenaCount, 4);
	return true;
}

#endif
