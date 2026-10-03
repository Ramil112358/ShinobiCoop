#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UObject;
class UWorld;

/** Shared look for all glowing effects: every one is a dynamic instance of M_ChakraBodyAura (built by Scripts/setup_vfx.py). */
namespace ShinobiVfx
{
	namespace Colors
	{
		const FLinearColor Chakra(0.1f, 0.45f, 1.f);
		const FLinearColor Fire(1.f, 0.22f, 0.02f);
		const FLinearColor BlackFlame(0.35f, 0.02f, 0.6f);
		const FLinearColor Lightning(0.05f, 0.3f, 1.f); // Chidori blue (keep R and G under 1 at glow intensities, or it reads white)
		const FLinearColor Smoke(0.6f, 0.6f, 0.6f);
		const FLinearColor Ice(0.55f, 0.85f, 1.f);
		const FLinearColor BlackFlameCore(0.01f, 0.f, 0.015f);
		const FLinearColor Blood(0.35f, 0.f, 0.f);
	}

	/**
	 * A dynamic instance of M_ChakraBodyAura. Core 0 glows only at the silhouette (body auras); Core 1 glows
	 * all over (orbs and bursts). Inflate pushes the surface out along its normals (0 for plain meshes).
	 * Returns null, with an ensure, if the material asset is missing.
	 */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeGlow(UObject* Outer, const FLinearColor& Color, float Intensity, float Core, float Inflate = 0.f);

	/**
	 * Flickering flame tongues: M_Flame (additive: fire, lightning) or, with bDark, M_FlameDark (translucent: a
	 * near-black core with a Color rim, for Amaterasu). Erode (0-1) thins the tongues; Fade 0-1 fades it out.
	 * Returns null, with an ensure, if the material asset is missing.
	 */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeFlame(UObject* Outer, const FLinearColor& Color, float Intensity, bool bDark = false);

	/** A cell of one of the SpriteCook sheets (Scripts/setup_sprite_vfx.py). Flame, BlackFlame, Smoke and Ember are 4 x 2 atlases of 8 / 8 / 4 / 4 variants. */
	enum class ESprite : uint8
	{
		Flame,      // realistic fire tongue, additive (8)
		BlackFlame, // Amaterasu's flame, translucent (8)
		Smoke,      // dark smoke plume, translucent (4)
		Ember,      // spray of sparks, additive (4)
		Lightning,  // branching bolt, additive (4)
		Ring,       // shockwave ring, additive
		Vortex,     // spiral energy, additive
		Slash,      // curved streak, additive
		Burst,      // radial light rays, additive
	};

	/** How many variants the sprite has (the Variant argument below wraps around this). */
	SHINOBICOOP_API int32 SpriteVariants(ESprite Kind);

	/**
	 * A quad material showing one variant of the sprite. Tint multiplies the sheet's own colours (white keeps them;
	 * for BlackFlame it is the glow colour, for Smoke the smoke colour). Intensity scales the glow. Fade 0-1 is a parameter
	 * of the returned material. Null, with an ensure, if an asset is missing.
	 */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeSprite(UObject* Outer, ESprite Kind, int32 Variant, const FLinearColor& Tint, float Intensity);

	/** Switch a MakeSprite material to another variant of the same sprite (flipbook-style flicker). */
	SHINOBICOOP_API void SetSpriteVariant(UMaterialInstanceDynamic* Material, ESprite Kind, int32 Variant);

	/**
	 * A charred mark lying on the ground (the SpriteCook scorch texture), radius in world units: it fades in, stays for
	 * Linger seconds, fades out, and removes itself. Local and cosmetic (not replicated), so call it on every machine.
	 * Does nothing without a world or on a dedicated server.
	 */
	SHINOBICOOP_API void SpawnScorch(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Tint, float Linger);

	/** The Water Wall's flowing-water material (M_WaterWall, built by Scripts/setup_vfx.py) tinted Color: the Water Dragon's body.
	 * Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeWater(UObject* Outer, const FLinearColor& Color, float Intensity);

	/** Susanoo's glowing spectral armour (M_Susanoo, built by Scripts/setup_vfx.py) at Fade 0-1. Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeSusanoo(UObject* Outer, float Fade);

	/** Kamui's see-through chakra-blue ghost (M_Phased, built by Scripts/setup_vfx.py) at Opacity 0-1. Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakePhased(UObject* Outer, float Opacity);

	/** Flat unlit colour (M_Streak): the blood under Amaterasu's eyes. Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeStreak(UObject* Outer, const FLinearColor& Color);
}
