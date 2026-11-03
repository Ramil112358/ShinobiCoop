#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiInputConfig.h"

namespace ShinobiKeyBindings
{
	namespace
	{
		const TCHAR* ConfigSection = TEXT("/Script/ShinobiCoop.ShinobiKeyBindings");
		const TCHAR* AltPrefix = TEXT("Alt+");
		/** A controller binding saved as nothing (so the default doesn't come back). */
		const TCHAR* PadNone = TEXT("None");

		const TCHAR* ConfigKey(EBindable Which)
		{
			static const TCHAR* Names[] = { TEXT("Slot1"), TEXT("Slot2"), TEXT("Slot3"), TEXT("Slot4"), TEXT("Slot5"), TEXT("Slot6"), TEXT("Slot7"), TEXT("Slot8"),
				TEXT("SkillsScreen"), TEXT("Melee"), TEXT("Dodge"), TEXT("Sprint"), TEXT("Block"), TEXT("ChakraCharge"), TEXT("Revive"), TEXT("LockOn"),
				TEXT("Jump"), TEXT("Dismount"), TEXT("Map"), TEXT("MapZoomIn"), TEXT("MapZoomOut") };
			static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EBindable::Num), "one config key per bindable");
			return Names[static_cast<int32>(Which)];
		}

		/** "GamepadSlot1", "GamepadMelee"... */
		FString PadConfigKey(EBindable Which)
		{
			return FString(TEXT("Gamepad")) + ConfigKey(Which);
		}

		void Store(EBindable Which, const FBinding& Binding)
		{
			const FString Value = (Binding.bAlt ? FString(AltPrefix) : FString()) + Binding.Key.GetFName().ToString();
			GConfig->SetString(ConfigSection, ConfigKey(Which), *Value, GGameUserSettingsIni);
		}

		/** "Gamepad_RightShoulder+Gamepad_FaceButton_Bottom", "Gamepad_FaceButton_Top" or "None". */
		void StorePad(EBindable Which, const FPadBinding& Binding)
		{
			FString Value = PadNone;
			if (Binding.IsBound())
			{
				Value = Binding.Hold.IsValid() ? Binding.Hold.GetFName().ToString() + TEXT("+") + Binding.Button.GetFName().ToString()
					: Binding.Button.GetFName().ToString();
			}
			GConfig->SetString(ConfigSection, *PadConfigKey(Which), *Value, GGameUserSettingsIni);
		}

		void SaveAndApply()
		{
			GConfig->Flush(false, GGameUserSettingsIni);
			UShinobiInputConfig::Get().ApplyKeyBindings();
		}

		bool IsOneOf(const FKey& Key, std::initializer_list<FKey> Keys)
		{
			for (const FKey& Each : Keys)
			{
				if (Key == Each)
				{
					return true;
				}
			}
			return false;
		}
	}

	FText Label(EBindable Which)
	{
		static const TCHAR* Labels[] = { TEXT("Slot 1"), TEXT("Slot 2"), TEXT("Slot 3"), TEXT("Slot 4"), TEXT("Slot 5"), TEXT("Slot 6"), TEXT("Slot 7"), TEXT("Slot 8"),
			TEXT("Jutsu loadout"), TEXT("Melee"), TEXT("Dodge"), TEXT("Sprint"), TEXT("Block"), TEXT("Chakra charge"), TEXT("Revive"), TEXT("Lock-on"),
			TEXT("Jump"), TEXT("Dismount dragon"), TEXT("Map"), TEXT("Map zoom in"), TEXT("Map zoom out") };
		static_assert(UE_ARRAY_COUNT(Labels) == static_cast<int32>(EBindable::Num), "one label per bindable");
		return FText::FromString(Labels[static_cast<int32>(Which)]);
	}

	// ---- Keyboard and mouse ----

	FBinding Default(EBindable Which)
	{
		static const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::R, EKeys::T, EKeys::G, EKeys::CapsLock,
			EKeys::K, EKeys::LeftMouseButton, EKeys::LeftControl, EKeys::LeftShift, EKeys::F, EKeys::C, EKeys::E, EKeys::Tab,
			EKeys::SpaceBar, EKeys::X, EKeys::M, EKeys::Equals, EKeys::Hyphen };
		static_assert(UE_ARRAY_COUNT(Keys) == static_cast<int32>(EBindable::Num), "one default per bindable");
		return { Keys[static_cast<int32>(Which)], false };
	}

	FBinding Get(EBindable Which)
	{
		FString Value;
		if (!GConfig->GetString(ConfigSection, ConfigKey(Which), Value, GGameUserSettingsIni) || Value.IsEmpty())
		{
			return Default(Which);
		}
		FBinding Binding;
		Binding.bAlt = Value.RemoveFromStart(AltPrefix);
		Binding.Key = FKey(FName(*Value));
		return Binding.Key.IsValid() && !IsBlocked(Binding) ? Binding : Default(Which);
	}

	bool IsBlocked(const FBinding& Binding)
	{
		const FKey& Key = Binding.Key;
		if (!Key.IsValid() || Key.IsGamepadKey() || Key.IsTouch() || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D())
		{
			return true;
		}
		if (IsOneOf(Key, { EKeys::Escape, EKeys::LeftCommand, EKeys::RightCommand, EKeys::W, EKeys::A, EKeys::S, EKeys::D,
			EKeys::RightMouseButton, EKeys::LeftAlt, EKeys::RightAlt, EKeys::MouseScrollUp, EKeys::MouseScrollDown }))
		{
			return true;
		}
		return Binding.bAlt && (Key == EKeys::F4 || Key == EKeys::Tab); // Windows owns these
	}

	ERebind Rebind(EBindable Which, const FBinding& New, EBindable* OutSwappedWith)
	{
		if (IsBlocked(New))
		{
			return ERebind::Blocked;
		}
		const FBinding Old = Get(Which);
		ERebind Result = ERebind::Done;
		for (int32 Index = 0; Index < static_cast<int32>(EBindable::Num); ++Index)
		{
			const EBindable Other = static_cast<EBindable>(Index);
			if (Other != Which && Get(Other) == New)
			{
				Store(Other, Old); // nothing is ever left without a key
				Result = ERebind::Swapped;
				if (OutSwappedWith)
				{
					*OutSwappedWith = Other;
				}
				break;
			}
		}
		Store(Which, New);
		SaveAndApply();
		return Result;
	}

	FText KeyLabel(const FBinding& Binding, bool bShort)
	{
		struct FKeyName { FKey Key; const TCHAR* Long; const TCHAR* Short; };
		static const FKeyName Names[] = {
			{ EKeys::CapsLock, TEXT("Caps Lock"), TEXT("Caps") },
			{ EKeys::LeftMouseButton, TEXT("Left Mouse"), TEXT("LMB") },
			{ EKeys::MiddleMouseButton, TEXT("Middle Mouse"), TEXT("MMB") },
			{ EKeys::ThumbMouseButton, TEXT("Mouse 4"), TEXT("M4") },
			{ EKeys::ThumbMouseButton2, TEXT("Mouse 5"), TEXT("M5") },
			{ EKeys::LeftControl, TEXT("Left Ctrl"), TEXT("Ctrl") },
			{ EKeys::LeftShift, TEXT("Left Shift"), TEXT("Shift") },
			{ EKeys::SpaceBar, TEXT("Space"), TEXT("Space") },
		};
		FString Text = Binding.Key.GetDisplayName(false).ToString();
		for (const FKeyName& Each : Names)
		{
			if (Each.Key == Binding.Key)
			{
				Text = bShort ? Each.Short : Each.Long;
				break;
			}
		}
		return FText::FromString(Binding.bAlt ? AltPrefix + Text : Text);
	}

	// ---- Controller ----

	FPadBinding PadDefault(EBindable Which)
	{
		const FKey RB = EKeys::Gamepad_RightShoulder;
		const FKey None;
		// Slots 1-8 on RB + A / B / X / Y / D-pad up / right / down / left; the actions where they were before rebinding.
		static const FPadBinding Defaults[] = {
			{ RB, EKeys::Gamepad_FaceButton_Bottom }, { RB, EKeys::Gamepad_FaceButton_Right }, { RB, EKeys::Gamepad_FaceButton_Left }, { RB, EKeys::Gamepad_FaceButton_Top },
			{ RB, EKeys::Gamepad_DPad_Up }, { RB, EKeys::Gamepad_DPad_Right }, { RB, EKeys::Gamepad_DPad_Down }, { RB, EKeys::Gamepad_DPad_Left },
			{ None, EKeys::Gamepad_DPad_Left }, // Skills screen
			{ None, EKeys::Gamepad_FaceButton_Left }, // Melee
			{ None, EKeys::Gamepad_FaceButton_Right }, // Dodge
			{ None, EKeys::Gamepad_LeftThumbstick }, // Sprint
			{ None, EKeys::Gamepad_LeftTrigger }, // Block (hold)
			{ None, EKeys::Gamepad_LeftShoulder }, // Chakra charge
			{ None, EKeys::Gamepad_FaceButton_Top }, // Revive
			{ None, EKeys::Gamepad_RightThumbstick }, // Lock-on
			{ None, EKeys::Gamepad_FaceButton_Bottom }, // Jump
			{ None, EKeys::Gamepad_DPad_Down }, // Dismount dragon
			{ None, EKeys::Gamepad_Special_Left }, // Map (View)
			{ None, EKeys::Gamepad_DPad_Up }, // Map zoom in (past the closest level it wraps back to the whole map)
			{ None, None }, // Map zoom out
		};
		static_assert(UE_ARRAY_COUNT(Defaults) == static_cast<int32>(EBindable::Num), "one controller default per bindable");
		return Defaults[static_cast<int32>(Which)];
	}

	FPadBinding GetPad(EBindable Which)
	{
		FString Value;
		if (!GConfig->GetString(ConfigSection, *PadConfigKey(Which), Value, GGameUserSettingsIni) || Value.IsEmpty())
		{
			return PadDefault(Which);
		}
		if (Value == PadNone)
		{
			return FPadBinding();
		}
		FPadBinding Binding;
		FString HoldName, ButtonName;
		if (Value.Split(TEXT("+"), &HoldName, &ButtonName))
		{
			Binding.Hold = FKey(FName(*HoldName));
			Binding.Button = FKey(FName(*ButtonName));
		}
		else
		{
			Binding.Button = FKey(FName(*Value));
		}
		return Binding.IsBound() && IsPadAllowed(Binding) ? Binding : PadDefault(Which);
	}

	bool IsPadHold(const FKey& Button)
	{
		return Button == EKeys::Gamepad_RightShoulder || Button == EKeys::Gamepad_RightTrigger;
	}

	bool IsPadAllowed(const FPadBinding& Binding)
	{
		if (!Binding.IsBound())
		{
			return !Binding.Hold.IsValid();
		}
		const bool bButton = IsOneOf(Binding.Button, { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left,
			EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left,
			EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_LeftThumbstick, EKeys::Gamepad_RightThumbstick,
			EKeys::Gamepad_Special_Left });
		return bButton && (!Binding.Hold.IsValid() || IsPadHold(Binding.Hold));
	}

	ERebind RebindPad(EBindable Which, const FPadBinding& New, EBindable* OutSwappedWith)
	{
		if (!IsPadAllowed(New))
		{
			return ERebind::Blocked;
		}
		const FPadBinding Old = GetPad(Which);
		ERebind Result = ERebind::Done;
		for (int32 Index = 0; New.IsBound() && Index < static_cast<int32>(EBindable::Num); ++Index)
		{
			const EBindable Other = static_cast<EBindable>(Index);
			if (Other != Which && GetPad(Other) == New)
			{
				StorePad(Other, Old); // (Old may be nothing: then Other has nothing on the controller)
				Result = ERebind::Swapped;
				if (OutSwappedWith)
				{
					*OutSwappedWith = Other;
				}
				break;
			}
		}
		StorePad(Which, New);
		SaveAndApply();
		return Result;
	}

	FText PadButtonLabel(const FKey& Button, bool bShort)
	{
		struct FPadName { FKey Key; const TCHAR* Long; const TCHAR* Short; };
		static const FPadName Names[] = {
			{ EKeys::Gamepad_FaceButton_Bottom, TEXT("A"), TEXT("A") },
			{ EKeys::Gamepad_FaceButton_Right, TEXT("B"), TEXT("B") },
			{ EKeys::Gamepad_FaceButton_Left, TEXT("X"), TEXT("X") },
			{ EKeys::Gamepad_FaceButton_Top, TEXT("Y"), TEXT("Y") },
			{ EKeys::Gamepad_DPad_Up, TEXT("D-pad Up"), TEXT("↑") },
			{ EKeys::Gamepad_DPad_Right, TEXT("D-pad Right"), TEXT("→") },
			{ EKeys::Gamepad_DPad_Down, TEXT("D-pad Down"), TEXT("↓") },
			{ EKeys::Gamepad_DPad_Left, TEXT("D-pad Left"), TEXT("←") },
			{ EKeys::Gamepad_LeftShoulder, TEXT("LB"), TEXT("LB") },
			{ EKeys::Gamepad_RightShoulder, TEXT("RB"), TEXT("RB") },
			{ EKeys::Gamepad_LeftTrigger, TEXT("LT"), TEXT("LT") },
			{ EKeys::Gamepad_RightTrigger, TEXT("RT"), TEXT("RT") },
			{ EKeys::Gamepad_LeftThumbstick, TEXT("L3"), TEXT("L3") },
			{ EKeys::Gamepad_RightThumbstick, TEXT("R3"), TEXT("R3") },
			{ EKeys::Gamepad_Special_Left, TEXT("View"), TEXT("View") },
			{ EKeys::Gamepad_Special_Right, TEXT("Menu"), TEXT("Menu") },
		};
		for (const FPadName& Each : Names)
		{
			if (Each.Key == Button)
			{
				return FText::FromString(bShort ? Each.Short : Each.Long);
			}
		}
		return Button.GetDisplayName(false);
	}

	FText PadLabel(const FPadBinding& Binding, bool bShort)
	{
		if (!Binding.IsBound())
		{
			return bShort ? FText::GetEmpty() : INVTEXT("None");
		}
		const FString Button = PadButtonLabel(Binding.Button, bShort).ToString();
		if (!Binding.Hold.IsValid())
		{
			return FText::FromString(Button);
		}
		return FText::FromString(PadButtonLabel(Binding.Hold, bShort).ToString() + (bShort ? TEXT("+") : TEXT(" + ")) + Button);
	}

	// ---- Resets ----

	namespace
	{
		void RemoveKeys(bool bKeyboard, bool bPad)
		{
			for (int32 Index = 0; Index < static_cast<int32>(EBindable::Num); ++Index)
			{
				const EBindable Which = static_cast<EBindable>(Index);
				if (bKeyboard)
				{
					GConfig->RemoveKey(ConfigSection, ConfigKey(Which), GGameUserSettingsIni);
				}
				if (bPad)
				{
					GConfig->RemoveKey(ConfigSection, *PadConfigKey(Which), GGameUserSettingsIni);
				}
			}
			SaveAndApply();
		}
	}

	void ResetKeyboard()
	{
		RemoveKeys(true, false);
	}

	void ResetPad()
	{
		RemoveKeys(false, true);
	}

	void ResetToDefaults()
	{
		RemoveKeys(true, true);
	}
}
