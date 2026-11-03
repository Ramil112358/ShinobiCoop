#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "ShinobiBodyComponent.generated.h"

class UIKRetargeter;

/** How a rider reacts to the dragon under it (AShinobiDragonMount::UpdateRiderPose), laid over the seated pose by UShinobiRetargetAnimInstance. */
struct FShinobiRidePose
{
	float Pull[2] = { 0.f, 0.f }; // left, right rein: 1 draws that fist back a hand's length, negative lets it forward
	float Roll = 0.f;             // the torso leans to the right, degrees
	float Forward = 0.f;          // the torso leans forward, degrees (negative: back)

	bool IsRest() const { return FMath::IsNearlyZero(Pull[0], 0.01f) && FMath::IsNearlyZero(Pull[1], 0.01f) && FMath::IsNearlyZero(Roll, 0.1f) && FMath::IsNearlyZero(Forward, 0.1f); }
};

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

	/** Wears a creator look once Mutable has built its body and wardrobe (UShinobiMutableLook): then the body, the crowd
	 * retargeter, its face (UShinobiCreatorCatalog, on UShinobiFaceShapeAnimInstance, so SetFaceShape works) and the
	 * grooms bound to it go on together, and OnBodyBuilt fires. Until then (or if the build fails) what is worn stays.
	 * False (nothing changes or is pending) when the catalog, the retargeter or CO_Shinobi isn't there. */
	bool WearAppearance(const FShinobiAppearance& Look);

	/** After WearAppearance's body is on (the character re-dresses its outfit to the new skeleton then). */
	FSimpleMulticastDelegate OnBodyBuilt;

	/** Whether a Mutable body build for the last WearAppearance is still on its way. */
	bool IsMutableBuildPending() const;

	/** The face sliders and preset blends of Look on the face already worn (no rebuild: the creator's live edits). */
	void UpdateFace(const FShinobiAppearance& Look);

	/** Body sliders of Look (bone scales on this body, height on the whole character, capsule included) on the body
	 * already worn: the creator's live edits. */
	void UpdateBody(const FShinobiAppearance& Look);
	/** The look's skin tone, tattoos, make-up, freckles and piece colours on the worn body at once, without a Mutable
	 * build (the creator's skin rows and swatches; the worn mesh and head stay). */
	void UpdateSkin(const FShinobiAppearance& Look);

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

	/** The engine's bounds (the physics asset's bodies in the current pose) grown to every bone plus SkinPaddingCm, so a
	 * physics asset that misses the limbs or head can't leave the bounds inside the body (see the definition). */
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	/** How far the skin (and hair) reaches past the bones it hangs on, in the component's units: CalcBounds' margin. */
	static constexpr float SkinPaddingCm = 20.f;

	/** A MetaHuman's face, grooms and LODSync worn with this body (WearMetaHumanParts); empty otherwise. */
	const TArray<TObjectPtr<UActorComponent>>& GetMetaHumanParts() const { return MetaHumanParts; }
	/** Skin Tone's multiplier: ShinobiMutable::SkinToneDark at 0, white at 128, SkinToneLight at 255, linear between. */
	static FLinearColor SkinToneMultiplier(uint8 Tone);

	/** The static head worn with this body (WearStaticHead), or null. */
	USkeletalMeshComponent* GetStaticHead() const { return StaticHead; }

	/** Wears Head, a static (rig-less) head on this body's skeleton (Scripts/character/graft_head.py), following this
	 * body's pose; a MetaHuman's face and grooms come off. Null takes the head off. */
	void WearStaticHead(USkeletalMesh* Head);


	/** Scales Bone (local axes; one, or absent, is none) on top of the retargeted pose, and every mesh following this
	 * body's pose with it, cloth included. A bone with children of its own (a spine or pelvis bone, not a twist or
	 * corrective helper) passes the inverse to those children, so only the skin it carries changes shape. */
	void SetBoneShape(FName Bone, const FVector& Scale);
	const TMap<FName, FVector>& GetBoneShapes() const { return BoneShapes; }

	/** The rider's lean and rein pull on top of the pose (all at rest: none); set each frame while a dragon carries this body. */
	void SetRidePose(const FShinobiRidePose& Pose) { RidePose = Pose; }
	const FShinobiRidePose& GetRidePose() const { return RidePose; }

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

	/** Puts PendingLook on with Built, its Mutable body (null: nothing changes). */
	void DressBuilt(USkeletalMesh* Built);

	/** Each worn piece's sections in its slot's palette colour (FShinobiWardrobeRow::MaleTinted etc.). */
	void ColourPieces(const FShinobiAppearance& Look);

	/** The look's skin tone on the body (times HeadTint, a static head's neck match), the static head and a MetaHuman face. */
	void ApplySkinTone(const FShinobiAppearance& Look, const FLinearColor& HeadTint);

	/** The look's tattoos on the body's skin layer (M_ShinobiSkin; unused slots off, unknown designs or regions skipped),
	 * and its make-up and freckles on a MetaHuman face's. */
	void ApplySkinLayers(const FShinobiAppearance& Look);
	/** Stands the body Lift cm higher than its place (a worn shoe's sole; 0: back on it). */
	void LiftForSoles(float Lift);
	/** How far LiftForSoles has the body raised. */
	float SoleLift = 0.f;
	/** The worn look's sole (cm at height 1; DressBuilt), which UpdateBody lifts the body by. */
	float WornSole = 0.f;
	/** The worn static head's neck match (white without one): UpdateSkin tones the body over it. */
	FLinearColor WornHeadTint = FLinearColor::White;

	UPROPERTY(Transient)
	TObjectPtr<UIKRetargeter> Retargeter;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> MetaHumanParts;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> StaticHead;

	UPROPERTY(Transient)
	TObjectPtr<class UShinobiMutableLook> MutableLook;

	TMap<FName, FVector> BoneShapes;
	FShinobiRidePose RidePose;

	FShinobiAppearance Worn;

	/** The look whose Mutable body is being built. */
	FShinobiAppearance PendingLook;

	FString FittingName;
};
