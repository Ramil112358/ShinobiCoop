#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiCastFxComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/** The pieces a cast can show (for tests and the per-jutsu table). */
enum class EShinobiCastPart : uint8
{
	HandOrb,        // glowing sphere and light in the right hand (Chidori, Rasengan)
	RasenganShells, // two swirl cards turning against each other over the orb
	LightningArcs,  // crackling bolts around the orb (Chidori)
	MouthFlame,     // flame at the mouth while a Fireball charges
	EyeGlow,        // red eyes (Amaterasu)
	BloodStreaks,   // blood running from the eyes (Amaterasu)
	ChidoriRing,    // a ring of lightning on the ground while Chidori charges
};

/**
 * What a jutsu looks like on the caster while it's active (seals, charge and, for the dashes, the run-in):
 * - Fireball: a flame at the mouth while charging, growing with the charge.
 * - Chidori: a crackling ball of lightning in the right hand (a flickering SpriteCook card over a glow orb) with
 *   lightning arcs that lengthen with the charge (some reach the ground), and a ring of lightning on the ground around
 *   the caster while charging.
 * - Rasengan: a swirling chakra sphere (two SpriteCook cards turning against each other over a glow core), held above
 *   the palm and growing with the charge.
 * - Amaterasu: glowing red eyes, with blood running down from them during the seals.
 * Cosmetic only. It follows the State.Casting.* tags each jutsu grants while active (plus State.Casting.Charging),
 * which GAS replicates to every machine; the charge size runs on this machine's clock from when the Charging tag
 * appeared.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiCastFxComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShinobiCastFxComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The casting tag being shown, or an invalid tag when nothing is. */
	FGameplayTag GetShownTag() const;

	bool IsPartVisible(EShinobiCastPart Part) const;

	/** Centre of the hand orb (world space). */
	FVector GetOrbLocation() const;

	/** The Rasengan's outer wind layer, world radius round GetOrbLocation(); 0 unless the Rasengan is shown. */
	float GetRasenganOuterRadius() const;

	/** 1 + 0.8 x the charge so far (by this machine's clock since State.Casting.Charging appeared), frozen at the
	 * release; 1 when not charging. */
	float GetChargeVisualScale() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnCastingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnChargingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void Refresh();
	void UpdateHead(float ChargeScale);
	void UpdateArcs(float ChargeScale);
	void UpdateRing(float ChargeScale);
	void UpdateOrbCards();

	/** The middle of the right palm, between the wrist and the knuckles (the wrist if the rig has no finger bones). */
	FVector GetPalmCentre() const;

	/** 0 while the Rasengan forms, rising to 1 as its release (the thrust, palm to the front) begins. */
	float GetRasenganThrust() const;
	UStaticMeshComponent* MakeMesh(const TCHAR* Name, const TCHAR* MeshPath, UMaterialInstanceDynamic* Material, USceneComponent* Parent, FName Socket = NAME_None);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Orb;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> OrbMaterial;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Shells;

	/** Chidori's crackling ball (a SpriteCook card), over the orb with the arcs. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ChidoriCard;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ArcSegments;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> RingSegments;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> RingLight;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MouthFlame;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> MouthLight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Eyes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Streaks;

	/** The right hand bone's children (the fingers' roots): Chidori is held between them and the wrist. */
	TArray<FName> PalmBones;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TArray<FDelegateHandle> TagEventHandles;
	FDelegateHandle ChargingHandle;
	int32 ShownIndex = INDEX_NONE;
	float Age = 0.f;
	bool bCharging = false;
	float ChargeAge = 0.f;
	float NextArcShuffle = 0.f;
};
