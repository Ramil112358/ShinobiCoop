#pragma once

#include "CoreMinimal.h"

struct FShinobiMap;
struct FShinobiCharacterSlot;
class UTexture2D;

/** Which control a mouse picture lights. */
enum class EShinobiMouseIcon : uint8
{
	Wheel,
	RightButton,
};

/**
 * The main menu's pictures: roster portraits and map images captured in-game (Scripts/menu_shots.ps1) and imported to
 * /Game/Shinobi/UI/Menu (Scripts/setup_menu_art.py). A picture not captured yet loads as null; the menu draws a card without it.
 */
namespace ShinobiMenuArt
{
	/** Map.Path's level name without "Lvl_": "London", "Arena_Camden", "ThirdPerson". */
	SHINOBICOOP_API FString MapId(const FShinobiMap& Map);
	SHINOBICOOP_API FString MapImagePath(const FShinobiMap& Map);
	/** AShinobiPlayerCharacter::Roster[RosterIndex]'s portrait. */
	SHINOBICOOP_API FString PortraitPath(int32 RosterIndex);
	/** Quiet load: null (and no warning) when the picture isn't there. */
	SHINOBICOOP_API UTexture2D* LoadArt(const FString& Path);
	/** A made character's portrait (FShinobiCharacterSlot::Portrait) as a transient texture; null when it has none yet. */
	SHINOBICOOP_API UTexture2D* MadePortrait(const FShinobiCharacterSlot& Slot);
	/** Where the face sits in a 3:4 portrait, from the top (the menu stage films made characters' this way; the HUD's round
	 * picture is centred on it). */
	constexpr float MadePortraitFaceV = 0.39f;
	/** A white mouse outline with Lit filled in the accent (the preview's zoom and rotate hints), drawn into a new
	 * 64 px transient texture: keep it referenced (a brush in a UPROPERTY does). Null if the texture can't be made. */
	SHINOBICOOP_API UTexture2D* MouseIcon(EShinobiMouseIcon Lit);
}
