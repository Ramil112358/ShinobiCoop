#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * The player's graphics settings (settings menu): each row cycles through fixed values, applies at once and is saved
 * per machine (UGameUserSettings, GameUserSettings.ini). A client's settings never affect the host.
 */
namespace ShinobiSettings
{
	enum class ERow : uint8 { Quality, RenderScale, WindowMode, FrameCap, VSync, Outline, Num };

	/** "Graphics quality: High" etc. */
	SHINOBICOOP_API FString Label(ERow Row);
	/** Advances Row to its next value, applies and saves. World is used by the outline toggle. */
	SHINOBICOOP_API void Cycle(ERow Row, UWorld* World);

	SHINOBICOOP_API float GetRenderScale();
	SHINOBICOOP_API void SetRenderScale(float Percent);
}
