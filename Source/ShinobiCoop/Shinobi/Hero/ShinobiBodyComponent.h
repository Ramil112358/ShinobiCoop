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

	/** Wears the mesh and retargeter at these object paths; false (nothing changes) when either isn't imported. */
	bool Wear(const TCHAR* MeshPath, const TCHAR* RetargeterPath);

	/** Wears /Game/Shinobi/Characters/<Name>/SK_<Name> with RTG_MannyTo<Name>. */
	bool WearCharacter(const FString& Name);

	/** Hides the driver (the mesh this is attached to) and keeps it animating unseen. */
	void HideDriver();

	/** The retargeter of the body worn; UShinobiRetargetAnimInstance reads it when it (re)initialises. */
	UIKRetargeter* GetRetargeter() const { return Retargeter; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UIKRetargeter> Retargeter;
};
