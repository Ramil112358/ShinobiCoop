#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiTelegraph.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * A warning ring on the ground: an outer ring of Radius and an inner one that closes in on the centre as the time runs
 * out, so everyone can read where and when a jutsu lands. Purely cosmetic and replicated (the server spawns it, each
 * machine animates it on its own clock). Removes itself after Seconds.
 */
UCLASS()
class SHINOBICOOP_API AShinobiTelegraph : public AActor
{
	GENERATED_BODY()

public:
	AShinobiTelegraph();

	/** Server only (does nothing on a client); returns the spawned ring, or null. Where is a point on the ground. */
	static AShinobiTelegraph* Spawn(UWorld* World, const FVector& Where, float Radius, float Seconds, const FLinearColor& Tint);

	float GetRadius() const { return Radius; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Replicated)
	float Radius = 300.f;

	UPROPERTY(Replicated)
	float Seconds = 1.f;

	UPROPERTY(Replicated)
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Outer;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Inner;

	float Age = 0.f;
};
