#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ShinobiMutableTypes.generated.h"

class USkeletalMesh;
class UTexture2D;

/**
 * One wardrobe piece (a row of DT_Wardrobe_<Slot>): data, not graph. UShinobiMutableLook gives the sex's mesh and clip to
 * CO_Shinobi's "<Slot> Mesh" and "<Slot> Clip" parameters at runtime, so a new row needs no recompile. Meshes are authored
 * on each sex's base body (average build) and reshaped to the chosen build. Body skin under the piece is hidden by its clip
 * mesh, a closed volume around the covered body (Mutable's Clip Skeletal Mesh With Skeletal Mesh; Remove Mesh Blocks needs
 * layouts the generated body doesn't carry).
 */
USTRUCT(BlueprintType)
struct FShinobiWardrobeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TSoftObjectPtr<USkeletalMesh> MaleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TSoftObjectPtr<USkeletalMesh> FemaleMesh;

	/** The wardrobe source each sex's mesh was made on: CO_Shinobi reshapes the piece from the base's fit morph
	 * "Fit_<source>" (Scripts/setup_mutable.py stage fits). None: from the base itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	FName MaleFit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	FName FemaleFit;

	/** Per material slot of each sex's mesh, a copy of its material that takes a palette colour (none: that slot keeps its
	 * own); UShinobiBodyComponent puts it on the built body's matching sections with ColourParameters set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TArray<TSoftObjectPtr<UMaterialInterface>> MaleTinted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TArray<TSoftObjectPtr<UMaterialInterface>> FemaleTinted;

	/** The tinted copies' vector parameters that take the colour (City Sample: A_CrowdColor_main). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TArray<FName> ColourParameters;

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

/** A wardrobe colour (a row of DT_Palette): FShinobiAppearance::SlotColour indexes the rows in order. Row 0 ("Original")
 * and an index past the rows leave a piece in its own colours. */
USTRUCT(BlueprintType)
struct FShinobiPaletteRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	FLinearColor Colour = FLinearColor::White;
};
