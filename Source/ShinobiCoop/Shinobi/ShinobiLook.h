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

	/** Mesh writes custom depth with Stencil (nullptr is ignored). */
	SHINOBICOOP_API void EnableOutline(UPrimitiveComponent* Mesh, uint8 Stencil);
}
