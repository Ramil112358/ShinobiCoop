#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShinobiCreatorCatalog.generated.h"

class USkeletalMesh;
class UGroomAsset;
class UGroomBindingAsset;

UENUM()
enum class EShinobiSex : uint8
{
	Male,
	Female,
};

/** A body frame's build within its sex. */
UENUM()
enum class EShinobiBuild : uint8
{
	Under,
	Normal,
	Over,
};

UENUM()
enum class EShinobiGroomSlot : uint8
{
	Hair,
	Beard,
	Mustache,
};

UENUM()
enum class EShinobiWardrobeSlot : uint8
{
	Top,
	Bottom,
	Shoes,
};

/** A base body: one of each sex's builds. */
USTRUCT()
struct FShinobiCreatorFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Creator)
	EShinobiSex Sex = EShinobiSex::Male;

	UPROPERTY(EditAnywhere, Category = Creator)
	EShinobiBuild Build = EShinobiBuild::Normal;

	UPROPERTY(EditAnywhere, Category = Creator)
	TSoftObjectPtr<USkeletalMesh> Body;
};

/** A MetaHuman face preset (its grooms are bound to it: FShinobiCreatorBinding). */
USTRUCT()
struct FShinobiCreatorFace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Creator)
	FName Id;

	UPROPERTY(EditAnywhere, Category = Creator)
	EShinobiSex Sex = EShinobiSex::Male;

	UPROPERTY(EditAnywhere, Category = Creator)
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Grooms this face always wears, already bound to it (a Creator build's eyebrows and eyelashes; City Sample faces
	 * have theirs in the skin). */
	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<TSoftObjectPtr<UGroomBindingAsset>> OwnGrooms;
};

USTRUCT()
struct FShinobiCreatorGroom
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Creator)
	FName Id;

	UPROPERTY(EditAnywhere, Category = Creator)
	EShinobiSex Sex = EShinobiSex::Male;

	UPROPERTY(EditAnywhere, Category = Creator)
	EShinobiGroomSlot Slot = EShinobiGroomSlot::Hair;

	UPROPERTY(EditAnywhere, Category = Creator)
	TSoftObjectPtr<UGroomAsset> Groom;
};

/** A groom bound to a face (grooms attach to the face mesh they were bound to). */
USTRUCT()
struct FShinobiCreatorBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Creator)
	FName Face;

	UPROPERTY(EditAnywhere, Category = Creator)
	FName Groom;

	UPROPERTY(EditAnywhere, Category = Creator)
	TSoftObjectPtr<UGroomBindingAsset> Binding;
};

/** A wardrobe piece, fitted once per build of its sex (ByBuild indexed by EShinobiBuild). */
USTRUCT()
struct FShinobiCreatorStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Creator)
	FName Id;

	UPROPERTY(EditAnywhere, Category = Creator)
	EShinobiSex Sex = EShinobiSex::Male;

	UPROPERTY(EditAnywhere, Category = Creator)
	EShinobiWardrobeSlot Slot = EShinobiWardrobeSlot::Top;

	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<TSoftObjectPtr<USkeletalMesh>> ByBuild;
};

/**
 * Everything the character creator offers (Scripts/setup_creator.py writes it from the City Sample crowd and the
 * creator's own faces). Indices in FShinobiAppearance count within the player's sex (and slot), in this order.
 */
UCLASS()
class SHINOBICOOP_API UShinobiCreatorCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* Path = TEXT("/Game/Shinobi/Characters/Creator/DA_CreatorCatalog.DA_CreatorCatalog");

	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<FShinobiCreatorFrame> Frames;

	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<FShinobiCreatorFace> Faces;

	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<FShinobiCreatorGroom> Grooms;

	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<FShinobiCreatorBinding> Bindings;

	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<FShinobiCreatorStyle> Styles;

	const FShinobiCreatorFrame* FindFrame(EShinobiSex Sex, EShinobiBuild Build) const;
	/** The frame's name as a fitting body (outfit copies SK_<Outfit>_<Name>): CrowdMNormal, CrowdFOver, ... as
	 * Scripts/setup_creator.py FRAME_BODIES names them. */
	static FString FittingName(EShinobiSex Sex, EShinobiBuild Build);
	/** The Index-th face / groom / style of that sex (and slot); null past the end. */
	const FShinobiCreatorFace* FindFace(EShinobiSex Sex, int32 Index) const;
	const FShinobiCreatorGroom* FindGroom(EShinobiSex Sex, EShinobiGroomSlot Slot, int32 Index) const;
	const FShinobiCreatorStyle* FindStyle(EShinobiSex Sex, EShinobiWardrobeSlot Slot, int32 Index) const;
	UGroomBindingAsset* LoadBinding(FName Face, FName Groom) const;

	int32 CountFaces(EShinobiSex Sex) const;
	int32 CountGrooms(EShinobiSex Sex, EShinobiGroomSlot Slot) const;
	int32 CountStyles(EShinobiSex Sex, EShinobiWardrobeSlot Slot) const;
};

/** A player's look: indices into UShinobiCreatorCatalog within the sex (None = nothing in that slot). */
USTRUCT(BlueprintType)
struct FShinobiAppearance
{
	GENERATED_BODY()

	static constexpr uint8 None = 255;

	UPROPERTY()
	EShinobiSex Sex = EShinobiSex::Male;

	UPROPERTY()
	EShinobiBuild Build = EShinobiBuild::Normal;

	UPROPERTY()
	uint8 Face = 0;

	UPROPERTY()
	uint8 Hair = 0;

	UPROPERTY()
	uint8 Beard = None;

	UPROPERTY()
	uint8 Mustache = None;

	UPROPERTY()
	uint8 Top = 0;

	UPROPERTY()
	uint8 Bottom = 0;

	UPROPERTY()
	uint8 Shoes = 0;

	/** Face sliders in UShinobiFaceShapeAnimInstance::GetSliderNames order, -127..127 for -1..1; missing ones are 0. */
	UPROPERTY()
	TArray<int8> FaceShape;

	/** Body sliders in UShinobiBodyComponent::GetBodySliderNames order, -127..127 for -1..1; missing ones are 0. */
	UPROPERTY()
	TArray<int8> BodyShape;

	/** Preset blending: up to two other faces of the sex (catalog indices, None = off) and how far toward each, 0..255. */
	UPROPERTY()
	uint8 BlendFace[2] = {None, None};

	UPROPERTY()
	uint8 BlendWeight[2] = {0, 0};

	/** A client's look made safe for the server: known sex and build, slider lists no longer than the sliders. (Catalog
	 * indices past the end are left: wearing skips what the catalog doesn't have.) */
	void Sanitize();

	/** The look as a short shareable code (versioned bytes, URL-safe base64), and back (false, Out untouched, if it isn't
	 * one; the result is sanitized). */
	FString ToCode() const;
	static bool FromCode(const FString& Code, FShinobiAppearance& Out);

	bool operator==(const FShinobiAppearance& Other) const = default;
};
