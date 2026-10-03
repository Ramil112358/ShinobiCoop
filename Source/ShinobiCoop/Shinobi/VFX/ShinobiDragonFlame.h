#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shinobi/ShinobiDragonMount.h"
#include "ShinobiDragonFlame.generated.h"

class AShinobiCharacterBase;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * The dragon's fire-breath as one continuous jet (UGA_DragonBreath spawns it for the breath's length): a muzzle flash at the mouth and
 * a stream of flame, ember and smoke sprites that stream out along the rider's aim, spreading as they go, with a flickering light that
 * rides the flames. The server moves the actor with the aim (movement replicates); every machine that renders animates its own
 * particles from the clock, so nothing but the actor itself crosses the wire. Cosmetic: the damage is the ability's.
 */
UCLASS()
class SHINOBICOOP_API AShinobiDragonFlame : public AActor
{
	GENERATED_BODY()

public:
	AShinobiDragonFlame();

	/** Server only: a jet from Rider's dragon's mouth along their aim. Null on a client. */
	static AShinobiDragonFlame* Spawn(AShinobiCharacterBase* Rider, EShinobiDragonKind Kind = EShinobiDragonKind::Tairn);

	EShinobiDragonKind GetKind() const { return Kind; }

	/** Server: stops feeding the jet; the last flames drift out and the actor goes. */
	void Stop();

	AShinobiCharacterBase* GetRider() const { return Rider; }
	int32 GetParticleCount() const { return Particles.Num(); }

	static constexpr float Reach = 1500.f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	struct FParticle
	{
		UStaticMeshComponent* Mesh = nullptr;
		UMaterialInstanceDynamic* Material = nullptr;
		uint8 Sprite = 0;        // ShinobiVfx::ESprite
		float Phase = 0.f;       // where in its life it starts, so the jet is full from the first moment
		float Life = 0.5f;       // seconds from the mouth to the end of its flight
		float Width = 100.f;     // at the end of its life; it starts at a third of this
		float Spread = 0.f;      // sideways drift at the end of its life, as a fraction of the distance flown
		FVector Side = FVector::RightVector;
		float Reach = 1.f;       // fraction of Reach it flies
		int32 Variant = 0;
		float NextSwap = 0.f;
	};

	void AddParticle(UStaticMesh* Plane, uint8 Sprite, const FLinearColor& Tint, float Glow, float Phase, float Life, float Width, float Spread, float ReachFraction);
	void FollowRider();

	UPROPERTY(Replicated)
	TObjectPtr<AShinobiCharacterBase> Rider;

	/** Whose fire: Tairn's is blue-white and lightning-tinged, the crimson dragon's is ordinary orange. */
	UPROPERTY(Replicated)
	EShinobiDragonKind Kind = EShinobiDragonKind::Tairn;

	/** Set by Stop(); clients then stop feeding the jet too (they are destroyed a moment later). */
	UPROPERTY(ReplicatedUsing = OnRep_Stopped)
	bool bStopped = false;

	UFUNCTION()
	void OnRep_Stopped();

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	TArray<FParticle> Particles;
	float Age = 0.f;
	float StoppedAt = -1.f;
};
