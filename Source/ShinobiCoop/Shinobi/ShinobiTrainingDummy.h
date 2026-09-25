#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "ShinobiTrainingDummy.generated.h"

class UTextRenderComponent;

/** Enemy-team punching bag. Shows its health above its head and resets 1.5 s after being defeated. */
UCLASS()
class SHINOBICOOP_API AShinobiTrainingDummy : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiTrainingDummy(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void HandleOutOfHealth() override;

private:
	void ResetHealth();

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UTextRenderComponent> HealthText;

	FTimerHandle ResetTimer;

	/** Last values written to HealthText. Seeded from Attributes in the constructor, so Tick only calls
	 * SetText (which unconditionally marks render state dirty) when the rounded HP or MaxHP actually
	 * changes rather than on every frame. */
	int32 DisplayedHealth = -1;
	int32 DisplayedMaxHealth = -1;
};
