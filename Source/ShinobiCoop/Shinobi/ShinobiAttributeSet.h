#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ShinobiAttributeSet.generated.h"

DECLARE_MULTICAST_DELEGATE(FShinobiOutOfHealthDelegate);

/** Health, chakra, stamina and substitution charges shared by players and enemies. */
UCLASS()
class SHINOBICOOP_API UShinobiAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UShinobiAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/**
	 * Server only. Fires each time Health goes from above 0 to 0.
	 * Broadcast from inside PostAttributeChange, i.e. mid gameplay-effect-execute: handlers must
	 * not change Health synchronously from here. Defer (timer / next tick) if a handler needs to.
	 */
	FShinobiOutOfHealthDelegate OnOutOfHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Shinobi")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Shinobi")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Chakra, Category = "Shinobi")
	FGameplayAttributeData Chakra;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, Chakra)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxChakra, Category = "Shinobi")
	FGameplayAttributeData MaxChakra;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, MaxChakra)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Shinobi")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Shinobi")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SubstitutionCharges, Category = "Shinobi")
	FGameplayAttributeData SubstitutionCharges;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, SubstitutionCharges)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSubstitutionCharges, Category = "Shinobi")
	FGameplayAttributeData MaxSubstitutionCharges;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, MaxSubstitutionCharges)

	/** Meta attribute: damage waiting to be applied to Health. Server only, never replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "Shinobi")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, IncomingDamage)

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Chakra(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxChakra(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SubstitutionCharges(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxSubstitutionCharges(const FGameplayAttributeData& OldValue);

private:
	void ClampToMax(const FGameplayAttribute& Attribute, float& NewValue) const;
};
