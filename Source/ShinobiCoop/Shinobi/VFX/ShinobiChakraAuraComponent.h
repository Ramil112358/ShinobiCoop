#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiChakraAuraComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UPointLightComponent;

/**
 * Chakra-charge aura (body-hugging, Task 3b): a thin blue glow applied as an overlay material on the
 * owner's own skeletal mesh (UMeshComponent::SetOverlayMaterial draws the mesh a second time with it),
 * inflated along the vertex normals so it hugs the silhouette, plus a soft light at the capsule centre. Purely
 * cosmetic and not replicated: it follows the owner's State.ChakraCharging tag, which GAS replicates to
 * every machine (ReplicateActivationOwnedTags), so all players see it, and it always matches the current
 * pose because it is drawn from the same skinned mesh (including Task 4's power-up pose).
 *
 * The overlay is applied once at BeginPlay and left in place for the component's lifetime; charging is
 * driven purely by the material's Fade parameter (0 = the additive overlay contributes nothing), never
 * by toggling SetOverlayMaterial. Toggling it triggers PSO precache + MarkRenderStateDirty on the
 * skinned mesh, which with r.PSOPrecache.ProxyCreationStrategy=1 can delay recreating the skinned proxy
 * in -game/packaged builds -- the character could vanish for a few frames on the first charge. The
 * always-applied overlay costs one extra translucent skinned draw per character, which is fine for the
 * 2-4 players this project targets.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiChakraAuraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShinobiChakraAuraComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True while the owner has State.ChakraCharging (always false on a dedicated server). */
	bool IsCharging() const { return bCharging; }

	/** 0 = hidden, 1 = fully faded in. */
	float GetFade() const { return Fade; }

	/** The dynamic overlay material instance, or nullptr if the aura material asset is missing. */
	UMaterialInstanceDynamic* GetAuraMaterial() const { return AuraMaterial; }

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	FLinearColor Color = FLinearColor(0.1f, 0.45f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float Intensity = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi", meta = (ClampMin = "0.01"))
	float FadeTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float LightIntensity = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float LightRadius = 250.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnChargingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void UpdateAura();

	/** The owner's skeletal mesh; the aura is applied to this mesh's overlay material slot. */
	UPROPERTY()
	TObjectPtr<UMeshComponent> BodyMesh;

	/** Dynamic instance of M_ChakraBodyAura; null (and nothing applied) if the asset is missing. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> AuraMaterial;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle TagEventHandle;
	bool bCharging = false;
	float Fade = 0.f;
	float Age = 0.f;
};
