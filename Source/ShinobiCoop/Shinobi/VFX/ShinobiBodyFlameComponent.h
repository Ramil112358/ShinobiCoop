#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiBodyFlameComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Flames on a burning body: ten flickering flame tongues on the spine and limbs, plus a light, while its owner carries
 * State.Burning.* and isn't dead (the effects outlast a death). Amaterasu's black flame (M_FlameDark, dark purple
 * light) wins over a normal burn (M_Flame, orange). Cosmetic only; the tags replicate, so every machine shows it. A
 * fixed size: GAS replicates tags to every client but not effect magnitudes.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiBodyFlameComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShinobiBodyFlameComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsBurning() const { return bBurning; }
	bool IsBlack() const { return bBlack; }
	int32 NumVisibleTongues() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnTagChanged(const FGameplayTag Tag, int32 NewCount);
	void Refresh();

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Tongues;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FireMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BlackMaterial;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle BurningHandle;
	FDelegateHandle DeadHandle;
	bool bBurning = false;
	bool bBlack = false;
};
