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
	 * the larger wins; 0 pins the vertex to the body (a mesh without those colours is refused). The cloth config starts from
	 * the defaults in SetDefaultConfig, which the user then tunes in the Skeletal Mesh editor (CopyClothTuning shares it).
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Outfit")
	static bool BuildCloth(USkeletalMesh* Mesh, UPhysicsAsset* Collision, float HemMaxDistanceCm, float SleeveMaxDistanceCm);

	/**
	 * Copies the user's cloth tuning from Source onto each of Targets (copies of the same outfit fitted to other bodies):
	 * every cloth config (stiffness, damping, ...) and every enabled painted weight map (Max Distance, Backstop, Anim
	 * Drive, ...), each target vertex taking the value of the source vertex nearest it in the master garment's space
	 * (UV channels 1 and 2 of the copies, skin_outfit.py). Targets keep their own collision.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Outfit")
	static bool CopyClothTuning(USkeletalMesh* Source, const TArray<USkeletalMesh*>& Targets);

	/**
	 * Adds (or replaces) a capsule named "Tail" on Body's physics asset, on its Hips body, for its outfits' cloth to hang
	 * over the tail (it collides in the ragdoll too). Root and Tip are the tail capsule's ends relative to the Hips joint
	 * and Radius its radius, in metres in Blender's space for the body file (body_landmarks.find_tail: Z up, the body
	 * facing -Y); they become UE's component space here (Y flipped, cm).
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Outfit")
	static bool AddTailCapsule(USkeletalMesh* Body, FVector RootFromHips, FVector TipFromHips, float Radius);
};
