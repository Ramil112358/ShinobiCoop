#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiWaterDragon.generated.h"

class AShinobiCharacterBase;
class UStaticMeshComponent;

/** What a water dragon does on its way. The server alone needs these. */
struct FShinobiDragonHit
{
	float Damage = 0.f;
	/** How close to the dragon's head an enemy has to be to be caught. */
	float HitRadius = 140.f;
	/** Horizontal speed given to an enemy it catches. */
	float Knockback = 700.f;
};

/**
 * A dragon of water: a head and a snaking body that flies along Direction at Speed for Lifetime seconds. Every enemy of
 * the caster it passes takes Damage once, is staggered and thrown on along its path; allies are untouched. It breaks on
 * the first solid thing in its way (a wall, a rock) with a splash. Server-spawned and replicated; every machine draws the
 * flight on its own clock and the server decides who it catches.
 */
UCLASS()
class SHINOBICOOP_API AShinobiWaterDragon : public AActor
{
	GENERATED_BODY()

public:
	AShinobiWaterDragon();

	/** Server only (does nothing on a client). Direction need not be normalised. VisualScale sizes the body. */
	static AShinobiWaterDragon* Spawn(UWorld* World, AShinobiCharacterBase* Caster, const FVector& Start, const FVector& Direction, float Speed, float Lifetime, float VisualScale, const FShinobiDragonHit& Hit);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	FVector HeadAt(float Time) const;

	/** Server: catches the enemies the head passed between From and To, once each. */
	void Catch(const FVector& From, const FVector& To);

	void Finish(const FVector& Where);

	UPROPERTY(Replicated)
	FVector StartPoint = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector Heading = FVector::ForwardVector;

	UPROPERTY(Replicated)
	float Speed = 1800.f;

	UPROPERTY(Replicated)
	float Lifetime = 1.2f;

	UPROPERTY(Replicated)
	float VisualScale = 1.f;

	/** The head first, then the body from the neck to the tail. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	FShinobiDragonHit HitData;
	TWeakObjectPtr<AShinobiCharacterBase> Caster;
	TSet<TWeakObjectPtr<AShinobiCharacterBase>> Caught;
	FVector LastHead = FVector::ZeroVector;
	float Age = 0.f;
};
