#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UObject;

/** Shared look for all glowing effects: every one is a dynamic instance of M_ChakraBodyAura (built by Scripts/setup_vfx.py). */
namespace ShinobiVfx
{
	namespace Colors
	{
		const FLinearColor Chakra(0.1f, 0.45f, 1.f);
		const FLinearColor Fire(1.f, 0.22f, 0.02f);
		const FLinearColor BlackFlame(0.35f, 0.02f, 0.6f);
		const FLinearColor Lightning(0.55f, 0.8f, 1.f);
		const FLinearColor Smoke(0.6f, 0.6f, 0.6f);
	}

	/**
	 * A dynamic instance of M_ChakraBodyAura. Core 0 glows only at the silhouette (body auras); Core 1 glows
	 * all over (orbs and bursts). Inflate pushes the surface out along its normals (0 for plain meshes).
	 * Returns null, with an ensure, if the material asset is missing.
	 */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeGlow(UObject* Outer, const FLinearColor& Color, float Intensity, float Core, float Inflate = 0.f);
}
