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
		TairnFire,  // tall jets of blue-white lightning fire, additive (8): Tairn's breath
		CrimsonFire, // tall jets of orange dragon fire with embers, additive (8): the crimson dragon's breath
		ChakraWisp, // soft rising wisp of blue chakra, additive (4): the chakra charge
		ChakraSwirl, // curved blue chakra stream, head on the left, additive (4): the chakra charge
		ChidoriOrb,  // crackling ball of lightning centred in a tall cell, additive (8): Chidori in the hand
		RasenganOrb, // swirling chakra sphere centred in a tall cell, additive (8): Rasengan in the hand
		Dust,        // billowing pale dust cloud standing on the cell's bottom, translucent and shaded (8): Almighty Push
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
	 * Does nothing without a world or on a dedicated server. Mark swaps the charred texture for another (ChidoriDent,
	 * RasenganCrater); the material is unlit, so a mark lighter than the char needs a dark Tint or it glows.
	 */
	SHINOBICOOP_API void SpawnScorch(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Tint, float Linger, const TCHAR* Mark = nullptr);

	/** Marks for SpawnScorch: the cracked dent Chidori leaves, the spiral crater Rasengan leaves. */
	inline const TCHAR* ChidoriDent = TEXT("/Game/Shinobi/VFX/T_ChidoriDent.T_ChidoriDent");
	inline const TCHAR* RasenganCrater = TEXT("/Game/Shinobi/VFX/T_RasenganCrater.T_RasenganCrater");
	/** A ring of cracks (Almighty Push); the ring sits at about 0.77 of the mark's radius. */
	inline const TCHAR* PushCrackRing = TEXT("/Game/Shinobi/VFX/T_PushCrackRing.T_PushCrackRing");

	/**
	 * Count chunks of rock thrown up from the ground under Location, outward at about Speed, each about Size metres
	 * across: engine physics bounces and rolls them, and after Linger seconds they drop through the floor and go.
	 * Local and cosmetic like SpawnScorch: call it on every machine. Does nothing without a world or on a dedicated server.
	 */
	SHINOBICOOP_API void SpawnDebris(UWorld* World, const FVector& Location, int32 Count, float Speed, float Size, float Linger);

	/** The Water Wall's flowing-water material (M_WaterWall, built by Scripts/setup_vfx.py) tinted Color: the Water Dragon's body.
	 * Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeWater(UObject* Outer, const FLinearColor& Color, float Intensity);

	/** Susanoo's glowing spectral armour (M_Susanoo, built by Scripts/setup_vfx.py) at Fade 0-1. Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeSusanoo(UObject* Outer, float Fade);

	/** Kamui's see-through chakra-blue ghost (M_Phased, built by Scripts/setup_vfx.py) at Opacity 0-1. Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakePhased(UObject* Outer, float Opacity);

	/** Almighty Push's milky, see-through sphere of force that bends the world behind it (M_PushDome, built by
	 * Scripts/setup_vfx.py). Fade 0-1 is a parameter. Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakePushDome(UObject* Outer);

	/** Flat unlit colour (M_Streak): the blood under Amaterasu's eyes. Null, with an ensure, if the asset is missing. */
	SHINOBICOOP_API UMaterialInstanceDynamic* MakeStreak(UObject* Outer, const FLinearColor& Color);
}
