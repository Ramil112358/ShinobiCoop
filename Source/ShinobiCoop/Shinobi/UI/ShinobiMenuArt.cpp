#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
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
