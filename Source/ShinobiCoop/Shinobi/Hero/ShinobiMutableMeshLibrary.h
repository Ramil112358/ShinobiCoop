#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiMutableMeshLibrary.generated.h"

class USkeletalMesh;

/**
 * Editor-only mesh edits for Scripts/setup_mutable.py's Mutable bodies.
 */
UCLASS()
class SHINOBICOOP_API UShinobiMutableMeshLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Gives the Count vertices nearest Bone's reference-pose position, on every LOD, a Weight of Bone (their other weights
	 * scaled to make room), so the mesh counts as skinning Bone. Mutable keeps a merged bone's pose from a mesh that skins
	 * it (OpMeshMerge.cpp, ComputeBoneMergePriority): a MetaHuman body doesn't skin "head" (its face mesh does), so a
	 * garment skinned to the head would otherwise put its own body's head pose on the merged skeleton. Returns how many
	 * LOD 0 vertices took the bone (0 for an unknown bone or outside the editor). The caller saves the mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static int32 AddBoneSkinning(USkeletalMesh* Mesh, FName Bone, int32 Count = 8, float Weight = 0.00392f); // (1/255: the smallest 8-bit skin weight)

	/** Mesh 0, LOD 0 of a body DNA (a MetaHuman compatibility body: .../MetaHumanCharacter/Content/Optional/Body/
	 * FixedCompatibility/<type>.dna) as triangles in mesh space, cm. False when the file has no geometry. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static bool ReadDnaBody(const FString& DnaPath, TArray<FVector>& Positions, TArray<int32>& Triangles);

	/** LOD 0 of Mesh in its reference pose (with morph target Morph fully applied, when given), as triangles. False without a
	 * mesh description (outside the editor). */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static bool ReadMeshSurface(USkeletalMesh* Mesh, TArray<FVector>& Positions, TArray<int32>& Triangles, FName Morph = NAME_None);

	/**
	 * Adds (or replaces) morph target Morph on every LOD of Base (a Mutable base body), moving each vertex onto the closest
	 * point of the surface: the body a wardrobe source was made on, in that source's space. The moves are smoothed Smooth
	 * times over each vertex's neighbours and capped at MaxMove cm. CO_Shinobi applies the morph to the base for a piece's
	 * reshape Base Shape: Mutable's Skeletal Mesh Reshape needs its Base and Target shapes to share topology, which a morph
	 * of the base keeps (a separately built copy does not: the build splits vertices by their moved normals). Returns a
	 * report. The caller saves Base.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static FString BakeFitMorph(USkeletalMesh* Base, FName Morph, const TArray<FVector>& Positions, const TArray<int32>& Triangles, float MaxMove = 8.f, int32 Smooth = 2);

	/** The share of Piece's LOD 0 vertices more than Tolerance cm inside Body's LOD 0 surface (with Body's morph Morph
	 * applied, when given), by the closest triangle's outward side. -1 without mesh descriptions. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static float FractionInside(USkeletalMesh* Piece, USkeletalMesh* Body, float Tolerance = 0.5f, FName Morph = NAME_None);
};
