#pragma once

#include "CoreMinimal.h"

struct FShinobiMap;
class UTexture2D;

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
}
