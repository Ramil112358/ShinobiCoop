#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShinobiMovementComponent.generated.h"

/** Adds a sprint flag that travels in saved moves, so the server agrees with the client (no rubber-banding). */
UCLASS()
class SHINOBICOOP_API UShinobiMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UShinobiMovementComponent();

	void SetSprinting(bool bNewSprinting) { bWantsToSprint = bNewSprinting; }
	bool IsSprinting() const { return bWantsToSprint; }

	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SprintSpeed = 900.f;

	uint8 bWantsToSprint : 1;
};
