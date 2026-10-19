#include "Shinobi/ShinobiCombat.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiOwl.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

namespace ShinobiCombat
{
	namespace
	{
		bool IsInvulnerable(const AShinobiCharacterBase* Target)
		{
			const UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
			return ASC->HasMatchingGameplayTag(ShinobiTags::State_Invulnerable) || ASC->HasMatchingGameplayTag(ShinobiTags::State_Phased);
		}

		void Apply(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> Effect, float Damage)
		{
			const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(Effect, 1.f, Source->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Damage, Damage);
			Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->GetAbilitySystemComponent());
		}
	}

	void NoteCombat(const AActor* Instigator, AShinobiCharacterBase* Victim)
	{
		const AShinobiCharacterBase* Source = Cast<AShinobiCharacterBase>(Instigator);
		if (const AShinobiSummon* Summon = Cast<AShinobiSummon>(Instigator))
		{
			Source = Summon->GetSummoner(); // the summoner fights through their summon
		}
		else if (const AShinobiOwl* Owl = Cast<AShinobiOwl>(Instigator))
		{
			Source = Owl->GetSummoner();
		}
		if (!Source || !Victim || !Victim->IsHostileTo(Source))
		{
			return; // self-damage, a teammate, the world
		}
		if (AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(Victim))
		{
			Player->NoteCombat();
		}
		if (AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(const_cast<AShinobiCharacterBase*>(Source)))
		{
			Player->NoteCombat();
		}
	}

	bool ApplyDamage(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, float Amount, bool bStagger)
	{
		if (!Source || !Target)
		{
			return false;
		}
		// Decided before applying: the exec negates hits on invulnerable targets, so this is what "landed" means.
		const bool bLanded = !IsInvulnerable(Target);
		Apply(Source, Target, bStagger ? UGE_Damage::StaticClass() : UGE_DamageNoStagger::StaticClass(), Amount);
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

	bool ApplyStun(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target)
	{
		if (!Source || !Target || IsInvulnerable(Target))
		{
			return false;
		}
		const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(UGE_Shock::StaticClass(), 1.f, Source->MakeEffectContext());
		Spec.Data->SetDuration(UGE_Shock::Seconds * Target->GetStunMultiplier(), true);
		Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->GetAbilitySystemComponent());
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
			if (Caster->IsHostileTo(*It) && It->IsAlive() && FVector::Dist(It->GetActorLocation(), Center) <= Radius)
			{
				Result.Add(*It);
			}
		}
		return Result;
	}
}
