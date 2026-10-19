#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiInputConfig.h"

namespace ShinobiKeyBindings
{
	namespace
	{
		const TCHAR* ConfigSection = TEXT("/Script/ShinobiCoop.ShinobiKeyBindings");
		const TCHAR* AltPrefix = TEXT("Alt+");

		const TCHAR* ConfigKey(EBindable Which)
		{
			static const TCHAR* Names[] = { TEXT("Slot1"), TEXT("Slot2"), TEXT("Slot3"), TEXT("Slot4"), TEXT("Slot5"), TEXT("Slot6"), TEXT("Slot7"), TEXT("Slot8"),
				TEXT("SkillsScreen"), TEXT("Melee"), TEXT("Dodge"), TEXT("Sprint"), TEXT("Block"), TEXT("ChakraCharge"), TEXT("Revive"), TEXT("LockOn"),
				TEXT("Jump"), TEXT("Dismount"), TEXT("Map") };
			static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EBindable::Num), "one config key per bindable");
			return Names[static_cast<int32>(Which)];
		}

		void Store(EBindable Which, const FBinding& Binding)
		{
			const FString Value = (Binding.bAlt ? FString(AltPrefix) : FString()) + Binding.Key.GetFName().ToString();
			GConfig->SetString(ConfigSection, ConfigKey(Which), *Value, GGameUserSettingsIni);
		}

		void SaveAndApply()
		{
			GConfig->Flush(false, GGameUserSettingsIni);
			UShinobiInputConfig::Get().ApplyKeyBindings();
		}
	}

	FBinding Default(EBindable Which)
	{
		static const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::R, EKeys::T, EKeys::G, EKeys::CapsLock,
			EKeys::K, EKeys::LeftMouseButton, EKeys::LeftControl, EKeys::LeftShift, EKeys::F, EKeys::C, EKeys::E, EKeys::Tab,
			EKeys::SpaceBar, EKeys::X, EKeys::M };
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
		static const FKey Fixed[] = { EKeys::Escape, EKeys::LeftCommand, EKeys::RightCommand, EKeys::W, EKeys::A, EKeys::S, EKeys::D,
			EKeys::RightMouseButton, EKeys::LeftAlt, EKeys::RightAlt, EKeys::MouseScrollUp, EKeys::MouseScrollDown };
		for (const FKey& Each : Fixed)
		{
			if (Key == Each)
			{
				return true;
			}
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

	void ResetToDefaults()
	{
		for (int32 Index = 0; Index < static_cast<int32>(EBindable::Num); ++Index)
		{
			GConfig->RemoveKey(ConfigSection, ConfigKey(static_cast<EBindable>(Index)), GGameUserSettingsIni);
		}
		SaveAndApply();
	}

	FText Label(EBindable Which)
	{
		static const TCHAR* Labels[] = { TEXT("Slot 1"), TEXT("Slot 2"), TEXT("Slot 3"), TEXT("Slot 4"), TEXT("Slot 5"), TEXT("Slot 6"), TEXT("Slot 7"), TEXT("Slot 8"),
			TEXT("Skills screen"), TEXT("Melee"), TEXT("Dodge"), TEXT("Sprint"), TEXT("Block"), TEXT("Chakra charge"), TEXT("Revive"), TEXT("Lock-on"),
			TEXT("Jump"), TEXT("Dismount dragon"), TEXT("Map") };
		static_assert(UE_ARRAY_COUNT(Labels) == static_cast<int32>(EBindable::Num), "one label per bindable");
		return FText::FromString(Labels[static_cast<int32>(Which)]);
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
}
