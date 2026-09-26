#pragma once

#include "CoreMinimal.h"

class UObject;

/** Placeholder sounds (Scripts/make_sfx.py, imported by Scripts/setup_m5.py as /Game/Shinobi/Audio/SFX_<Name>). */
enum class EShinobiSfx : uint8
{
	Whoosh,    // melee swing
	Swish,     // dodge
	Impact,    // a hit landing
	Explosion, // fireball / firestorm
	Zap,       // Chidori
	Whirr,     // Rasengan
	Poof,      // Substitution
	Cast,      // jutsu wind-up
	Gong,      // wave start
	Victory,
	Thud,      // going down
};

namespace ShinobiSfx
{
	/** Cosmetic, local: plays nothing on a dedicated server or when the sound asset is missing. */
	SHINOBICOOP_API void Play(const UObject* WorldContext, EShinobiSfx Sfx, const FVector& Location, float Volume = 1.f);
	SHINOBICOOP_API void Play2D(const UObject* WorldContext, EShinobiSfx Sfx, float Volume = 1.f);
}
