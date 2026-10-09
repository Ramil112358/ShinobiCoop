#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShinobiMiniMapData.generated.h"

class UTexture2D;

/** One coordinate-addressed piece of the baked minimap. */
USTRUCT(BlueprintType)
struct FShinobiMiniMapTile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	int32 Y = 0;

	/** Normalized tile rectangle within the complete image; edge tiles may cover less than a full grid cell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	FVector2D UVMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	FVector2D UVMax = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<UTexture2D> Base = nullptr;
};

/**
 * Baked artwork (Scripts/minimap) and the explicit world XY rectangle (cm) it covers, for one playable level.
 * Every map uses the unmirrored top-down view of UE's left-handed axes: +X east (right), +Y south (down).
 */
UCLASS(BlueprintType)
class SHINOBICOOP_API UShinobiMiniMapData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	FVector2D WorldMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	FVector2D WorldMax = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	FIntPoint TileCount = FIntPoint::ZeroValue;

	/** Row-major: index = Y * TileCount.X + X. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap")
	TArray<FShinobiMiniMapTile> Tiles;

	/** Whether the explicit world bounds can produce a finite, non-zero map transform. */
	UFUNCTION(BlueprintPure, Category = "Minimap")
	bool HasValidBounds() const;

	/** Whether the coordinate-addressed tile array is complete, unique, and contains base artwork. */
	UFUNCTION(BlueprintPure, Category = "Minimap")
	bool HasValidTileGrid() const;

	/** World position to clamped, top-left-origin map UV. U grows with +X (east); V grows with +Y (south). */
	UFUNCTION(BlueprintPure, Category = "Minimap")
	FVector2D WorldToUV(const FVector& WorldLocation) const;

	/** The map's data by convention: level Lvl_<Name> owns /Game/Shinobi/UI/Minimaps/<Name>/DA_<Name>MiniMap. Null when none is baked. */
	static UShinobiMiniMapData* ForLevel(const FString& LevelName);
};
