#pragma once

#include "NativeGameplayTags.h"

namespace ShinobiTags
{
	// SetByCaller magnitudes
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_ChakraGain);

	// Character states
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_StaminaRegenBlocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ChakraCharging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);

	// Ability identity (asset tags)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ChakraCharge);
}
