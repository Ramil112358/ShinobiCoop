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

	/** A Chaos cloth asset's render mesh (a MetaHuman Fab outfit's) as the skeletal mesh at AssetPath (made, or replaced),
	 * through the engine's own converter (UChaosClothAssetBase::ExportToSkeletalMesh). Null if that fails. The caller saves it. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static USkeletalMesh* ExportClothAsset(UObject* ClothAsset, const FString& AssetPath);

	/** Mesh 0, LOD 0 of a body DNA (a MetaHuman compatibility body: .../MetaHumanCharacter/Content/Optional/Body/
	 * FixedCompatibility/<type>.dna) as triangles in mesh space, cm. False when the file has no geometry. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static bool ReadDnaBody(const FString& DnaPath, TArray<FVector>& Positions, TArray<int32>& Triangles);

	/** LOD 0 of Mesh in its reference pose (with morph target Morph fully applied, when given), as triangles. False without a
	 * mesh description (outside the editor). */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static bool ReadMeshSurface(USkeletalMesh* Mesh, TArray<FVector>& Positions, TArray<int32>& Triangles, FName Morph = NAME_None);

	/** ReadMeshSurface of Mesh posed like Target (PoseLike): for wardrobe sources made on another body (the Mutable Sample's,
	 * the UE mannequin's) whose skeleton shares Target's bone names. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static bool ReadPosedSurface(USkeletalMesh* Mesh, USkeletalMesh* Target, TArray<FVector>& Positions, TArray<int32>& Triangles);

	/**
	 * Makes Out (a copy of Base) Piece fitted to Base: Piece's LOD 0 (posed like Base as its body PoseFrom is, when given:
	 * ReadPosedSurface) moved from its closest point on Base's fit shape (morph FitMorph) to the same point of Base, its
	 * offset kept, and skinned like that point; with Piece's materials, on every LOD. The row then needs no fit morph:
	 * CO_Shinobi's own reshape extrapolates a vertex off a sheared fit triangle into a spike (cuffs, hoods). Returns the
	 * vertex count (0: nothing done). The caller saves Out.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static int32 RebindPiece(USkeletalMesh* Out, USkeletalMesh* Base, FName FitMorph, USkeletalMesh* Piece, USkeletalMesh* PoseFrom);

	/**
	 * Adds (or replaces) morph target Morph on every LOD of Base (a Mutable base body), moving each vertex onto the closest
	 * point of the surface: the body a wardrobe source was made on, in that source's space. The moves are smoothed Smooth
	 * times over each vertex's neighbours and capped at MaxMove cm. Vertices skinned to KeepBones or their children stay
	 * (by their skin weight on them, faded out over KeepFade rings of neighbours): where the bodies differ in pose or end
	 * (hands, the neck into the head) projecting shears triangles, and Reshape stretches a piece bound there into a spike.
	 * CO_Shinobi applies the morph to the base for a piece's reshape Base Shape: Mutable's Skeletal Mesh Reshape needs its
	 * Base and Target shapes to share topology, which a morph of the base keeps (a separately built copy does not: the
	 * build splits vertices by their moved normals). Returns a
	 * report. The caller saves Base.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static FString BakeFitMorph(USkeletalMesh* Base, FName Morph, const TArray<FVector>& Positions, const TArray<int32>& Triangles, float MaxMove,
		int32 Smooth, const TArray<FName>& KeepBones, int32 KeepFade = 12);

	/**
	 * Makes Clip (a copy of Base) the coverage volume of Piece: the closed shell under the part of Base (with its morph
	 * FitMorph: the piece's fit shape, when given) that Piece covers, for CO_Shinobi's Clip With Mesh to hide that skin.
	 * A vertex is covered when its outward normal (from 1 cm inside) hits Piece within Reach cm; triangles with all three
	 * corners covered, eroded by Erode rings so skin stays at the hems, are taken on Base's own shape (the fit shape only
	 * judges coverage: clips aren't reshaped, and the average built body is Base), pushed Out cm outward and joined by side walls
	 * to a copy pushed Depth cm inward. Skinned like Base, on every LOD (the caller may drop all but LOD 0). With Below (cm,
	 * shoes), a vertex is also covered when Piece is straight above it within Below cm, and every toe near the shoe is:
	 * the whole foot inside a shoe, toes past a short shoe's tip included. The volume is then remeshed from its winding number into one clean closed surface (the
	 * layers cross where skin folds; Mutable judges inside by hits straight up). Returns the covered triangles (0: Clip
	 * unchanged, nothing covered). The caller saves Clip.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static int32 MakeClipVolume(USkeletalMesh* Clip, USkeletalMesh* Base, FName FitMorph, USkeletalMesh* Piece, float Reach = 4.f, int32 Erode = 1,
		float Out = 0.3f, float Depth = 3.f, float Below = 0.f);

	/** The share of Piece's LOD 0 vertices more than Tolerance cm inside Body's LOD 0 surface (with Body's morph Morph
	 * applied, when given), by the closest triangle's outward side. -1 without mesh descriptions. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static float FractionInside(USkeletalMesh* Piece, USkeletalMesh* Body, float Tolerance = 0.5f, FName Morph = NAME_None);

	/** The share of Points inside the closed volume Volume's LOD 0 surface as Mutable's clipping judges it (an odd number
	 * of hits straight up: overlapping parts cancel). -1 without a mesh description or points. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static float FractionOfPointsInside(const TArray<FVector>& Points, USkeletalMesh* Volume);
};
