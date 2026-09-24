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
