#pragma once

#include "CoreMinimal.h"
#include "Components/TextRenderComponent.h"
#include "ShinobiOverheadTextComponent.generated.h"

/**
 * "HP 440 / 500 [Burn] [Shock]" floating above a character (training dummies, enemies), turned to face the
 * local camera. Polls the owner's attributes and status tags each tick and only rewrites the text when it
 * changes (SetText always marks render state dirty).
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiOverheadTextComponent : public UTextRenderComponent
{
	GENERATED_BODY()

public:
	UShinobiOverheadTextComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Refresh();

	int32 DisplayedHealth = -1;
	int32 DisplayedMaxHealth = -1;
	FString DisplayedStatuses;
};
