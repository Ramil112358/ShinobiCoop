#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * The player's character and outfit picks and graphics settings (settings menu): each row cycles through fixed values, applies at once and is saved
 * per machine (UGameUserSettings, GameUserSettings.ini). A client's settings never affect the host.
 */
namespace ShinobiSettings
{
	enum class ERow : uint8 { Character, Outfit, Quality, RenderScale, WindowMode, FrameCap, VSync, Outline, Num };

	/** "Graphics quality: High" etc. */
	SHINOBICOOP_API FString Label(ERow Row);
	/** Advances Row to its next value, applies and saves. World is used by the outline toggle. */
	SHINOBICOOP_API void Cycle(ERow Row, UWorld* World);

	/** The roster character this machine's player plays (saved by the Character row; the hero by default). */
	SHINOBICOOP_API FString GetSavedCharacter();
	/** The outfit this machine's player wears (saved by the Outfit row; None by default). */
	SHINOBICOOP_API FString GetSavedOutfit();
	/** Saves the pick (the main menu's SHINOBI tab); a name not in the roster / outfit list is ignored. A roster pick also
	 * stops playing a made character (UShinobiCharacterSave::Active). */
	SHINOBICOOP_API void SetSavedCharacter(const FString& Name);
	SHINOBICOOP_API void SetSavedOutfit(const FString& Name);

	/** The rows the settings panel shows: the main menu leaves out Character and Outfit (its SHINOBI tab has them). */
	SHINOBICOOP_API TArray<ERow> RowsFor(bool bMainMenu);

	SHINOBICOOP_API float GetRenderScale();
	SHINOBICOOP_API void SetRenderScale(float Percent);
}
