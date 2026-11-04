#pragma once

#include "CoreMinimal.h"

class UWorld;
enum class EShinobiSnow : uint8;

/**
 * The player's character and outfit picks and graphics settings (settings menu): each row cycles through fixed values, applies at once and is saved
 * per machine (UGameUserSettings, GameUserSettings.ini). A client's settings never affect the host.
 */
namespace ShinobiSettings
{
	/**
	 * Every row is always listed. A row (or one of its options) the machine can't use is locked, with the reason (RowLock,
	 * OptionLock), like the graphics menus of current PC games: DLSS, frame generation, Reflex and ray tracing depend on the
	 * GPU and NVIDIA's plugins (ShinobiGraphics). As in Battlefield 6 / The Witcher 3 next-gen, the upscaler (Off / TSR / NVIDIA
	 * DLSS) and its quality (RenderScale: Quality ... Ultra Performance) are apart from native anti-aliasing (TSR / NVIDIA DLAA),
	 * which applies while upscaling is off.
	 */
	enum class ERow : uint8
	{
		Character, Face, Outfit,
		Quality, LightingQuality,
		Upscaler, RenderScale, AntiAliasing, Sharpness, DynamicResolution, FrameGeneration, LowLatency,
		RayTracing, RayReconstruction, MegaLights,
		WindowMode, Resolution, FrameCap, VSync,
		Outline, Snow,
		Num
	};

	/** "Graphics quality: High" etc. (RowName: RowValue). */
	SHINOBICOOP_API FString Label(ERow Row);
	/** "Graphics quality" etc. */
	SHINOBICOOP_API FString RowName(ERow Row);
	/** "High" etc.: the row's current value. */
	SHINOBICOOP_API FString RowValue(ERow Row);
	/** One or two sentences on what the row does, for the settings panel's help line. */
	SHINOBICOOP_API FString RowDescription(ERow Row);
	/** The heading the row is listed under: CHARACTER, GRAPHICS, UPSCALING & FRAME GENERATION, RAY TRACING, DISPLAY or EFFECTS. */
	SHINOBICOOP_API FString RowSection(ERow Row);
	/** Moves Row Delta values on (negative: back), wrapping and skipping locked options, then applies and saves (a locked row:
	 * nothing). World is used by the outline toggle and the in-game character switch. */
	SHINOBICOOP_API void Step(ERow Row, UWorld* World, int32 Delta);
	/** Every value Row can take, in order ("Low" ... "Cinematic", "Off" / "On"). */
	SHINOBICOOP_API TArray<FString> RowOptions(ERow Row);
	/** Index into RowOptions of Row's current value; INDEX_NONE when it's none of them (Custom quality). */
	SHINOBICOOP_API int32 RowIndex(ERow Row);
	/** Sets Row to RowOptions(Row)[Index] (out of range, a locked row or option: nothing), then applies and saves. */
	SHINOBICOOP_API void SetRowIndex(ERow Row, UWorld* World, int32 Index);
	/** More than two options, or anti-aliasing: the settings panel picks from a dropdown list (locked options greyed out in it);
	 * On / Off rows step. */
	SHINOBICOOP_API bool UsesDropdown(ERow Row);
	/** Step(Row, World, +1). */
	SHINOBICOOP_API void Cycle(ERow Row, UWorld* World);

	/** The roster character this machine's player plays (saved by the Character row; the hero by default). */
	SHINOBICOOP_API FString GetSavedCharacter();
	/** The outfit this machine's player wears (saved by the Outfit row; None by default). */
	SHINOBICOOP_API FString GetSavedOutfit();
	/** The face this machine's player wears on a character with several (saved by the Face row; empty = each one's own). */
	SHINOBICOOP_API FString GetSavedFace();
	SHINOBICOOP_API void SetSavedFace(const FString& Name);
	/** Saves the pick (the main menu's SHINOBI tab); a name not in the roster / outfit list is ignored. A roster pick also
	 * stops playing a made character (UShinobiCharacterSave::Active). */
	SHINOBICOOP_API void SetSavedCharacter(const FString& Name);
	SHINOBICOOP_API void SetSavedOutfit(const FString& Name);

	/** Why Row can't be changed on this machine right now, for the player ("" when it can): no RTX card, no plugin, VSync while
	 * frame generation is on... A locked row is shown greyed out and doesn't step. */
	SHINOBICOOP_API FString RowLock(ERow Row);
	/** Why RowOptions(Row)[Option] can't be picked ("" when it can): DLSS without an RTX card, 4X frame generation on an RTX 40... */
	SHINOBICOOP_API FString OptionLock(ERow Row, int32 Option);
	/** OptionLock for each of RowOptions(Row). */
	SHINOBICOOP_API TArray<FString> OptionLocks(ERow Row);
	/** The line under a row's description: why it, or some of its options, are locked ("" when nothing is). */
	SHINOBICOOP_API FString RowNote(ERow Row);
	/** The rows the settings panel shows: all of them, but the main menu leaves out Character, Face and Outfit (its SHINOBI tab
	 * has them). */
	SHINOBICOOP_API TArray<ERow> RowsFor(bool bMainMenu);

	/** Snow weather this machine picks when it hosts (Off by default). In a session the host's pick applies to everyone. */
	SHINOBICOOP_API EShinobiSnow GetSavedSnow();

	/** The Graphics quality preset (0 Low ... 4 Cinematic) every quality group is at, or INDEX_NONE (Custom) when they differ.
	 * The render resolution doesn't count: Upscaling method / quality own it (UGameUserSettings::GetOverallScalabilityLevel
	 * also wants the preset's own resolution, so it would read Custom whenever upscaling picks another one). */
	SHINOBICOOP_API int32 GetQualityPreset();

	SHINOBICOOP_API float GetRenderScale();
	SHINOBICOOP_API void SetRenderScale(float Percent);
}
