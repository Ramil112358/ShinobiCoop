#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiBurst.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

UENUM()
enum class EShinobiBurst : uint8
{
	Fire,         // Fireball impact
	Firestorm,    // Rasengan on a burning target: a big fire sphere the size of its 500 radius
	Lightning,    // Chidori strike
	Spiral,       // Rasengan strike
	Substitution, // a log left where the player stood, in a puff of smoke
};

/**
 * One-shot impact effect: an expanding, fading glow sphere with a light (and, for Substitution, a log that
 * lingers). Spawned by the server with Spawn(); it replicates to every client and plays there on its own
 * clock. Stands in for the spec's Gameplay Cues: native C++ cue notifies would still need asset-registry
 * paths and Blueprint assets, while one small replicated actor covers all the one-shot effects.
 */
UCLASS()
class SHINOBICOOP_API AShinobiBurst : public AActor
{
	GENERATED_BODY()

public:
	AShinobiBurst();

	/** Server only (does nothing on a client); returns the spawned burst, or null. */
	static AShinobiBurst* Spawn(UWorld* World, EShinobiBurst Kind, const FVector& Location);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EShinobiBurst GetKind() const { return Kind; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Replicated)
	EShinobiBurst Kind = EShinobiBurst::Fire;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Log;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SphereMaterial;

	float Age = 0.f;
	float Duration = 0.4f;
	float MaxScale = 3.f;
	float Intensity = 8.f;
	float LightIntensity = 0.f;
};
