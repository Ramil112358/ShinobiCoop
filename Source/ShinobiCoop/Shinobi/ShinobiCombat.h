#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class AShinobiCharacterBase;
class UAbilitySystemComponent;
class UGameplayEffect;

/** Server-side combat helpers shared by abilities and projectiles. Call them on the server only. */
namespace ShinobiCombat
{
	/**
	 * Applies GE_Damage (Data.Damage = Amount) from Source to Target and returns whether the hit landed: false
	 * with no source/target, or when the target is invulnerable. An invulnerable target still receives the
	 * effect so the damage exec can negate it and request Substitution; it just doesn't count as a hit (no
	 * chakra, knockback or status for the attacker).
	 */
	SHINOBICOOP_API bool ApplyDamage(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, float Amount);

	/**
	 * Applies a status effect (Burn, Shock, ...) from Source to Target, with Data.Damage = DamagePerTick for
	 * effects that deal damage. Skipped (returns false) for an invulnerable target, like a hit that didn't land.
	 */
	SHINOBICOOP_API bool ApplyStatus(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> Effect, float DamagePerTick = 0.f);

	/** Characters hostile to Caster whose location is within Radius (3D) of Center. */
	SHINOBICOOP_API TArray<AShinobiCharacterBase*> FindHostilesInRadius(const AShinobiCharacterBase* Caster, const FVector& Center, float Radius);
}
