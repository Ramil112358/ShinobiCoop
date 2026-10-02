#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiMarkerKunai.generated.h"

class AShinobiCharacterBase;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Teleport's special kunai. Flies straight (4000 u/s, 2500 range) and sticks in the first surface it meets; it flies
 * through characters. It does no damage: the caster's next press of the key jumps to one (UGA_TeleportJump). Lasts
 * Lifetime seconds. Server-spawned and replicated; sticking is decided on the server.
 */
UCLASS()
class SHINOBICOOP_API AShinobiMarkerKunai : public AActor
{
	GENERATED_BODY()

public:
	AShinobiMarkerKunai();

	static constexpr float Lifetime = 6.f;
	static constexpr float Speed = 4000.f;
	static constexpr float Range = 2500.f;

	/** Server. Owner and Instigator are the caster. */
	static AShinobiMarkerKunai* Spawn(UWorld* World, AShinobiCharacterBase* Caster, const FVector& Start, const FRotator& Direction);

	/** Server: it has hit a surface and stopped. */
	bool IsStuck() const { return bStuck; }

	/** Server: when it was thrown (world time), to tell the oldest. */
	float GetThrownAt() const { return ThrownAt; }

	/** Where a character's capsule centre goes to stand at this kunai: out from the surface by the radius, and up or down
	 * by the half height for a floor or ceiling. */
	FVector GetArrivalPoint(float CapsuleRadius, float CapsuleHalfHeight) const;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnStopped(const FHitResult& ImpactResult);

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UProjectileMovementComponent> Movement;

	bool bStuck = false;
	float ThrownAt = 0.f;
	FVector StuckNormal = FVector::UpVector;
};
