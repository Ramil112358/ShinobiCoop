#include "Shinobi/ShinobiGameState.h"
#include "Net/UnrealNetwork.h"
#include "ShinobiCoop.h"

void AShinobiGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShinobiGameState, Wave);
	DOREPLIFETIME(AShinobiGameState, NumWaves);
	DOREPLIFETIME(AShinobiGameState, Phase);
	DOREPLIFETIME(AShinobiGameState, EnemiesRemaining);
	DOREPLIFETIME(AShinobiGameState, PhaseEndTime);
	DOREPLIFETIME(AShinobiGameState, CampsCleared);
	DOREPLIFETIME(AShinobiGameState, NumCamps);
	DOREPLIFETIME(AShinobiGameState, VillageLocation);
	DOREPLIFETIME(AShinobiGameState, HubName);
	DOREPLIFETIME(AShinobiGameState, VictoryText);
	DOREPLIFETIME(AShinobiGameState, PvpPhase);
	DOREPLIFETIME(AShinobiGameState, PvpFormat);
	DOREPLIFETIME(AShinobiGameState, PvpRound);
	DOREPLIFETIME(AShinobiGameState, PvpWinsA);
	DOREPLIFETIME(AShinobiGameState, PvpWinsB);
	DOREPLIFETIME(AShinobiGameState, PvpRoundResult);
}

void AShinobiGameState::OnRep_PvpWins()
{
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: score A %d - B %d"), PvpWinsA, PvpWinsB);
}

float AShinobiGameState::GetPhaseTimeRemaining() const
{
	return PhaseEndTime > 0.f ? FMath::Max(0.f, PhaseEndTime - static_cast<float>(GetServerWorldTimeSeconds())) : 0.f;
}
