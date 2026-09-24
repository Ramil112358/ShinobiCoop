#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ShinobiCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UShinobiAttributeSet;
class UShinobiGameplayAbility;

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

	EShinobiTeam GetTeam() const { return Team; }
	bool IsHostileTo(const AShinobiCharacterBase* Other) const { return Other && Other != this && Other->Team != Team; }

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	/** Server only. Called each time Health reaches 0. */
	virtual void HandleOutOfHealth();

	/** Constructor-only: Manny mesh + combat anim blueprint from the template content. */
	void UseMannequinMesh();

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UShinobiAttributeSet> Attributes;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	EShinobiTeam Team = EShinobiTeam::Enemies;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSubclassOf<UShinobiGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

private:
	void GrantStartupAbilitiesAndEffects();
	bool bStartupGranted = false;
};
