#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiFirePatch.generated.h"

class AShinobiCharacterBase;
class UPointLightComponent;
class UStaticMeshComponent;

UENUM()
enum class EShinobiFirePatch : uint8
{
	Fire,       // where a Fireball lands: 3 s, catches enemies with Burn
	BlackFlame, // Amaterasu cast at a spot with no enemy: 5 s, catches enemies with the black flame
};

/**
 * Flames left burning on the ground for a few seconds: a cluster of flickering flame tongues and a light. Enemies of
 * the caster inside Radius (150 x Scale) catch the patch's status every 0.5 s (refreshing it); allies are unharmed.
 * Spawned by the server with Spawn(); it replicates to every client, which shows it on its own clock.
 */
UCLASS()
class SHINOBICOOP_API AShinobiFirePatch : public AActor
{
	GENERATED_BODY()

public:
	AShinobiFirePatch();

	/** Server only (does nothing on a client); returns the spawned patch, or null. */
	static AShinobiFirePatch* Spawn(UWorld* World, EShinobiFirePatch Kind, const FVector& Location, AShinobiCharacterBase* Caster, float Scale, float DamagePerTick);

	static constexpr float BaseRadius = 150.f;
	static constexpr float TickInterval = 0.5f;

	EShinobiFirePatch GetKind() const { return Kind; }
	float GetRadius() const { return BaseRadius * Scale; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	/** Server: the status on every enemy of the caster inside the radius. */
	void BurnInside();

	UPROPERTY(Replicated)
	EShinobiFirePatch Kind = EShinobiFirePatch::Fire;

	UPROPERTY(Replicated)
	float Scale = 1.f;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Tongues;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UPointLightComponent> Light;

	TWeakObjectPtr<AShinobiCharacterBase> Caster;
	float DamagePerTick = 0.f;
	float Age = 0.f;
	float Duration = 3.f;
	FTimerHandle BurnTimer;
};
