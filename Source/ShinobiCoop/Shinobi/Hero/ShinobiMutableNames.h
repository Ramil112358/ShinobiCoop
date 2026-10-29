#pragma once

#include "CoreMinimal.h"

/**
 * Names shared by CO_Shinobi (built by Scripts/setup_mutable.py, which mirrors them in Scripts/mutable/names.py) and the
 * code that sets its parameters. Change both together.
 */
namespace ShinobiMutable
{
	inline constexpr const TCHAR* ObjectPath = TEXT("/Game/Shinobi/Mutable/CO_Shinobi.CO_Shinobi");
	inline constexpr const TCHAR* PalettePath = TEXT("/Game/Shinobi/Mutable/DT_Palette.DT_Palette");

	inline constexpr const TCHAR* Sex = TEXT("Sex");
	inline constexpr const TCHAR* Build = TEXT("Build");
	inline constexpr const TCHAR* BuildBlend = TEXT("Build Blend");
	inline constexpr const TCHAR* SkinTone = TEXT("Skin Tone");
	inline constexpr const TCHAR* Aged = TEXT("Aged");
	inline constexpr const TCHAR* MakeUp = TEXT("Make Up");
	inline constexpr const TCHAR* Freckles = TEXT("Freckles");

	/** Wardrobe slot parameters, in EShinobiWardrobeSlot order; "<slot> Colour" and "<slot> Wear" are its colour and wear. */
	inline constexpr const TCHAR* SlotNames[] = {TEXT("Top"), TEXT("Bottom"), TEXT("Shoes"), TEXT("Outer"), TEXT("Accessory"), TEXT("Cloak")};
	inline constexpr const TCHAR* ColourSuffix = TEXT(" Colour");
	inline constexpr const TCHAR* WearSuffix = TEXT(" Wear");

	inline constexpr int32 MaxTattoos = 3;
}
