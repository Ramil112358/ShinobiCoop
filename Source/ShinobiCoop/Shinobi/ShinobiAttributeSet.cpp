#include "Shinobi/ShinobiAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UShinobiAttributeSet::UShinobiAttributeSet()
{
	InitHealth(500.f);
	InitMaxHealth(500.f);
	InitChakra(100.f);
	InitMaxChakra(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitSubstitutionCharges(3.f);
	InitMaxSubstitutionCharges(3.f);
	InitShield(0.f);
	InitIncomingDamage(0.f);
}

void UShinobiAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, Chakra, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, MaxChakra, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, SubstitutionCharges, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, MaxSubstitutionCharges, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, Shield, COND_None, REPNOTIFY_Always);
}

void UShinobiAttributeSet::ClampToMax(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetChakraAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxChakra());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetSubstitutionChargesAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxSubstitutionCharges());
	}
}

void UShinobiAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampToMax(Attribute, NewValue);
}

void UShinobiAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampToMax(Attribute, NewValue);
}

void UShinobiAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	const AActor* Owner = GetOwningActor();
	const bool bAuthority = Owner && Owner->HasAuthority();

	// When a Max attribute shrinks, force the corresponding current value back down to the new
	// cap. ApplyModToAttribute does not recurse into Pre/PostGameplayEffectExecute, so this is
	// safe to call from here, and it internally no-ops on non-authority.
	if (bAuthority)
	{
		if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
		{
			GetOwningAbilitySystemComponent()->ApplyModToAttribute(GetHealthAttribute(), EGameplayModOp::Override, NewValue);
		}
		else if (Attribute == GetMaxChakraAttribute() && GetChakra() > NewValue)
		{
			GetOwningAbilitySystemComponent()->ApplyModToAttribute(GetChakraAttribute(), EGameplayModOp::Override, NewValue);
		}
		else if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
		{
			GetOwningAbilitySystemComponent()->ApplyModToAttribute(GetStaminaAttribute(), EGameplayModOp::Override, NewValue);
		}
		else if (Attribute == GetMaxSubstitutionChargesAttribute() && GetSubstitutionCharges() > NewValue)
		{
			GetOwningAbilitySystemComponent()->ApplyModToAttribute(GetSubstitutionChargesAttribute(), EGameplayModOp::Override, NewValue);
		}
	}

	if (Attribute == GetHealthAttribute() && OldValue > 0.f && NewValue <= 0.f && bAuthority)
	{
		OnOutOfHealth.Broadcast();
	}
}

void UShinobiAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage > 0.f)
		{
			SetHealth(GetHealth() - Damage); // clamped to [0, MaxHealth] in PreAttributeBaseChange
		}
	}
}

void UShinobiAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, Health, OldValue); }
void UShinobiAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, MaxHealth, OldValue); }
void UShinobiAttributeSet::OnRep_Chakra(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, Chakra, OldValue); }
void UShinobiAttributeSet::OnRep_MaxChakra(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, MaxChakra, OldValue); }
void UShinobiAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, Stamina, OldValue); }
void UShinobiAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, MaxStamina, OldValue); }
void UShinobiAttributeSet::OnRep_SubstitutionCharges(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, SubstitutionCharges, OldValue); }
void UShinobiAttributeSet::OnRep_MaxSubstitutionCharges(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, MaxSubstitutionCharges, OldValue); }
void UShinobiAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, Shield, OldValue); }
