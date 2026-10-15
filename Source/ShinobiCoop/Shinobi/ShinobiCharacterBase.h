#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "ShinobiCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UShinobiAttributeSet;
class UShinobiGameplayAbility;
class UShinobiBodyFlameComponent;
class UShinobiCastFxComponent;
class UShinobiChakraAuraComponent;
class UShinobiFeedbackComponent;
class UShinobiPhaseFxComponent;
class UShinobiPhysicalReactionComponent;
class UShinobiPerfectSusanooComponent;

UENUM(BlueprintType)
enum class EShinobiTeam : uint8
{
	Players,
	Enemies,
	/** PvP arena sides (AShinobiPvpGameMode): hostile to each other and to Enemies, never to their own side. */
	TeamA,
	TeamB
};

inline bool IsPvpTeam(EShinobiTeam Team)
{
	return Team == EShinobiTeam::TeamA || Team == EShinobiTeam::TeamB;
}

/** Character with an ability system, shared by players, enemies and dummies. */
UCLASS()
class SHINOBICOOP_API AShinobiCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AShinobiCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UShinobiAttributeSet* GetAttributes() const { return Attributes; }

	/** Body aura: chakra charge and status glows (burn, black flame, shock). */
	UShinobiChakraAuraComponent* GetChakraAura() const { return ChakraAura; }
	/** Hand orb shown while a jutsu is active. */
	UShinobiCastFxComponent* GetCastFx() const { return CastFx; }

	UShinobiBodyFlameComponent* GetBodyFlames() const { return BodyFlames; }

	/** Susanoo's figure (the Perfect Susanoo) while the armour is up. */
	UShinobiPerfectSusanooComponent* GetSusanoo() const { return Susanoo; }

	EShinobiTeam GetTeam() const { return Team; }
	/** Server: changes sides, at any time (replicated to every client, also after the pawn first replicated), and re-outlines
	 * for every viewer. */
	void SetTeam(EShinobiTeam NewTeam);
	bool IsHostileTo(const AShinobiCharacterBase* Other) const { return Other && Other != this && Other->Team != Team; }

	/** The mesh you see: the hero for players (AShinobiPlayerCharacter), else GetMesh(). Never null. Auras, the cast
	 * orb and the death ragdoll act on it; animation always runs on GetMesh(). */
	virtual USkeletalMeshComponent* GetBodyMesh() const { return GetMesh(); }

	/** The body mesh's right-hand and head bones: "hand_r"/"head" on Manny, "RightHand"/"Head" on a Mixamo-rigged hero. */
	FName GetRightHandBone() const;
	FName GetHeadBone() const;

	/** False once downed or dead: out of the fight, so not a target. */
	bool IsAlive() const;

	/** Kamui: untouchable and unseen by the AI. */
	bool IsPhased() const;

	/** What the AI may aim at: alive and not phased. */
	bool IsTargetable() const { return IsAlive() && !IsPhased(); }

	/** The character the local player controls (the first local player controller's pawn), or null: a dedicated server, or
	 * nobody possessed yet. Outlines and damage-number colours are relative to it. */
	static const AShinobiCharacterBase* GetLocalViewer(const UWorld* World);

	/** Outlines the body red when hostile to the local viewer, else dark. No viewer: the old static rule (Enemies are red). */
	void RefreshOutline();
	/** RefreshOutline against a given viewer. */
	virtual void ApplyOutlineFor(const AShinobiCharacterBase* Viewer);
	/** Re-outlines every character in World; call when the local pawn changes. */
	static void RefreshAllOutlines(UWorld* World);

	/** Stuns (Shock) last this many times their normal duration (the boss shrugs them off faster). */
	float GetStunMultiplier() const { return StunMultiplier; }

	/** Physical hit reactions (humanoids, set up by UseMannequinMesh); null for other bodies. */
	UShinobiPhysicalReactionComponent* GetPhysicalReactions() const { return PhysicalReactions; }

	/** Hard launches (the server's, at least the reactions' KnockdownSpeed) also knock the character down. */
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride) override;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	/** The outline stencil for a viewer (see RefreshOutline). */
	uint8 OutlineStencilFor(const AShinobiCharacterBase* Viewer) const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only. Called each time Health reaches 0. */
	virtual void HandleOutOfHealth();

	/** Constructor-only: Manny (or Quinn) mesh + combat anim blueprint from the template content, swapped for the
	 * motion-matching AnimBP in BeginPlay. */
	void UseMannequinMesh(bool bQuinn = false);

	/** Set by UseMannequinMesh: BeginPlay gives the mesh the sample's motion-matching AnimBP (SandboxCharacter_CMC_ABP). */
	bool bUseLocomotionAnim = false;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi", meta = (ClampMin = "0"))
	float StunMultiplier = 1.f;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UShinobiAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiChakraAuraComponent> ChakraAura;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiCastFxComponent> CastFx;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiBodyFlameComponent> BodyFlames;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiPhysicalReactionComponent> PhysicalReactions;

	/** Damage numbers, hit-stop, camera shake and sounds (cosmetic, every machine that renders). */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiFeedbackComponent> Feedback;

	/** Kamui's ghost look (cosmetic, every machine that renders). */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiPhaseFxComponent> PhaseFx;

	/** Susanoo's figure (cosmetic, every machine that renders). */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiPerfectSusanooComponent> Susanoo;

	UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_Team, Category = "Shinobi")
	EShinobiTeam Team = EShinobiTeam::Enemies;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSubclassOf<UShinobiGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

private:
	UFUNCTION()
	void OnRep_Team();

	/** The sides changed: this body is outlined afresh, and so is everyone else when this is the viewer's own body. */
	void OnTeamChanged();

	void GrantStartupAbilitiesAndEffects();

	/** Every machine: State.Dead turns the body into a ragdoll with no capsule collision or movement. */
	void OnDeadTagChanged(const FGameplayTag Tag, int32 Count);

	/** Every machine: Kamui makes the capsule ignore pawns (walk through enemies); on the server, ending it nudges the
	 * character out of anything it overlaps. */
	void OnPhasedTagChanged(const FGameplayTag Tag, int32 Count);
	ECollisionResponse PrePhasePawnResponse = ECR_Block;
	bool bStartupGranted = false;
};
