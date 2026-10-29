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
};
