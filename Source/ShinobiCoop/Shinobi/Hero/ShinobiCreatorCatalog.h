#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShinobiCreatorCatalog.generated.h"

class USkeletalMesh;
class UGroomAsset;
class UGroomBindingAsset;

UENUM(BlueprintType)
enum class EShinobiSex : uint8
{
	Male,
	Female,
};

/** A body frame's build within its sex. Own is a roster character's own MetaHuman body (Mutable looks only). */
UENUM(BlueprintType)
enum class EShinobiBuild : uint8
{
	Under,
	Normal,
	Over,
	Own,
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
	Outer,
	Accessory,
	Cloak,
};

constexpr int32 ShinobiWardrobeSlotCount = 6;

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

	/** A roster MetaHuman's built blueprint: its face, grooms and LODSync are worn as on its roster body
	 * (UShinobiBodyComponent::WearMetaHumanParts) instead of Mesh with OwnGrooms. */
	UPROPERTY(EditAnywhere, Category = Creator)
	TSoftClassPtr<AActor> MetaHuman;
};

/** A roster character's static head grafted onto the Mutable base of its sex (Scripts/setup_mutable.py heads). */
USTRUCT()
struct FShinobiCreatorHead
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Creator)
	FName Id;

	UPROPERTY(EditAnywhere, Category = Creator)
	EShinobiSex Sex = EShinobiSex::Male;

	UPROPERTY(EditAnywhere, Category = Creator)
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** The body skin tint that matches this head's skin (the base body material's "Basecolor Global Multiply Post-Bake"). */
	UPROPERTY(EditAnywhere, Category = Creator)
	FLinearColor SkinTint = FLinearColor::White;
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

/** A tattoo on a Mutable look: a DT_Tattoos design placed on a body region (offset, rotation and scale as small integers;
 * clients rebuild the Mutable projector from the region's anchor). */
USTRUCT()
struct FShinobiTattoo
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Design = 0;

	UPROPERTY()
	uint8 Region = 0;

	UPROPERTY()
	int8 OffsetU = 0;

	UPROPERTY()
	int8 OffsetV = 0;

	UPROPERTY()
	uint8 Rotation = 0;

	UPROPERTY()
	uint8 Scale = 128;

	bool operator==(const FShinobiTattoo& Other) const = default;
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

	/** Mutable looks (version 2): a roster head instead of a MetaHuman face (None = the face above). */
	UPROPERTY()
	uint8 Head = None;

	/** How far toward Build's morph the body goes, 0..255 (Normal has none). */
	UPROPERTY()
	uint8 BuildBlend = 255;

	UPROPERTY()
	uint8 Outer = None;

	UPROPERTY()
	uint8 Accessory = None;

	UPROPERTY()
	uint8 Cloak = None;

	/** Per wardrobe slot (EShinobiWardrobeSlot order): DT_Palette row of the piece's colour, and its wear 0..255. */
	UPROPERTY()
	uint8 SlotColour[ShinobiWardrobeSlotCount] = {};

	UPROPERTY()
	uint8 SlotWear[ShinobiWardrobeSlotCount] = {};

	/** Skin: tone and age 0..255, make-up and freckles option indices (0 = none). */
	UPROPERTY()
	uint8 SkinTone = 128;

	UPROPERTY()
	uint8 Aged = 0;

	UPROPERTY()
	uint8 MakeUp = 0;

	UPROPERTY()
	uint8 Freckles = 0;

	/** Up to ShinobiMutable::MaxTattoos placed tattoos. */
	UPROPERTY()
	TArray<FShinobiTattoo> Tattoos;

	/** The row field of a wardrobe slot. */
	uint8& Slot(EShinobiWardrobeSlot S);
	uint8 Slot(EShinobiWardrobeSlot S) const { return const_cast<FShinobiAppearance*>(this)->Slot(S); }

	/** A client's look made safe for the server: known sex and build, slider lists no longer than the sliders, at most
	 * ShinobiMutable::MaxTattoos tattoos. (Catalog and table indices past the end are left: wearing skips what isn't
	 * there.) */
	void Sanitize();

	/** The look as a short shareable code (versioned bytes, URL-safe base64), and back (false, Out untouched, if it isn't
	 * one; the result is sanitized). */
	FString ToCode() const;
	static bool FromCode(const FString& Code, FShinobiAppearance& Out);

	bool operator==(const FShinobiAppearance& Other) const = default;
};

/** A roster character's look in the creator catalog. */
USTRUCT()
struct FShinobiRosterPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Creator)
	FName Roster;

	UPROPERTY(EditAnywhere, Category = Creator)
	FShinobiAppearance Look;
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

	/** Static roster heads (FShinobiAppearance::Head indexes this, across both sexes). */
	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<FShinobiCreatorHead> Heads;

	/** Each roster character's look (Scripts/setup_mutable.py catalog). */
	UPROPERTY(EditAnywhere, Category = Creator)
	TArray<FShinobiRosterPreset> Presets;

	const FShinobiCreatorFrame* FindFrame(EShinobiSex Sex, EShinobiBuild Build) const;
	/** The frame's name as a fitting body (outfit copies SK_<Outfit>_<Name>): CrowdMNormal, CrowdFOver, ... as
	 * Scripts/setup_creator.py FRAME_BODIES names them. */
	static FString FittingName(EShinobiSex Sex, EShinobiBuild Build);
	/** The Index-th face / groom / style of that sex (and slot); null past the end. */
	const FShinobiCreatorFace* FindFace(EShinobiSex Sex, int32 Index) const;
	const FShinobiCreatorGroom* FindGroom(EShinobiSex Sex, EShinobiGroomSlot Slot, int32 Index) const;
	const FShinobiCreatorStyle* FindStyle(EShinobiSex Sex, EShinobiWardrobeSlot Slot, int32 Index) const;
	/** Heads[Index]; null for None or past the end. */
	const FShinobiCreatorHead* FindHead(uint8 Index) const;
	/** The preset of the roster character named Roster, or null. */
	const FShinobiAppearance* FindPreset(FName Roster) const;
	/** Editor (Scripts/setup_mutable.py catalog; FShinobiAppearance's fields aren't visible to Python): Roster's preset is a
	 * default look with these fields, Pieces as its Top, Bottom and Shoes, and no hair, beard or mustache (a MetaHuman
	 * face's blueprint carries its hair). */
	UFUNCTION(BlueprintCallable, Category = Creator)
	void SetPreset(FName Roster, EShinobiSex Sex, uint8 Head, uint8 Face, EShinobiBuild Build, uint8 Pieces);
	UGroomBindingAsset* LoadBinding(FName Face, FName Groom) const;

	int32 CountFaces(EShinobiSex Sex) const;
	int32 CountGrooms(EShinobiSex Sex, EShinobiGroomSlot Slot) const;
	int32 CountStyles(EShinobiSex Sex, EShinobiWardrobeSlot Slot) const;
};
