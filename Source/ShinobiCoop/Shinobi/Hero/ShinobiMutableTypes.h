#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ShinobiMutableTypes.generated.h"

class USkeletalMesh;
class UTexture2D;

/**
 * One wardrobe piece (a row of DT_Wardrobe_<Slot>): Scripts/setup_mutable.py makes it a child object of the slot's group
 * in CO_Shinobi, named after the row. Meshes are authored on each sex's base body (average build) and reshaped to the
 * chosen build. Body skin under the piece is hidden by its clip mesh, a closed volume around the covered body
 * (Mutable's Clip With Mesh; Remove Mesh Blocks needs layouts the generated body doesn't carry).
 */
USTRUCT(BlueprintType)
struct FShinobiWardrobeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TSoftObjectPtr<USkeletalMesh> MaleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TSoftObjectPtr<USkeletalMesh> FemaleMesh;

	/** Closed volumes around the body each sex's mesh covers; none hides nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TSoftObjectPtr<USkeletalMesh> MaleClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TSoftObjectPtr<USkeletalMesh> FemaleClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/** Whether the piece's material takes the Wear parameter (the CREATE tab shows a Wear slider for it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	bool bHasWear = false;
};
