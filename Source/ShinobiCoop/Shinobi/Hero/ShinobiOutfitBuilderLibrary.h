#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiOutfitBuilderLibrary.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

/**
 * Editor-only helpers for Scripts/setup_hero.py: Chaos cloth on the outfit copies fitted to each roster body
 * (Scripts/character/skin_outfit.py; docs/character/outfit-steps.md). Every function logs and returns false on failure
 * (and does nothing outside the editor). The caller saves the assets.
 */
UCLASS()
class SHINOBICOOP_API UShinobiOutfitBuilderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Makes LOD 0 section 0 of Mesh one clothing asset (replacing any it had) that collides with Collision's spheres and
	 * capsules. Its Max Distance (how far each vertex may swing from where the skin puts it, cm) comes from the vertex
	 * colours skin_outfit.py paints: R is the hem share (x HemMaxDistanceCm), G the sleeve share (x SleeveMaxDistanceCm),
	 * the larger wins; 0 pins the vertex to the body. A mesh without those colours (blue channel 255 everywhere) gets a
	 * stand-in: the hem share ramps from 0 at the hips bone to 1 30 cm below it. The cloth config starts from the
	 * defaults below, which the user then tunes in the Skeletal Mesh editor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Outfit")
	static bool BuildCloth(USkeletalMesh* Mesh, UPhysicsAsset* Collision, float HemMaxDistanceCm, float SleeveMaxDistanceCm);
};
