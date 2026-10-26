#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "ShinobiBodyComponent.generated.h"

class UIKRetargeter;

/**
 * A character's visible body: a skeletal mesh attached to the character's own mesh (the "driver", a mannequin that
 * keeps running every animation, hidden) that copies the driver's pose every frame through an IK retargeter
 * (UShinobiRetargetAnimInstance). Players wear the hero or their roster pick; enemies and villagers wear the characters
 * Scripts/setup_hero.py imports to /Game/Shinobi/Characters/<Name>/.
 */
UCLASS(ClassGroup = Shinobi)
class SHINOBICOOP_API UShinobiBodyComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	UShinobiBodyComponent(const FObjectInitializer& ObjectInitializer);

	/** Wears the mesh and retargeter at these object paths; false (nothing changes) when either isn't imported. With a
	 * MetaHuman blueprint class (its body is MeshPath), its other skeletal meshes and grooms are worn too (WearMetaHumanParts). */
	bool Wear(const TCHAR* MeshPath, const TCHAR* RetargeterPath, const TCHAR* MetaHumanClassPath = nullptr);

	/** Wears a creator look (UShinobiCreatorCatalog): the frame's body, its face (on UShinobiFaceShapeAnimInstance, so
	 * SetFaceShape works), the grooms bound to that face and the wardrobe pieces fitted to the frame. False (nothing
	 * changes) when the catalog or the frame isn't there. */
	bool WearAppearance(const FShinobiAppearance& Look);

	/** The face sliders and preset blends of Look on the face already worn (no rebuild: the creator's live edits). */
	void UpdateFace(const FShinobiAppearance& Look);

	/** Body sliders of Look (bone scales on this body, height on the whole character, capsule included) on the body
	 * already worn: the creator's live edits. */
	void UpdateBody(const FShinobiAppearance& Look);

	/** The body slider names, in order (FShinobiAppearance::BodyShape). */
	static TArray<FName> GetBodySliderNames();

	/** The look last worn by WearAppearance (or edited by UpdateFace / UpdateBody). */
	const FShinobiAppearance& GetAppearance() const { return Worn; }

	/** The creator frame worn as a fitting body (UShinobiCreatorCatalog::FittingName), empty for a roster body. */
	const FString& GetFittingName() const { return FittingName; }

	/** Wears /Game/Shinobi/Characters/<Name>/SK_<Name> with RTG_MannyTo<Name>. */
	bool WearCharacter(const FString& Name);

	/** Hides the driver (the mesh this is attached to) and keeps it animating unseen. */
	void HideDriver();

	/** A MetaHuman's face, grooms and LODSync worn with this body (WearMetaHumanParts); empty otherwise. */
	const TArray<TObjectPtr<UActorComponent>>& GetMetaHumanParts() const { return MetaHumanParts; }

	/** Wears Head, a static (rig-less) head on this body's skeleton (Scripts/character/graft_head.py), following this
	 * body's pose; a MetaHuman's face and grooms come off. Null takes the head off. */
	void WearStaticHead(USkeletalMesh* Head);

	/** Sets a body morph (Scripts/setup_creator.py) on this body and on every mesh following its pose (outfit, head):
	 * the copies carry the same morphs (Scripts/character/transfer_morphs.py), so they keep fitting. */
	void SetBodyMorph(FName Morph, float Weight);

	/** Scales Bone (local axes; one, or absent, is none) on top of the retargeted pose, and every mesh following this
	 * body's pose with it, cloth included. A bone with children of its own (a spine or pelvis bone, not a twist or
	 * corrective helper) passes the inverse to those children, so only the skin it carries changes shape. */
	void SetBoneShape(FName Bone, const FVector& Scale);
	const TMap<FName, FVector>& GetBoneShapes() const { return BoneShapes; }

	/** Sets a face slider (UShinobiFaceShapeAnimInstance) on the MetaHuman face worn with this body; the first call puts
	 * the face on that native graph (body pose, RigLogic, slider offsets) in place of its blueprints. False without a
	 * MetaHuman face or for an unknown slider. */
	bool SetFaceShape(FName Slider, float Value);

	/** The retargeter of the body worn; UShinobiRetargetAnimInstance reads it when it (re)initialises. */
	UIKRetargeter* GetRetargeter() const { return Retargeter; }

private:
	/**
	 * Copies the MetaHuman blueprint's components other than its root and Body onto the owner under their blueprint names:
	 * the face (its ABP_Face copies this body's pose as its attached parent) and the hair and eyebrow grooms bound to it,
	 * attached as in the blueprint with this standing in for Body, then its LODSync, re-pointed from Body to this.
	 * MetaHumanComponentUE (body correctives) is left out. Clears the previous MetaHuman's parts first; null just clears.
	 */
	void WearMetaHumanParts(const TCHAR* ClassPath);

	UPROPERTY(Transient)
	TObjectPtr<UIKRetargeter> Retargeter;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> MetaHumanParts;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> StaticHead;

	TMap<FName, FVector> BoneShapes;

	FShinobiAppearance Worn;

	FString FittingName;
};
