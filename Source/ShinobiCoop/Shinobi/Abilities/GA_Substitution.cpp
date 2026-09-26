#include "Shinobi/Abilities/GA_Substitution.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Substitution::UGA_Substitution()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Substitution));
	// Otherwise the dodge's root-motion force keeps pushing the teleported character.
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Dodge);
	CostGameplayEffectClass = UGE_SubstitutionCost::StaticClass();

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ShinobiTags::Event_Substitution;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_Substitution::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	const AShinobiCharacterBase* Attacker = TriggerEventData ? Cast<AShinobiCharacterBase>(TriggerEventData->Instigator.Get()) : nullptr;
	if (!Self || !Attacker || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector Behind = Attacker->GetActorLocation() - Attacker->GetActorForwardVector().GetSafeNormal2D() * BehindDistance;
	const FRotator FaceAttacker(0.f, (Attacker->GetActorLocation() - Behind).Rotation().Yaw, 0.f);
	Self->TeleportTo(Behind, FaceAttacker); // stays put if something blocks the spot; the hit is negated either way

	ShinobiCombat::ApplyStatus(GetAbilitySystemComponentFromActorInfo(), Attacker, UGE_Stagger::StaticClass());
	FGameplayEventData Stagger;
	Stagger.EventTag = ShinobiTags::Event_Hit_Stagger;
	Stagger.Instigator = Self;
	Stagger.Target = Attacker;
	Attacker->GetAbilitySystemComponent()->HandleGameplayEvent(Stagger.EventTag, &Stagger); // interrupts their wind-up or charge too

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
