#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiCastFxComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Jutsu hand orb: a glowing sphere with a small light in the right hand while a jutsu is active (wind-up and
 * execution). Fire for Fireball, crackling lightning for Chidori, a bigger blue spiral for Rasengan, dark
 * purple for Amaterasu. Cosmetic only; it follows the State.Casting.* tags each jutsu grants while active,
 * which GAS replicates to every machine.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiCastFxComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShinobiCastFxComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The casting tag the orb currently shows, or an invalid tag when hidden. */
	FGameplayTag GetShownTag() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnCastingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void Refresh();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Orb;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> OrbMaterial;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TArray<FDelegateHandle> TagEventHandles;
	int32 ShownIndex = INDEX_NONE;
	float Age = 0.f;
};
