#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiSusanooComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * A Susanoo figure: while its owner carries StateTag, Model (origin at the feet, in the glowing M_Susanoo) stands around them.
 * It fades in, breathes slowly, and fades out when the tag goes. The tag replicates, so every machine shows it. Cosmetic only.
 * Defaults: the Susanoo jutsu's 6 m ribcage (State.Susanoo, SM_Susanoo). Players have a second one for their guard (UGA_Block).
 */
UCLASS(ClassGroup = (Shinobi))
class SHINOBICOOP_API UShinobiSusanooComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShinobiSusanooComponent();

	/** True while the armour is up (the figure may still be fading out after it ends). */
	bool IsShown() const { return bWanted; }

	/** The figure's mesh component; null until the armour first goes up, and on a dedicated server. */
	UStaticMeshComponent* GetFigure() const { return Figure; }

	/** Set by the owner's constructor; the defaults are the Susanoo jutsu's. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	FGameplayTag StateTag;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	FSoftObjectPath Model;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float FadeInSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float FadeOutSeconds = 0.5f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnStateChanged(const FGameplayTag Tag, int32 Count);

	/** Builds the figure and its material the first time they are needed. Does nothing if the model is not imported. */
	void EnsureFigure();

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
