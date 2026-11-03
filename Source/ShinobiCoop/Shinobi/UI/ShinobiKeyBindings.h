#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * The keys a player can rebind (the CONTROLS tab): the keyboard/mouse key of the 8 jutsu slots, the Skills screen and the
 * core actions (Alt is the only modifier), and the gamepad button of each jutsu slot (pressed while RB is held). The other
 * gamepad buttons, WASD, the camera and Esc stay fixed. Saved per machine in GameUserSettings.ini;
 * UShinobiInputConfig::ApplyKeyBindings puts them into the mapping contexts.
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

	/** "Slot 1", "Jutsu loadout", "Chakra charge"... */
	SHINOBICOOP_API FText Label(EBindable Which);
	/** "Alt+1", "Caps Lock", "Mouse 4"; bShort for the HUD's key chips ("Caps", "M4", "MMB"). */
	SHINOBICOOP_API FText KeyLabel(const FBinding& Binding, bool bShort = false);

	/** The gamepad: each jutsu slot casts on RB + its button. */
	constexpr int32 NumPadSlots = 8;
	/** Slots 1-4 on A / B / X / Y, slots 5-8 on D-pad up / right / down / left. */
	SHINOBICOOP_API FKey PadDefault(int32 Slot);
	/** The saved button, or the default when none is saved or the saved one is no longer allowed. */
	SHINOBICOOP_API FKey GetPad(int32 Slot);
	/** Allowed with RB: the face buttons, the D-pad, LB, LT, RT and L3. Not RB itself, R3 (lock-on), Start / View or the sticks. */
	SHINOBICOOP_API bool IsPadAllowed(const FKey& Button);
	/** Slot gets Button. If another slot had it, that slot takes Slot's old one (OutSwappedWith says which). Saves and applies. */
	SHINOBICOOP_API ERebind RebindPad(int32 Slot, const FKey& Button, int32* OutSwappedWith = nullptr);
	/** "A", "D-pad Up", "LT"; with bChord, RB first: "RB + A" (bShort: "RB+A", or an arrow for the D-pad), how the HUD and menus show a slot. */
	SHINOBICOOP_API FText PadLabel(const FKey& Button, bool bShort = false, bool bChord = true);
}
