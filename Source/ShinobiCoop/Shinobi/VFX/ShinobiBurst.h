#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiBurst.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

namespace ShinobiVfx { enum class ESprite : uint8; }

UENUM()
enum class EShinobiBurst : uint8
{
	Fire,         // Fireball impact
	Firestorm,    // Rasengan on a burning target: a big fire sphere the size of its 500 radius
	Lightning,    // Chidori strike
	Spiral,       // Rasengan strike
	Substitution, // a log left where the player stood, in a puff of smoke
	FireBreath,       // Fireball: a flame cone from the caster's mouth along the burst's rotation
	BlackFlameIgnite, // Amaterasu: the black flames catching on the target
	Shockwave,        // Almighty Push: a ring of force and dust on the ground
	Swirl,            // Kamui: the spiral the caster slips into or out of
	TeleportFlash,    // Teleport: the flash where the caster arrives
	Crumble,          // Earth Wall breaking
	Splash,           // Water Wall breaking
	MeteorImpact,     // Fire Meteor landing
	FrostImpact,      // an ice shard landing
	DragonSplash,     // Water Dragon ending
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

	/** Server only (does nothing on a client); returns the spawned burst, or null. Scale multiplies its size (a charged
	 * jutsu's burst is bigger); Rotation aims the directional kinds (FireBreath). */
	static AShinobiBurst* Spawn(UWorld* World, EShinobiBurst Kind, const FVector& Location, float Scale = 1.f, const FRotator& Rotation = FRotator::ZeroRotator);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EShinobiBurst GetKind() const { return Kind; }
	float GetScale() const { return Scale; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Replicated)
	EShinobiBurst Kind = EShinobiBurst::Fire;

	UPROPERTY(Replicated)
	float Scale = 1.f;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Log;

	/** FireBreath's flame cone: tip at the actor, widening along its forward. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Cone;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SphereMaterial;

	/** One SpriteCook sprite on a quad (flames, lightning, a ring, a flash). The components belong to the actor. */
	struct FCard
	{
		UStaticMeshComponent* Mesh = nullptr;
		UMaterialInstanceDynamic* Material = nullptr;
		uint8 Sprite = 0;        // ShinobiVfx::ESprite
		FVector Offset = FVector::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector; // full size in world units
		float Rise = 0.f;        // units it climbs over the effect
		float Delay = 0.f;       // seconds before it appears (a stream of flame reaches its far cards later)
		bool bFlat = false;      // lies on the ground (rings, vortexes) instead of facing the camera
		bool bFlicker = false;   // switches variant every few frames (lightning, flame)
		float Spin = 0.f;        // degrees per second, flat cards only
		float NextSwap = 0.f;
		int32 Variant = 0;
	};

	/** Replace the glow sphere with sprite cards for this kind; false if the kind keeps the sphere (Substitution). */
	bool BuildCards();
	void AddCard(UStaticMesh* Plane, ShinobiVfx::ESprite Sprite, const FLinearColor& Tint, float Glow, const FVector& Offset, const FVector2D& Size, float Rise = 0.f, bool bFlat = false, bool bFlicker = false, float Delay = 0.f);

	TArray<FCard> Cards;

	float Age = 0.f;
	float Duration = 0.4f;
	float MaxScale = 3.f;
	float Intensity = 8.f;
	float LightIntensity = 0.f;
};
