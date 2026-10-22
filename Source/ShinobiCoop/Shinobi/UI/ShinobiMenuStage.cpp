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
	Camera->SetRelativeLocation(FVector(560.f, 0.f, 115.f));
	Camera->SetRelativeRotation(FRotator(-3.f, 180.f, 0.f));
	Camera->SetFieldOfView(45.f);
}

void AShinobiMenuStage::BeginPlay()
{
	Super::BeginPlay();
	if (ShownBody == INDEX_NONE)
	{
		Show(0, 0);
	}
}

void AShinobiMenuStage::Show(int32 RosterIndex, int32 OutfitIndex)
{
	if (!Driver->GetSkeletalMeshAsset())
	{
		Driver->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, MenuStageManny));
		Driver->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Driver->PlayAnimation(LoadObject<UAnimSequence>(nullptr, MenuStageIdle), true);
	}
	ShownBody = INDEX_NONE;
	for (const int32 Index : { RosterIndex, 0 }) // the pick, else the hero
	{
		if (Index >= 0 && Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster)
			&& Body->Wear(AShinobiPlayerCharacter::Roster[Index].Mesh, AShinobiPlayerCharacter::Roster[Index].Retargeter))
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
	AShinobiPlayerCharacter::DressOutfit(*Body, *Outfit, OutfitIndex, ShownBody);

}

void AShinobiMenuStage::FramePortrait()
{
	TargetOffset = 0.f;
	Driver->SetRelativeLocation(FVector::ZeroVector);
	// The head bone's height (bodies differ), head and shoulders around it.
	float HeadZ = 165.f;
	for (int32 Bone = 0; Bone < Body->GetNumBones(); ++Bone)
	{
		if (Body->GetBoneName(Bone).ToString().EndsWith(TEXT("Head"), ESearchCase::IgnoreCase))
		{
			HeadZ = GetActorTransform().InverseTransformPosition(Body->GetBoneLocation(Body->GetBoneName(Bone))).Z;
			break;
		}
	}
	// The head bone sits at the skull's base: framed from just above the hair to the chest (tuned on the roster).
	Camera->SetFieldOfView(24.f);
	Camera->SetRelativeLocation(FVector(300.f, 0.f, HeadZ - 2.f));
	Camera->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
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
}
