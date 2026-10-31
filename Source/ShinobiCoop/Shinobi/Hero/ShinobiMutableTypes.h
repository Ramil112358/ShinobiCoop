#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
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

	/** Slots this piece takes off when worn (a whole outfit replaces Top, Bottom and Shoes; a dress, Bottom). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	TArray<EShinobiWardrobeSlot> Replaces;

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

	/** How far each sex's shoe reaches below the base's feet (cm; Scripts/setup_mutable.py wardrobe): a body wearing it
	 * stands that much higher, so the sole is on the floor, not in it. 0 for other slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	float MaleSole = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Wardrobe)
	float FemaleSole = 0.f;
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

/** A tattoo design (a row of DT_Tattoos): FShinobiTattoo::Design indexes the rows in order. */
USTRUCT(BlueprintType)
struct FShinobiTattooDesign : public FTableRowBase
{
	GENERATED_BODY()

	/** The design, with a transparent border (the skin layer clamps it: it shows once). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tattoo)
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tattoo)
	TSoftObjectPtr<UTexture2D> Thumbnail;
};

/** Where on the body a tattoo goes (a row of DT_TattooRegions): FShinobiTattoo::Region indexes the rows in order. In the
 * MetaHuman body's UVs (Scripts/setup_mutable.py stage tattoos). */
USTRUCT(BlueprintType)
struct FShinobiTattooRegion : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tattoo)
	FVector2D Centre = FVector2D(0.5, 0.5);

	/** The design's size in UV units at FShinobiTattoo::Scale 128. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tattoo)
	float Scale = 0.1f;

	/** Turns that stand the design upright there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tattoo)
	float Rotation = 0.f;
};

/** A make-up look (a row of DT_MakeUp; row 0 is none): FShinobiAppearance::MakeUp indexes the rows in order. Masks are the
 * MetaHuman plugin's face masks (Textures/ArtistDelights); a missing mask leaves that part bare. */
USTRUCT(BlueprintType)
struct FShinobiMakeUpRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MakeUp)
	TSoftObjectPtr<UTexture2D> Eye;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MakeUp)
	FLinearColor EyeColour = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MakeUp)
	TSoftObjectPtr<UTexture2D> Lips;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MakeUp)
	FLinearColor LipsColour = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MakeUp)
	TSoftObjectPtr<UTexture2D> Blush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MakeUp)
	FLinearColor BlushColour = FLinearColor::Black;
};
