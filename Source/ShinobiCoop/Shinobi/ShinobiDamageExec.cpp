#include "Shinobi/ShinobiDamageExec.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	void SendEvent(UAbilitySystemComponent* Target, const FGameplayTag& Tag, const AActor* Instigator, float Magnitude)
	{
		FGameplayEventData Payload;
		Payload.EventTag = Tag;
		Payload.Instigator = Instigator;
		Payload.Target = Target->GetAvatarActor();
		Payload.EventMagnitude = Magnitude;
		Target->HandleGameplayEvent(Tag, &Payload);
	}
}

void UShinobiDamageExec::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* Target = ExecutionParams.GetTargetAbilitySystemComponent();
	const UAbilitySystemComponent* Source = ExecutionParams.GetSourceAbilitySystemComponent();
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	float Damage = Spec.GetSetByCallerMagnitude(ShinobiTags::Data_Damage, false, 0.f);
	if (!Target || Damage <= 0.f)
	{
		return;
	}

	// Only direct hits send events. They come from instant effects, whose spec is the applier's own copy, so a
	// listener that removes effects from the target (e.g. chakra charge ending) can't invalidate it mid-execution.
	const bool bPeriodic = Spec.GetPeriod() > 0.f;
	if (!bPeriodic)
	{
		const AActor* Attacker = Source ? Source->GetAvatarActor() : nullptr;
		if (Target->HasMatchingGameplayTag(ShinobiTags::State_Invulnerable))
		{
			const bool bHitByOther = Attacker && Attacker != Target->GetAvatarActor();
			if (bHitByOther && Target->GetNumericAttribute(UShinobiAttributeSet::GetSubstitutionChargesAttribute()) >= 1.f)
			{
				SendEvent(Target, ShinobiTags::Event_Substitution, Attacker, Damage);
			}
			return;
		}
		if (Target->HasMatchingGameplayTag(ShinobiTags::State_ShockBonus))
		{
			Damage *= ShockBonusMultiplier;
			Target->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_ShockBonus));
		}
		if (Damage >= StaggerThreshold)
		{
			SendEvent(Target, ShinobiTags::Event_Hit_Stagger, Attacker, Damage);
		}
	}
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UShinobiAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage));
}
