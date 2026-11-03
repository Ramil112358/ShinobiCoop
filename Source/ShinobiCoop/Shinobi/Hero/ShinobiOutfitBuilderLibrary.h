#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiOutfitBuilderLibrary.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

/**
 * Editor-only helpers for Scripts/setup_hero.py: Chaos cloth on the outfit copies fitted to each roster body
 * (Scripts/character/skin_outfit.py; docs/character/outfit-steps.md), and the bodies' ragdolls. Every function logs and
 * returns false on failure (and does nothing outside the editor). The caller saves the assets.
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
	 * Adds (or replaces) a capsule named "Tail" on Physics (Body's cloth collision, BuildClothCollision), on its Hips body,
	 * for its outfits' cloth to hang over the tail. Root and Tip are the tail capsule's ends relative to the Hips joint
	 * and Radius its radius, in metres in Blender's space for the body file (body_landmarks.find_tail: Z up, the body
	 * facing -Y); they become UE's component space here (Y flipped, cm).
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Outfit")
	static bool AddTailCapsule(USkeletalMesh* Body, UPhysicsAsset* Physics, FVector RootFromHips, FVector TipFromHips, float Radius);

	/**
	 * A cloth-only physics asset for Body at AssetPath (created or rebuilt; the caller saves it): capsules fitted to the
	 * vertices each bone carries, plus the tail (AddTailCapsule), kept apart from the ragdoll's. Sizes are in bone units,
	 * so the minimum bone size is divided by the rig's scale (1 for bodies cleaned by Scripts/character/clean_rig.py).
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Outfit")
	static UPhysicsAsset* BuildClothCollision(USkeletalMesh* Body, const FString& AssetPath);

	/** How many bodies (one per bone it simulates) Physics has; 0 for none. setup_hero.py rebuilds a ragdoll with too few. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Outfit")
	static int32 NumPhysicsBodies(const UPhysicsAsset* Physics);

	/**
	 * Rebuilds Mesh's own physics asset (its death ragdoll, which also sizes the mesh's bounds) in place with the editor's
	 * default settings, the ones SkeletalMeshEditorSubsystem.create_physics_asset gave the other bodies. For an asset made
	 * before the rig was cleaned to unit-scale bones: every bone looked shorter than the minimum size then, so it got only
	 * a few torso bodies. The caller saves the asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Outfit")
	static bool RebuildRagdoll(USkeletalMesh* Mesh);
};
