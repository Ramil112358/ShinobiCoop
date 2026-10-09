#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiPhaseFxComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInterface;
class USkeletalMeshComponent;

/**
 * Kamui's look: while its owner carries State.Phased, every visible body mesh (the hero, a worn outfit) is swapped to the
 * see-through M_Phased ghost and drops out of the outline; both are put back afterwards. The tag replicates, so every
 * machine shows it. One opacity for all viewers: the enemies are AI, so only the player and teammates look at it.
 */
UCLASS(ClassGroup = (Shinobi))
class SHINOBICOOP_API UShinobiPhaseFxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr float GhostOpacity = 0.3f;
	/** Seconds after the tag lands before the body turns ghost: Kamui's eye spiral plays first. Purely cosmetic. */
	static constexpr float GhostDelay = 0.2f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnPhasedChanged(const FGameplayTag Tag, int32 Count);
	void ApplyGhost();
	void RestoreBody();

	struct FSaved
	{
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		TArray<TWeakObjectPtr<UMaterialInterface>> Materials;
		bool bCustomDepth = false;
	};

	TArray<FSaved> Saved;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle PhasedHandle;
	FTimerHandle GhostTimer;
};
