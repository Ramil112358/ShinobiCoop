#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * The keyboard and mouse keys a player can rebind (Settings > Shortcuts): the 8 jutsu slots, the Skills screen and the core
 * actions. Alt is the only modifier. Gamepad buttons, WASD, the camera and Esc stay fixed. Saved per machine in
 * GameUserSettings.ini; UShinobiInputConfig::ApplyKeyBindings puts them into the mapping context.
 */
namespace ShinobiKeyBindings
{
	enum class EBindable : uint8 { Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7, Slot8, SkillsScreen, Melee, Dodge, Sprint, Block, ChakraCharge, Revive, LockOn, Jump, Dismount, Map, Num };

	struct FBinding
	{
		FKey Key;
		bool bAlt = false;
		bool operator==(const FBinding& Other) const { return Key == Other.Key && bAlt == Other.bAlt; }
	};

	SHINOBICOOP_API FBinding Default(EBindable Which);
	/** The saved binding, or the default when none is saved or the saved one is no longer allowed. */
	SHINOBICOOP_API FBinding Get(EBindable Which);

	/** Esc, the Windows keys, Alt+F4, Alt+Tab, WASD, right mouse, Alt itself, the wheel and axes, and every gamepad key. */
	SHINOBICOOP_API bool IsBlocked(const FBinding& Binding);

	enum class ERebind : uint8 { Done, Swapped, Blocked };
	/** Which gets New. If another binding already had New, it takes Which's old one (OutSwappedWith says which). Saves and applies. */
	SHINOBICOOP_API ERebind Rebind(EBindable Which, const FBinding& New, EBindable* OutSwappedWith = nullptr);
	SHINOBICOOP_API void ResetToDefaults();

	/** "Slot 1", "Skills screen", "Chakra charge"... */
	SHINOBICOOP_API FText Label(EBindable Which);
	/** "Alt+1", "Caps Lock", "Mouse 4"; bShort for the HUD's key chips ("Caps", "M4", "MMB"). */
	SHINOBICOOP_API FText KeyLabel(const FBinding& Binding, bool bShort = false);
}
