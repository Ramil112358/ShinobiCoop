#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

/**
 * The realistic look's character outline (sub-project #3b): characters write a stencil ID into custom depth and
 * M_OutlinePost draws a thin edge around each stencil region (dark for friendly, a faint red rim for enemies).
 * Needs r.CustomDepth=3 (depth + stencil) in DefaultEngine.ini.
 */
namespace ShinobiLook
{
	constexpr uint8 StencilFriendly = 1; // players and villagers
	constexpr uint8 StencilEnemy = 2;

	/** Mesh gets Stencil and writes custom depth while the outline setting is on (nullptr is ignored). */
	SHINOBICOOP_API void EnableOutline(UPrimitiveComponent* Mesh, uint8 Stencil);

	/** The player's outline setting (saved in GameUserSettings.ini; on by default). */
	SHINOBICOOP_API bool IsOutlineEnabled();
	/** Saves the setting and applies it to every outlined character in World. */
	SHINOBICOOP_API void SetOutlineEnabled(UWorld* World, bool bEnabled);
}
