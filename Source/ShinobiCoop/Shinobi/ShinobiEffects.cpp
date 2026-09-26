#include "Shinobi/ShinobiEffects.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiDamageExec.h"
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

void UShinobiEffect::AddDamageExecution()
{
	FGameplayEffectExecutionDefinition& Execution = Executions.AddDefaulted_GetRef();
	Execution.CalculationClass = UShinobiDamageExec::StaticClass();
}

void UShinobiEffect::SetPeriodic(float PeriodSeconds)
{
	Period = FScalableFloat(PeriodSeconds);
	bExecutePeriodicEffectOnApplication = false;
}

void UShinobiEffect::UseSingleRefreshingStack()
{
	// Direct access to StackingType is deprecated since 5.7 and its setter is editor-only, so constructors still
	// assign it directly, the same way UGameplayEffect's own constructor does.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;
}

UGE_Damage::UGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddDamageExecution();
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
	// Resume immediately when the block is lifted instead of waiting up to one more period.
	// bExecutePeriodicEffectOnApplication defaults to true, which would also fire immediately when this
	// effect is first applied at spawn; ExecuteAndResetPeriod already provides that first free tick via the
	// initial un-inhibit (every new active effect starts inhibited and is un-inhibited on the same apply), so
	// leaving both on would double-execute at t=0.
	PeriodicInhibitionPolicy = EGameplayEffectPeriodInhibitionRemovedPolicy::ExecuteAndResetPeriod;
	bExecutePeriodicEffectOnApplication = false;
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
	// bExecutePeriodicEffectOnApplication is deliberately left at its default (true), the opposite of
	// UGE_ChakraCharge: sprint should cost something for any tap, however short, instead of letting
	// taps under 0.25 s drain for free.
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
	// Without this, bExecutePeriodicEffectOnApplication's default (true) grants a free +6.25 on every
	// activation, so tapping the ability repeatedly beats holding it. Gains should only come from a full period.
	bExecutePeriodicEffectOnApplication = false;
}

UGE_Burn::UGE_Burn()
{
	SetDuration(4.f);
	SetPeriodic(1.f);
	AddDamageExecution();
	GrantTag(ShinobiTags::State_Burning_Normal);
	UseSingleRefreshingStack();
}

UGE_BlackFlame::UGE_BlackFlame()
{
	SetDuration(10.f);
	SetPeriodic(1.f);
	AddDamageExecution();
	GrantTag(ShinobiTags::State_Burning_Black);
	UseSingleRefreshingStack();
}

UGE_Shock::UGE_Shock()
{
	SetDuration(1.5f);
	GrantTag(ShinobiTags::State_Shocked);
	UseSingleRefreshingStack();
}

UGE_ShockBonus::UGE_ShockBonus()
{
	SetDuration(10.f);
	GrantTag(ShinobiTags::State_ShockBonus);
	UseSingleRefreshingStack();
}

UGE_Stagger::UGE_Stagger()
{
	SetDuration(0.5f);
	GrantTag(ShinobiTags::State_Staggered);
	UseSingleRefreshingStack();
}

UGE_JutsuCooldown::UGE_JutsuCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ShinobiTags::Data_Cooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

UGE_SubstitutionCost::UGE_SubstitutionCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddModifier(UShinobiAttributeSet::GetSubstitutionChargesAttribute(), -1.f);
}

UGE_SubstitutionRegen::UGE_SubstitutionRegen()
{
	SetInfinitePeriodic(15.f);
	AddModifier(UShinobiAttributeSet::GetSubstitutionChargesAttribute(), 1.f);
	bExecutePeriodicEffectOnApplication = false; // a fresh spawn already has full charges
}
