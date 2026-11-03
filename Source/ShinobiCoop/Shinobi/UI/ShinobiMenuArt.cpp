#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/PackageName.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

namespace
{
	FString MenuArtPath(const FString& Name)
	{
		return FString::Printf(TEXT("/Game/Shinobi/UI/Menu/%s.%s"), *Name, *Name);
	}
}

FString ShinobiMenuArt::MapId(const FShinobiMap& Map)
{
	FString Id = FPackageName::GetShortName(Map.Path);
	Id.RemoveFromStart(TEXT("Lvl_"));
	return Id;
}

FString ShinobiMenuArt::MapImagePath(const FShinobiMap& Map)
{
	return MenuArtPath(TEXT("T_Map_") + MapId(Map));
}

FString ShinobiMenuArt::PortraitPath(int32 RosterIndex)
{
	return MenuArtPath(FString(TEXT("T_Portrait_")) + AShinobiPlayerCharacter::Roster[RosterIndex].Name);
}

UTexture2D* ShinobiMenuArt::LoadArt(const FString& Path)
{
	// Check first: a missing package logs even with LOAD_Quiet on some paths.
	if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)))
	{
		return nullptr;
	}
	return LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

UTexture2D* ShinobiMenuArt::MadePortrait(const FShinobiCharacterSlot& Slot)
{
	if (Slot.Portrait.IsEmpty())
	{
		return nullptr;
	}
	return FImageUtils::ImportBufferAsTexture2D(TArrayView64<const uint8>(Slot.Portrait.GetData(), Slot.Portrait.Num()));
}
