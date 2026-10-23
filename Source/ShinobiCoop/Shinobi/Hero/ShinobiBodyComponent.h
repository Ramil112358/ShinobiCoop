#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
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

	/** Wears /Game/Shinobi/Characters/<Name>/SK_<Name> with RTG_MannyTo<Name>. */
	bool WearCharacter(const FString& Name);

	/** Hides the driver (the mesh this is attached to) and keeps it animating unseen. */
	void HideDriver();

	/** A MetaHuman's face, grooms and LODSync worn with this body (WearMetaHumanParts); empty otherwise. */
	const TArray<TObjectPtr<UActorComponent>>& GetMetaHumanParts() const { return MetaHumanParts; }

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
};
