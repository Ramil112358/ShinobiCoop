#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiSusanooComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * Susanoo's look: while its owner carries State.Susanoo, a spectral ribcage figure (SM_Susanoo, about 6 m tall, in the glowing
 * M_Susanoo) stands around them. It fades in over a moment, pulses slowly, and fades out when the armour ends. The tag
 * replicates, so every machine shows it. Cosmetic only; the armour itself is UGE_Susanoo and the damage exec.
 */
UCLASS(ClassGroup = (Shinobi))
class SHINOBICOOP_API UShinobiSusanooComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShinobiSusanooComponent();

	/** True while the armour is up (the figure may still be fading out after it ends). */
	bool IsShown() const { return bWanted; }

	/** The figure's mesh component; null on a dedicated server. */
	UStaticMeshComponent* GetFigure() const { return Figure; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnSusanooChanged(const FGameplayTag Tag, int32 Count);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Figure;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Material;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle Handle;
	bool bWanted = false;
	float Fade = 0.f;
	float Age = 0.f;
};
