#include "Shinobi/ShinobiCombat.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

namespace ShinobiCombat
{
	namespace
	{
		bool IsInvulnerable(const AShinobiCharacterBase* Target)
		{
			return Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Invulnerable);
		}

		void Apply(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> Effect, float Damage)
		{
			const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(Effect, 1.f, Source->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Damage, Damage);
			Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->GetAbilitySystemComponent());
		}
	}

	bool ApplyDamage(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, float Amount)
	{
		if (!Source || !Target)
		{
			return false;
		}
		// Decided before applying: the exec negates hits on invulnerable targets, so this is what "landed" means.
		const bool bLanded = !IsInvulnerable(Target);
		Apply(Source, Target, UGE_Damage::StaticClass(), Amount);
		return bLanded;
	}

	bool ApplyStatus(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> Effect, float DamagePerTick)
	{
		if (!Source || !Target || !Effect || IsInvulnerable(Target))
		{
			return false;
		}
		Apply(Source, Target, Effect, DamagePerTick);
		return true;
	}

	TArray<AShinobiCharacterBase*> FindHostilesInRadius(const AShinobiCharacterBase* Caster, const FVector& Center, float Radius)
	{
		TArray<AShinobiCharacterBase*> Result;
		if (!Caster)
		{
			return Result;
		}
		// ponytail: linear scan over all characters like FindHostilesInCone; switch to an overlap query if counts grow
		for (TActorIterator<AShinobiCharacterBase> It(Caster->GetWorld()); It; ++It)
		{
			if (Caster->IsHostileTo(*It) && FVector::Dist(It->GetActorLocation(), Center) <= Radius)
			{
				Result.Add(*It);
			}
		}
		return Result;
	}
}
