#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiSkinLibrary.generated.h"

class UMaterial;

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
	 * - three tattoos "Tattoo <i>" (texture), placed in UV space by vector "Tattoo <i> Offset" (U, V), scalars
	 *   "Tattoo <i> Scale" (UV size) and "Tattoo <i> Rotation" (turns), over the base colour by their alpha times
	 *   "Tattoo <i> Opacity"; clamped, so a design shows once;
	 * - "Eye Makeup", "Lips Makeup", "Blush Makeup" and "Freckles" masks (red channel, face UVs) with "<mask> Colour" and
	 *   "<mask> Opacity".
	 * False if Material has no attributes output. Recompiles; the caller saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Skin")
	static bool AddSkinLayers(UMaterial* Material);
};
