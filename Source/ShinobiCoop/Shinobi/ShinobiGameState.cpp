#include "Shinobi/ShinobiGameState.h"
#include "Net/UnrealNetwork.h"

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
}

float AShinobiGameState::GetPhaseTimeRemaining() const
{
	return PhaseEndTime > 0.f ? FMath::Max(0.f, PhaseEndTime - static_cast<float>(GetServerWorldTimeSeconds())) : 0.f;
}
