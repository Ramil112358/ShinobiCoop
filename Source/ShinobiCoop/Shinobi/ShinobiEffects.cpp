#include "Shinobi/ShinobiEffects.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiTags.h"

void UShinobiEffect::AddModifier(const FGameplayAttribute& Attribute, float Magnitude)
{
	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = Attribute;
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
}

void UShinobiEffect::AddSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
{
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = DataTag;
	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = Attribute;
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

void UShinobiEffect::SetDuration(float Seconds)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Seconds));
}

void UShinobiEffect::SetInfinitePeriodic(float PeriodSeconds)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(PeriodSeconds);
}

void UShinobiEffect::GrantTag(const FGameplayTag& Tag)
{
	if (!TargetTagsComponent)
	{
		TargetTagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
		GEComponents.Add(TargetTagsComponent);
	}
	FInheritedTagContainer Tags = TargetTagsComponent->GetConfiguredTargetTagChanges();
	Tags.AddTag(Tag);
	TargetTagsComponent->SetAndApplyTargetTagChanges(Tags);
}

UTargetTagRequirementsGameplayEffectComponent& UShinobiEffect::Requirements()
{
	if (!RequirementsComponent)
	{
		RequirementsComponent = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TagRequirements"));
		GEComponents.Add(RequirementsComponent);
	}
	return *RequirementsComponent;
}

void UShinobiEffect::IgnoreApplicationIfTargetHas(const FGameplayTag& Tag)
{
	Requirements().ApplicationTagRequirements.IgnoreTags.AddTag(Tag);
}

void UShinobiEffect::InhibitWhileTargetHas(const FGameplayTag& Tag)
{
	Requirements().OngoingTagRequirements.IgnoreTags.AddTag(Tag);
}

UGE_Damage::UGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCallerModifier(UShinobiAttributeSet::GetIncomingDamageAttribute(), ShinobiTags::Data_Damage);
	IgnoreApplicationIfTargetHas(ShinobiTags::State_Invulnerable);
}

UGE_ChakraGain::UGE_ChakraGain()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCallerModifier(UShinobiAttributeSet::GetChakraAttribute(), ShinobiTags::Data_ChakraGain);
}

// Rates are applied every 0.25 s, so per-period values are a quarter of the per-second rate.
UGE_ChakraRegen::UGE_ChakraRegen()
{
	SetInfinitePeriodic(0.25f);
	AddModifier(UShinobiAttributeSet::GetChakraAttribute(), 0.5f);
}

UGE_StaminaRegen::UGE_StaminaRegen()
{
	SetInfinitePeriodic(0.25f);
	AddModifier(UShinobiAttributeSet::GetStaminaAttribute(), 5.f);
	InhibitWhileTargetHas(ShinobiTags::State_StaminaRegenBlocked);
}

UGE_StaminaRegenDelay::UGE_StaminaRegenDelay()
{
	SetDuration(1.f);
	GrantTag(ShinobiTags::State_StaminaRegenBlocked);
}

UGE_SprintDrain::UGE_SprintDrain()
{
	SetInfinitePeriodic(0.25f);
	AddModifier(UShinobiAttributeSet::GetStaminaAttribute(), -2.5f);
	GrantTag(ShinobiTags::State_StaminaRegenBlocked);
	GrantTag(ShinobiTags::State_Sprinting);
}

UGE_DodgeCost::UGE_DodgeCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddModifier(UShinobiAttributeSet::GetStaminaAttribute(), -25.f);
}

UGE_DodgeIFrames::UGE_DodgeIFrames()
{
	SetDuration(0.2f);
	GrantTag(ShinobiTags::State_Invulnerable);
}

UGE_ChakraCharge::UGE_ChakraCharge()
{
	SetInfinitePeriodic(0.25f);
	AddModifier(UShinobiAttributeSet::GetChakraAttribute(), 6.25f);
}
