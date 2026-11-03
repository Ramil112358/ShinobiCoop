#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * What a player can rebind (the CONTROLS tab), on both devices: the 8 jutsu slots, the Skills screen and every action but
 * moving, the camera and Pause. A keyboard/mouse binding is a key or Alt + key. A controller binding is a button, or RB
 * or RT held + a button (RB and RT never act alone). Esc / Menu always pause. Saved per machine in GameUserSettings.ini;
 * UShinobiInputConfig::ApplyKeyBindings puts them into the mapping context.
 */
namespace ShinobiKeyBindings
{
	enum class EBindable : uint8 { Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7, Slot8, SkillsScreen, Melee, Dodge, Sprint, Block, ChakraCharge, Revive, LockOn, Jump, Dismount, Map, MapZoomIn, MapZoomOut, Num };

	enum class ERebind : uint8 { Done, Swapped, Blocked };

	/** "Slot 1", "Jutsu loadout", "Chakra charge"... */
	SHINOBICOOP_API FText Label(EBindable Which);

	// ---- Keyboard and mouse ----

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

	/** Which gets New. If another binding already had New, it takes Which's old one (OutSwappedWith says which). Saves and applies. */
	SHINOBICOOP_API ERebind Rebind(EBindable Which, const FBinding& New, EBindable* OutSwappedWith = nullptr);
	/** "Alt+1", "Caps Lock", "Mouse 4"; bShort for the HUD's key chips ("Caps", "M4", "MMB"). */
	SHINOBICOOP_API FText KeyLabel(const FBinding& Binding, bool bShort = false);

	// ---- Controller ----

	struct FPadBinding
	{
		/** RB or RT, held first; invalid for a lone button. */
		FKey Hold;
		/** Invalid: nothing on the controller. */
		FKey Button;
		bool IsBound() const { return Button.IsValid(); }
		bool operator==(const FPadBinding& Other) const { return Hold == Other.Hold && Button == Other.Button; }
	};

	SHINOBICOOP_API FPadBinding PadDefault(EBindable Which);
	/** The saved binding (possibly none), or the default when none is saved or the saved one is no longer allowed. */
	SHINOBICOOP_API FPadBinding GetPad(EBindable Which);
	/** RB and RT: only ever held first, for a two-button combo. */
	SHINOBICOOP_API bool IsPadHold(const FKey& Button);
	/** Button: A, B, X, Y, the D-pad, LB, LT, L3, R3 or View (not RB / RT alone, Menu, which pauses, or the sticks).
	 * Hold: none, RB or RT. An unbound binding is allowed too. */
	SHINOBICOOP_API bool IsPadAllowed(const FPadBinding& Binding);
	/** Which gets New. If another binding already had New, it takes Which's old one (OutSwappedWith says which). Saves and applies. */
	SHINOBICOOP_API ERebind RebindPad(EBindable Which, const FPadBinding& New, EBindable* OutSwappedWith = nullptr);
	/** One button's name: "A", "D-pad Up", "LT" (bShort: an arrow for the D-pad). */
	SHINOBICOOP_API FText PadButtonLabel(const FKey& Button, bool bShort = false);
	/** "RB + A", "Y", "None"; bShort for the HUD's key chips: "RB+A", "Y", and empty for none. */
	SHINOBICOOP_API FText PadLabel(const FPadBinding& Binding, bool bShort = false);

	// ---- Resets ----

	SHINOBICOOP_API void ResetKeyboard();
	SHINOBICOOP_API void ResetPad();
	/** Both devices. */
	SHINOBICOOP_API void ResetToDefaults();
}
