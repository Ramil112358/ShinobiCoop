#include "Shinobi/ShinobiGameMode.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/ShinobiTrainingDummy.h"

AShinobiGameMode::AShinobiGameMode()
{
	DefaultPawnClass = AShinobiPlayerCharacter::StaticClass();
	PlayerControllerClass = AShinobiPlayerController::StaticClass();
}

void AShinobiGameMode::BeginPlay()
{
	Super::BeginPlay();

	TActorIterator<APlayerStart> Start(GetWorld());
	if (!Start)
	{
		return;
	}
	const FVector Origin = Start->GetActorLocation();
	const FVector Forward = Start->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FRotator FacePlayer = (-Forward).Rotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	for (const float Offset : { -300.f, 0.f, 300.f })
	{
		GetWorld()->SpawnActor<AShinobiTrainingDummy>(Origin + Forward * 600.f + Right * Offset, FacePlayer, Params);
	}
}
