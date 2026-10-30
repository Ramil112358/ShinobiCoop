#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiSkinLibrary.generated.h"

class UMaterial;

/**
 * Editor-only skin material edits for Scripts/setup_mutable.py (P3b spike, docs/superpowers/plans/2026-10-30-mutable-
 * customisation-p3b-skin.md): layers drawn over a MetaHuman skin material's baked base colour.
 */
UCLASS()
class SHINOBICOOP_API UShinobiSkinLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Inserts a tattoo layer in front of Material's Material Attributes output (MetaHuman skin materials route everything
	 * through attributes): texture parameter "Tattoo", placed in UV space by vector "Tattoo Offset" (U, V), scalars
	 * "Tattoo Scale" (UV size) and "Tattoo Rotation" (turns), blended over the base colour by its alpha times scalar
	 * "Tattoo Opacity" (default 0: no change). False if Material has no attributes output. Recompiles; the caller saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Skin")
	static bool AddTattooLayer(UMaterial* Material);
};
