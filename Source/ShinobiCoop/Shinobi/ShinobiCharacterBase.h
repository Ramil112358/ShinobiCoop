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
class UShinobiCastFxComponent;
class UShinobiChakraAuraComponent;
class UShinobiFeedbackComponent;

UENUM(BlueprintType)
enum class EShinobiTeam : uint8
{
	Players,
	Enemies
};

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

	EShinobiTeam GetTeam() const { return Team; }
	bool IsHostileTo(const AShinobiCharacterBase* Other) const { return Other && Other != this && Other->Team != Team; }

	/** False once downed or dead: out of the fight, so not a target. */
	bool IsAlive() const;

	/** Stuns (Shock) last this many times their normal duration (the boss shrugs them off faster). */
	float GetStunMultiplier() const { return StunMultiplier; }

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	/** Server only. Called each time Health reaches 0. */
	virtual void HandleOutOfHealth();

	/** Constructor-only: Manny (or Quinn) mesh + combat anim blueprint from the template content. */
	void UseMannequinMesh(bool bQuinn = false);

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

	/** Damage numbers, hit-stop, camera shake and sounds (cosmetic, every machine that renders). */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiFeedbackComponent> Feedback;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	EShinobiTeam Team = EShinobiTeam::Enemies;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSubclassOf<UShinobiGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

private:
	void GrantStartupAbilitiesAndEffects();

	/** Every machine: State.Dead turns the body into a ragdoll with no capsule collision or movement. */
	void OnDeadTagChanged(const FGameplayTag Tag, int32 Count);
	bool bStartupGranted = false;
};
