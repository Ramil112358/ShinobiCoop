#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "ShinobiPlayerCharacter.generated.h"

/** Player-controlled shinobi. */
UCLASS()
class SHINOBICOOP_API AShinobiPlayerCharacter : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer);
};
