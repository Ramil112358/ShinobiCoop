#pragma once

#include "NativeGameplayTags.h"

namespace ShinobiTags
{
	// SetByCaller magnitudes
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_ChakraGain);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Cooldown);

	// Gameplay events (sent on the server by UShinobiDamageExec and GA_Substitution)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Stagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Substitution);

	// Character states
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_StaminaRegenBlocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ChakraCharging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Burning);        // parent: matches either burn
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Burning_Normal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Burning_Black);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Shocked);        // stunned
	// Not "State.Shocked.Bonus": a child tag matches its parent, so it would count as a stun.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ShockBonus);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Staggered);

	// Ability identity (asset tags)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ChakraCharge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Jutsu);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Substitution);

	// Jutsu cooldowns (granted by GE_JutsuCooldown)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Jutsu_Fireball);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Jutsu_Chidori);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Jutsu_Rasengan);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Jutsu_Amaterasu);

	// Activation failure reasons
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fail_NoTarget);
}
