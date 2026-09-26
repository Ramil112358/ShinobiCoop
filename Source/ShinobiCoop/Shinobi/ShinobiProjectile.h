#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiProjectile.generated.h"

class AShinobiCharacterBase;
class UGameplayEffect;
class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Server-spawned, replicated projectile (Fireball; M3's kunai). Flies straight; explodes on the first character
 * hostile to its instigator (direct damage, plus StatusEffect if the hit lands) or on world geometry, and deals
 * SplashDamage to the other hostiles within SplashRadius. Damage is applied on the server only, from the
 * instigator's ability system. Spawn with SpawnActorDeferred (Owner and Instigator = the caster), set the damage
 * fields, then FinishSpawning.
 */
UCLASS()
class SHINOBICOOP_API AShinobiProjectile : public AActor
{
	GENERATED_BODY()

public:
	AShinobiProjectile();

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float DirectDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SplashDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SplashRadius = 0.f;

	/** Applied to the direct target when its hit lands (e.g. GE_Burn), with Data.Damage = StatusDamagePerTick. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TSubclassOf<UGameplayEffect> StatusEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float StatusDamagePerTick = 0.f;

protected:
	virtual void BeginPlay() override;

	/** Look, set per class in constructors (clients spawn the same class, so nothing extra replicates). */
	FLinearColor GlowColor;
	float GlowIntensity = 8.f;
	bool bBurstOnImpact = true;

private:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Server only. DirectTarget may be null (hit a wall or the floor). */
	void Explode(AShinobiCharacterBase* DirectTarget, const FVector& Location);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	bool bExploded = false;
};

/** Enemy kunai: a small grey blade, faster, no light or fire burst. Set DirectDamage only. */
UCLASS()
class SHINOBICOOP_API AShinobiKunai : public AShinobiProjectile
{
	GENERATED_BODY()

public:
	AShinobiKunai();
};
