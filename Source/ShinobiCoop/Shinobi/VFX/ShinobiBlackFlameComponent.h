#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiBlackFlameComponent.generated.h"

class UAbilitySystemComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Amaterasu's black flames on whoever carries State.Burning.Black and isn't dead (the effect outlasts a death): ten
 * flickering flame tongues (M_FlameDark) on the body's spine and limbs, plus a dark purple light. Cosmetic only; the tag replicates, so every machine shows it. A
 * fixed size: GAS replicates tags to every client but not effect magnitudes, so the charge shows in the ignition
 * burst instead.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiBlackFlameComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShinobiBlackFlameComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsBurning() const { return bBurning; }
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

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle BurningHandle;
	FDelegateHandle DeadHandle;
	bool bBurning = false;
};
