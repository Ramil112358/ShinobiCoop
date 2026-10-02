#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiMeteor.generated.h"

class AShinobiCharacterBase;
class UGameplayEffect;
class UPointLightComponent;
class UStaticMeshComponent;

UENUM()
enum class EShinobiMeteor : uint8
{
	Fire, // Fire Meteor: one big flaming rock
	Ice,  // Ice Meteor Shower: a small ice shard
};

/** What a meteor does where it lands. Server-side only; the clients only need to see it fall. */
struct FShinobiMeteorImpact
{
	float Damage = 0.f;
	float Radius = 0.f;
	/** Applied to every enemy the damage landed on (GE_Burn, GE_Chill). */
	TSubclassOf<UGameplayEffect> Status;
	float StatusDamagePerTick = 0.f;
	/** Greater than 0: leaves an AShinobiFirePatch of this scale. */
	float FirePatchScale = 0.f;
};

/**
 * A meteor falling onto ImpactPoint over FallSeconds, then landing: damage and a status for the caster's enemies in
 * Radius, a burst, and (Fire) a patch of ground fire. Server-spawned and replicated; every machine animates the fall on
 * its own clock, and the server decides the landing. If the caster is gone by then it still lands, hurting nobody.
 */
UCLASS()
class SHINOBICOOP_API AShinobiMeteor : public AActor
{
	GENERATED_BODY()

public:
	AShinobiMeteor();

	/** Server only (does nothing on a client). VisualScale sizes the rock, its light and the landing burst. */
	static AShinobiMeteor* Spawn(UWorld* World, AShinobiCharacterBase* Caster, EShinobiMeteor Kind, const FVector& ImpactPoint, float FallSeconds, float VisualScale, const FShinobiMeteorImpact& Impact);

	EShinobiMeteor GetKind() const { return Kind; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	/** Server: the landing. */
	void Land();

	UPROPERTY(Replicated)
	EShinobiMeteor Kind = EShinobiMeteor::Fire;

	UPROPERTY(Replicated)
	FVector StartPoint = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float FallSeconds = 1.4f;

	UPROPERTY(Replicated)
	float VisualScale = 1.f;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	FShinobiMeteorImpact Impact;
	TWeakObjectPtr<AShinobiCharacterBase> Caster;
	float Age = 0.f;
};
