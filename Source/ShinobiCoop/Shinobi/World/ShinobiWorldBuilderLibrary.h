#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiWorldBuilderLibrary.generated.h"

class ALandscape;
class UMaterialInterface;
class UStaticMesh;

/**
 * Editor-only helpers for Scripts/world/build_world.py (sub-project #3): what UE's Python API can't do on its own.
 * Every function logs and returns null/false on failure (and does nothing outside the editor).
 */
UCLASS()
class SHINOBICOOP_API UShinobiWorldBuilderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Metres per landscape quad; the landscape's X/Y scale is QuadMeters * 100 uu. */
	static constexpr double QuadMeters = 1.0;
	/** Quads per section (and per component: 1 section each). An image of N*63+1 pixels makes N x N components. */
	static constexpr int32 QuadsPerSection = 63;

	/**
	 * Creates a World Partition landscape centred on the origin from a 16-bit heightmap PNG (UE encoding: 32768 = 0,
	 * 128 per metre at the Z scale of 100 used here), one metre per pixel, square, N*63+1 pixels. Each weightmap PNG
	 * (8-bit, same size) becomes a paint layer named by its key, with a layer info asset created in LayerInfoPath.
	 * Streaming proxies are split at WorldPartitionGridSize components per proxy.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|World")
	static ALandscape* CreateLandscape(UWorld* World, const FString& HeightmapPng, UMaterialInterface* Material,
		const TMap<FName, FString>& WeightmapPngs, const FString& LayerInfoPath, int32 WorldPartitionGridSize = 4);

	/**
	 * Replaces a tree's collision with one upright capsule around its trunk: the vertices in the bottom 10% of the
	 * mesh's height give the trunk centre and radius (80th percentile of their distance from it, clamped to 10-80 cm);
	 * the capsule reaches 60% of the tree's height, at most 6 m. Traces use it too (simple as complex), so leaves never
	 * snag players, cameras or kunai. Returns the radius in cm, 0 on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|World")
	static float SetTrunkCollision(UStaticMesh* Mesh);
};
