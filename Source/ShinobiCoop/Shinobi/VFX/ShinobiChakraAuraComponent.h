#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiChakraAuraComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Body aura: a thin glow applied as an overlay material on the owner's own skeletal mesh
 * (UMeshComponent::SetOverlayMaterial draws the mesh a second time with it), inflated along the vertex normals
 * so it hugs the silhouette, plus a soft light at the capsule centre. It shows the owner's most important
 * state, in this order: black flame (dark purple), burn (orange), shock (flickering white-blue), chakra
 * charge (blue). While charging, chakra also flows around the body: soft wisps rise off it, streams spiral up
 * around it and a swirl turns on the ground (camera-facing / orbiting SpriteCook cards, T_ChakraFlow). Purely cosmetic and not replicated: it follows gameplay tags, which GAS replicates to every
 * machine, and it always matches the current pose because it is drawn from the same skinned mesh.
 *
 * The overlay is applied once at BeginPlay and left in place for the component's lifetime; showing and
 * hiding only drive the material's Fade parameter (0 = the additive overlay contributes nothing), never
 * SetOverlayMaterial. Toggling it triggers PSO precache + MarkRenderStateDirty on the skinned mesh, which
 * with r.PSOPrecache.ProxyCreationStrategy=1 can delay recreating the skinned proxy in -game/packaged
 * builds -- the character could vanish for a few frames. The always-applied overlay costs one extra
 * translucent skinned draw per character.
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

	/** The state tag the aura currently shows, or an invalid tag when nothing is active. */
	FGameplayTag GetShownTag() const;

	/** 0 = hidden, 1 = fully faded in. */
	float GetFade() const { return Fade; }

	/** How many of the flowing chakra cards (wisps, streams, ground swirl) are visible. */
	int32 NumVisibleFlowCards() const;

	/** The dynamic overlay material instance, or nullptr if the aura material asset is missing. */
	UMaterialInstanceDynamic* GetAuraMaterial() const { return AuraMaterial; }

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
	void OnStateTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RefreshShownState();
	void UpdateAura();
	void CreateFlowCards();
	void UpdateFlow(float DeltaTime);

	/** The owner's skeletal mesh; the aura is applied to this mesh's overlay material slot. */
	UPROPERTY()
	TObjectPtr<UMeshComponent> BodyMesh;

	/** Dynamic instance of M_ChakraBodyAura; null (and nothing applied) if the asset is missing. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> AuraMaterial;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	/** Chakra flow while charging: wisps first, then the orbiting streams, then the ground swirl. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FlowCards;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FlowMaterials;

	/** Per card (not the ground swirl): life 0-1, life length in seconds, orbit angle (degrees), radius and start height. */
	struct FFlowCard
	{
		float Life = 1.f;
		float Duration = 1.f;
		float Angle = 0.f;
		float Radius = 0.f;
		float Height = 0.f;
	};
	TArray<FFlowCard> Flow;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TArray<FDelegateHandle> TagEventHandles;
	/** Index into the look table (see the .cpp) of the state being shown; INDEX_NONE when nothing is active. */
	int32 ShownIndex = INDEX_NONE;
	bool bCharging = false;
	float Fade = 0.f;
	/** Fade of the chakra flow, 0-1: follows charging alone (a burn can take over the body glow while the flow goes on). */
	float FlowFade = 0.f;
	/** Half the capsule height: the flow spans the body from the feet (-HalfHeight) up. */
	float HalfHeight = 90.f;
	float Age = 0.f;
};
