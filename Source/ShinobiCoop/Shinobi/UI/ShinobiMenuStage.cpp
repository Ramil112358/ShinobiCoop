#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

namespace
{
	const TCHAR* const MenuStageManny = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	const TCHAR* const MenuStageIdle = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");
}

AShinobiMenuStage::AShinobiMenuStage()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Faces the actor's +X, like a character's mesh; the camera looks back at it from +X.
	Driver = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Driver"));
	Driver->SetupAttachment(Root);
	Driver->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Driver->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Body = CreateDefaultSubobject<UShinobiBodyComponent>(TEXT("Body"));
	Body->SetupAttachment(Driver);
	// The mesh's own bounds: physics-asset bounds can be a few cm (a cloth-only physics asset), which the cloak in front
	// occludes, culling the whole body.
	Body->bComponentUseFixedSkelBounds = true;

	Outfit = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Outfit"));
	Outfit->SetupAttachment(Body);
	Outfit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Outfit->SetVisibility(false);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	SetCloseUp(false);
}

void AShinobiMenuStage::BeginPlay()
{
	Super::BeginPlay();
	if (ShownBody == INDEX_NONE)
	{
		Show(0, 0);
	}
}

void AShinobiMenuStage::StartDriver()
{
	if (!Driver->GetSkeletalMeshAsset())
	{
		Driver->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, MenuStageManny));
		Driver->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Driver->PlayAnimation(LoadObject<UAnimSequence>(nullptr, MenuStageIdle), true);
	}
}

void AShinobiMenuStage::ShowAppearance(const FShinobiAppearance& Look, int32 OutfitIndex)
{
	StartDriver();
	if (!Body->WearAppearance(Look))
	{
		Show(0, OutfitIndex);
		return;
	}
	ShownBody = 0; // (no roster body: a portrait of it would be the hero's)
	Body->HideDriver();
	AShinobiPlayerCharacter::DressOutfit(*Body, *Outfit, OutfitIndex, Body->GetFittingName());
}

void AShinobiMenuStage::Show(int32 RosterIndex, int32 OutfitIndex)
{
	StartDriver();
	ShownBody = INDEX_NONE;
	for (const int32 Index : { RosterIndex, 0 }) // the pick, else the hero
	{
		if (Index >= 0 && Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster)
			&& Body->Wear(AShinobiPlayerCharacter::Roster[Index].Mesh, AShinobiPlayerCharacter::Roster[Index].Retargeter, AShinobiPlayerCharacter::Roster[Index].MetaHuman)) // a MetaHuman's face and hair too
		{
			ShownBody = Index;
			break;
		}
	}
	if (ShownBody == INDEX_NONE)
	{
		ShownBody = 0; // nothing imported: the Manny stands in
		Driver->SetVisibility(true);
		return;
	}
	Body->HideDriver();
	AShinobiPlayerCharacter::DressOutfit(*Body, *Outfit, OutfitIndex, AShinobiPlayerCharacter::Roster[ShownBody].Name);

}

float AShinobiMenuStage::HeadHeight() const
{
	// The head bone's height (bodies differ).
	for (int32 Bone = 0; Bone < Body->GetNumBones(); ++Bone)
	{
		if (Body->GetBoneName(Bone).ToString().EndsWith(TEXT("Head"), ESearchCase::IgnoreCase))
		{
			return GetActorTransform().InverseTransformPosition(Body->GetBoneLocation(Body->GetBoneName(Bone))).Z;
		}
	}
	return 165.f;
}

void AShinobiMenuStage::FramePortrait()
{
	TargetOffset = 0.f;
	bCloseUp = false;
	Driver->SetRelativeLocation(FVector::ZeroVector);
	// The head bone sits at the skull's base: framed from just above the hair to the chest (tuned on the roster).
	Camera->SetFieldOfView(24.f);
	Camera->SetRelativeLocation(FVector(300.f, 0.f, HeadHeight() - 2.f));
	Camera->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
}

void AShinobiMenuStage::SetCloseUp(bool bHead)
{
	bCloseUp = bHead;
	Camera->SetRelativeRotation(FRotator(bHead ? 0.f : -3.f, 180.f, 0.f));
	Camera->SetFieldOfView(bHead ? 22.f : 45.f);
	if (!bHead)
	{
		Camera->SetRelativeLocation(FVector(560.f, 0.f, 115.f)); // (Tick keeps a close-up on the head)
	}
}

void AShinobiMenuStage::SetFocus(EShinobiMenuStageFocus Focus)
{
	// The camera looks along -X, so screen-left is +Y.
	TargetOffset = Focus == EShinobiMenuStageFocus::Left ? FocusOffsetCm : Focus == EShinobiMenuStageFocus::Right ? -FocusOffsetCm : 0.f;
}

void AShinobiMenuStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector Location = Driver->GetRelativeLocation();
	if (!FMath::IsNearlyEqual(Location.Y, TargetOffset, 0.1f))
	{
		Location.Y = FMath::FInterpTo(Location.Y, TargetOffset, DeltaSeconds, 6.f);
		Driver->SetRelativeLocation(Location);
	}
	if (bCloseUp)
	{
		// Straight at the face, which then sits mid-screen, clear of a side panel.
		Camera->SetRelativeLocation(FVector(330.f, Location.Y, HeadHeight() + 2.f));
	}
}
