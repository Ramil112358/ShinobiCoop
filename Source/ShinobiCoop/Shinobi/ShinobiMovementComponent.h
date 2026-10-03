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

	/** Rides State.Mounted: flies while it is on and falls once it ends. Runs in every move, on the client and the server alike,
	 * so the tag (which replicates) is all the two need to agree on. */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	static constexpr float FlightSpeed = 1500.f;

	/** The lowest a mounted rider's capsule floor may be above the ground: the dragon's legs hang below the rider, so it can lift off
	 * from the ground and never lowers into it. */
	static constexpr float MountedGroundClearance = 230.f;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

	/** Server: position corrections also carry rotation for the next Seconds. Corrections are location-only by
	 * default, so a server-side teleport that turns the character (Substitution) would otherwise never turn the
	 * owning client. */
	void ForceRotationCorrectionFor(float Seconds);
	virtual bool ShouldCorrectRotation() const override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SprintSpeed = 900.f;

	uint8 bWantsToSprint : 1;

private:
	double ForceRotationCorrectionUntil = 0.0;
	bool bWasMounted = false;
};
