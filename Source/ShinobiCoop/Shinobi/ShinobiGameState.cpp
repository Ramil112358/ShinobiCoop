#include "Shinobi/ShinobiGameState.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ShinobiCoop.h"
#include "Shinobi/UI/ShinobiSettings.h"

AShinobiGameState::AShinobiGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only while it snows
}

void AShinobiGameState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SetSnow(ShinobiSettings::GetSavedSnow()); // the host's pick
	}
}

void AShinobiGameState::SetSnow(EShinobiSnow NewSnow)
{
	if (HasAuthority() && NewSnow != Snow)
	{
		Snow = NewSnow;
		OnRep_Snow(); // the host draws it too
	}
}

void AShinobiGameState::OnRep_Snow()
{
	if (SnowEffect)
	{
		SnowEffect->DestroyComponent();
		SnowEffect = nullptr;
	}
	SetActorTickEnabled(false);
	if (Snow == EShinobiSnow::Off || IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	// Copies of the pack's NS_Snowing_GPU / NS_Snowing_Storm_GPU, widened to fill the view.
	const TCHAR* Path = Snow == EShinobiSnow::Blizzard ? TEXT("/Game/Shinobi/VFX/Weather/NS_SnowBlizzard.NS_SnowBlizzard") : TEXT("/Game/Shinobi/VFX/Weather/NS_SnowLight.NS_SnowLight");
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, Path);
	if (!System)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("Snow weather: missing %s"), Path);
		return;
	}
	// The flakes live in the world; only where new ones appear follows the camera, so they fall past as it moves.
	SnowEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, false, true);
	SetActorTickEnabled(SnowEffect != nullptr);
	Tick(0.f);
}

void AShinobiGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* Local = GetWorld()->GetFirstPlayerController();
	if (SnowEffect && Local && Local->PlayerCameraManager)
	{
		SnowEffect->SetWorldLocation(Local->PlayerCameraManager->GetCameraLocation());
	}
}

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
	DOREPLIFETIME(AShinobiGameState, Snow);
}

void AShinobiGameState::OnRep_PvpWins()
{
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: score A %d - B %d"), PvpWinsA, PvpWinsB);
}

float AShinobiGameState::GetPhaseTimeRemaining() const
{
	return PhaseEndTime > 0.f ? FMath::Max(0.f, PhaseEndTime - static_cast<float>(GetServerWorldTimeSeconds())) : 0.f;
}
