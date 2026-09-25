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
};
