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

	/** The body: a Skeletal Mesh Parameter given the sex's base (SK_MutableBase_<Sex>), so the sex needs no parameter. */
	inline constexpr const TCHAR* Body = TEXT("Body");
	inline constexpr const TCHAR* BasePathFormat = TEXT("/Game/Shinobi/Mutable/Bases/SK_MutableBase_%s.SK_MutableBase_%s");
	/** Build enum (EShinobiBuild names; Normal has no morph) and the morph's weight 0..1. */
	inline constexpr const TCHAR* Build = TEXT("Build");
	inline constexpr const TCHAR* BuildBlend = TEXT("Build Blend");
	/** Skin (phase 3: texture layers on the MetaHuman body). */
	inline constexpr const TCHAR* SkinTone = TEXT("Skin Tone");
	inline constexpr const TCHAR* Aged = TEXT("Aged");
	inline constexpr const TCHAR* MakeUp = TEXT("Make Up");
	inline constexpr const TCHAR* Freckles = TEXT("Freckles");

	/**
	 * Wardrobe slots, in EShinobiWardrobeSlot order. Each is dataless (meshes are given at runtime, so new pieces need no
	 * recompile): enum "<slot>" ON/OFF wears Skeletal Mesh Parameter "<slot> Mesh" (reshaped to the build), and enum
	 * "<slot> Hides" ON/OFF clips the body with Skeletal Mesh Parameter "<slot> Clip".
	 */
	inline constexpr const TCHAR* SlotNames[] = {TEXT("Top"), TEXT("Bottom"), TEXT("Shoes"), TEXT("Outer"), TEXT("Accessory"), TEXT("Cloak")};
	inline constexpr const TCHAR* MeshSuffix = TEXT(" Mesh");
	inline constexpr const TCHAR* ClipSuffix = TEXT(" Clip");
	inline constexpr const TCHAR* HidesSuffix = TEXT(" Hides");
	inline constexpr const TCHAR* On = TEXT("ON");
	inline constexpr const TCHAR* Off = TEXT("OFF");

	/** Material parameters the wearer sets on a piece's materials (not Mutable parameters: runtime meshes keep theirs). */
	inline constexpr const TCHAR* ColourParameter = TEXT("Colour");
	inline constexpr const TCHAR* WearParameter = TEXT("Wear");

	/** The slot's wardrobe Data Table (rows: FShinobiWardrobeRow). */
	inline FString WardrobeTablePath(const TCHAR* Slot) { return FString::Printf(TEXT("/Game/Shinobi/Mutable/DT_Wardrobe_%s.DT_Wardrobe_%s"), Slot, Slot); }

	inline constexpr int32 MaxTattoos = 3;
}
