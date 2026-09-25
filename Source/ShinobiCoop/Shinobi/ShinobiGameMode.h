#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShinobiGameMode.generated.h"

/** M1 training mode: Shinobi pawn/controller and three training dummies in front of the first PlayerStart. */
UCLASS()
class SHINOBICOOP_API AShinobiGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShinobiGameMode();

protected:
	virtual void BeginPlay() override;
};
