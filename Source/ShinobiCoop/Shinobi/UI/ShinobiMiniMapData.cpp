#include "Shinobi/UI/ShinobiMiniMapData.h"
#include "Engine/Texture2D.h"

bool UShinobiMiniMapData::HasValidBounds() const
{
	return FMath::IsFinite(WorldMin.X) && FMath::IsFinite(WorldMin.Y)
		&& FMath::IsFinite(WorldMax.X) && FMath::IsFinite(WorldMax.Y)
		&& WorldMax.X > WorldMin.X && WorldMax.Y > WorldMin.Y;
}

bool UShinobiMiniMapData::HasValidTileGrid() const
{
	if (TileCount.X <= 0 || TileCount.Y <= 0 || Tiles.Num() != TileCount.X * TileCount.Y)
	{
		return false;
	}

	TSet<FIntPoint> Coordinates;
	for (const FShinobiMiniMapTile& Tile : Tiles)
	{
		if (Tile.X < 0 || Tile.X >= TileCount.X || Tile.Y < 0 || Tile.Y >= TileCount.Y
			|| Coordinates.Contains(FIntPoint(Tile.X, Tile.Y)) || !Tile.Base
			|| Tile.UVMin.X < 0.f || Tile.UVMin.Y < 0.f || Tile.UVMax.X > 1.f || Tile.UVMax.Y > 1.f
			|| Tile.UVMax.X <= Tile.UVMin.X || Tile.UVMax.Y <= Tile.UVMin.Y)
		{
			return false;
		}
		Coordinates.Add(FIntPoint(Tile.X, Tile.Y));
	}
	return true;
}

FVector2D UShinobiMiniMapData::WorldToUV(const FVector& WorldLocation) const
{
	if (!HasValidBounds())
	{
		return FVector2D::ZeroVector;
	}

	return FVector2D(
		FMath::Clamp((WorldLocation.X - WorldMin.X) / (WorldMax.X - WorldMin.X), 0.f, 1.f),
		FMath::Clamp((WorldLocation.Y - WorldMin.Y) / (WorldMax.Y - WorldMin.Y), 0.f, 1.f));
}

UShinobiMiniMapData* UShinobiMiniMapData::ForLevel(const FString& LevelName)
{
	FString Name = LevelName;
	if (!Name.RemoveFromStart(TEXT("Lvl_")))
	{
		return nullptr;
	}
	const FString Path = FString::Printf(TEXT("/Game/Shinobi/UI/Minimaps/%s/DA_%sMiniMap.DA_%sMiniMap"), *Name, *Name, *Name);
	return LoadObject<UShinobiMiniMapData>(nullptr, *Path, nullptr, LOAD_NoWarn);
}
