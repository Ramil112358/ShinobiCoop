#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiFaceBlendLibrary.generated.h"

class USkeletalMesh;

/**
 * Editor-only helper for Scripts/setup_creator.py (character creator preset blending): bakes, into a MetaHuman face's
 * mesh descriptions, one morph target per other face of the same topology that turns this face's neutral shape into that
 * face's (both aligned at FACIAL_C_FacialRoot). UShinobiBodyComponent::UpdateFace sets them by the blend weights.
 * Mesh descriptions keep morphs across rebuilds and leave the face's DNA and RigLogic alone. The caller saves the mesh.
 */
UCLASS()
class SHINOBICOOP_API UShinobiFaceBlendLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Adds (or replaces) morph "Blend_<Names[i]>" toward Others[i] on every LOD of Face up to MaxLods whose mesh
	 * description has the same vertex count as that face's; other LODs and mismatched faces are skipped and reported.
	 * Returns a one-line report per face ("" outside the editor).
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Creator")
	static FString BakeFaceBlendMorphs(USkeletalMesh* Face, const TArray<USkeletalMesh*>& Others, const TArray<FName>& Names, int32 MaxLods = 4);
};
