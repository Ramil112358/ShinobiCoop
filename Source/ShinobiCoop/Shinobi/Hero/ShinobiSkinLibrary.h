#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiSkinLibrary.generated.h"

class UMaterial;
class USkeletalMesh;

/**
 * Editor-only skin material edits for Scripts/setup_mutable.py (stage skinlayer): layers drawn over a MetaHuman skin
 * material's baked base colour.
 */
UCLASS()
class SHINOBICOOP_API UShinobiSkinLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Inserts the skin layers in front of Material's Material Attributes output (MetaHuman skin materials route everything
	 * through attributes), each lerping the base colour toward its own, all off by default (opacity 0):
	 * - three tattoos "Tattoo <i>" (a mask: white ink, red channel), placed in UV space by vector "Tattoo <i> Offset"
	 *   (U, V), scalars "Tattoo <i> Scale" (UV size) and "Tattoo <i> Rotation" (turns), in vector "Tattoo <i> Colour" (ink)
	 *   by the mask times "Tattoo <i> Opacity", inside its own square only (a design shows once);
	 * - "Eye Makeup", "Lips Makeup", "Blush Makeup" and "Freckles" masks (red channel, face UVs) with "<mask> Colour" and
	 *   "<mask> Opacity".
	 * False if Material has no attributes output. Recompiles; the caller saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Skin")
	static bool AddSkinLayers(UMaterial* Material);

	/** The UV (channel 0) of Mesh's skin over a point Along of the way from Bone to End (0: the bone), on the side
	 * Direction points to (component space): the vertex nearest that point pushed Reach cm out. (-1, -1) if none. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Skin")
	static FVector2D SkinUV(USkeletalMesh* Mesh, FName Bone, FName End, FVector Direction, float Along = 0.5f, float Reach = 30.f);
};
