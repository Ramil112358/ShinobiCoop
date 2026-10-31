#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiStudioSet.h"

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
	Camera->bOverrideAspectRatioAxisConstraint = true; // (a wider window shows more at the sides, the same height)
	Camera->AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
	PlaceCamera();
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
	ShownLook = Look;
	// The made body lands later (Mutable builds it): dress the outfit on its skeleton then.
	Body->OnBodyBuilt.RemoveAll(this);
	Body->OnBodyBuilt.AddWeakLambda(this, [this, OutfitIndex]()
	{
		AShinobiPlayerCharacter::DressOutfit(*Body, *Outfit, OutfitIndex, Body->GetFittingName());
	});
	if (!Body->WearAppearance(Look))
	{
		Body->OnBodyBuilt.RemoveAll(this);
		Show(0, OutfitIndex);
		return;
	}
	ShownBody = 0; // (no roster body: a portrait of it would be the hero's)
	Body->HideDriver();
}

void AShinobiMenuStage::Show(int32 RosterIndex, int32 OutfitIndex, int32 Face)
{
	StartDriver();
	Body->OnBodyBuilt.RemoveAll(this);
	ShownBody = INDEX_NONE;
	for (const int32 Index : { RosterIndex, 0 }) // the pick, else the hero
	{
		if (Index >= 0 && Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster)
			&& Body->Wear(AShinobiPlayerCharacter::Roster[Index].Mesh, AShinobiPlayerCharacter::Roster[Index].Retargeter,
				AShinobiPlayerCharacter::FaceMetaHuman(Index, Index == RosterIndex ? Face : 0))) // a MetaHuman's face and hair too
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
	// A roster preset replaces that body once built; until then (or if it can't be) the body above stands in.
	if (const TOptional<FShinobiAppearance> Preset = AShinobiPlayerCharacter::RosterPreset(RosterIndex, Face))
	{
		Body->OnBodyBuilt.AddWeakLambda(this, [this, OutfitIndex]() { AShinobiPlayerCharacter::DressOutfit(*Body, *Outfit, OutfitIndex, Body->GetFittingName()); });
		if (Body->WearAppearance(*Preset))
		{
			ShownBody = RosterIndex;
		}
		else
		{
			Body->OnBodyBuilt.RemoveAll(this);
		}
	}

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
	Yaw = TargetYaw = Zoom = TargetZoom = AutoZoom = 0.f;
	bPortrait = true; // (until the view is moved)
	Driver->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(0.f, -90.f, 0.f));
	// The head bone sits at the skull's base: framed from just above the hair to the chest (tuned on the roster).
	Camera->SetFieldOfView(24.f);
	Camera->SetRelativeLocation(FVector(300.f, 0.f, HeadHeight() - 2.f));
	Camera->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
}

AShinobiStudioSet* AShinobiMenuStage::GetStudio() const
{
	TActorIterator<AShinobiStudioSet> Studio(GetWorld());
	return Studio ? *Studio : nullptr;
}

void AShinobiMenuStage::SetStudio(bool bStudio)
{
	if (bStudio == bInStudio)
	{
		return;
	}
	if (!bStudio)
	{
		SetActorTransform(Home, false, nullptr, ETeleportType::ResetPhysics);
		bInStudio = false;
	}
	else if (const AShinobiStudioSet* Studio = GetStudio())
	{
		Home = GetActorTransform();
		SetActorTransform(Studio->GetActorTransform(), false, nullptr, ETeleportType::ResetPhysics);
		bInStudio = true;
	}
}

void AShinobiMenuStage::AddYaw(float Degrees)
{
	bPortrait = false;
	TargetYaw += Degrees;
}

void AShinobiMenuStage::AddZoom(float Delta)
{
	bPortrait = false;
	TargetZoom = FMath::Clamp(TargetZoom + Delta, 0.f, 1.f);
}

void AShinobiMenuStage::SetAutoZoom(float NewZoom)
{
	bPortrait = false;
	AutoZoom = TargetZoom = FMath::Clamp(NewZoom, 0.f, 1.f);
}

void AShinobiMenuStage::ResetView()
{
	Yaw = FRotator::NormalizeAxis(Yaw); // the short way round to the front
	TargetYaw = 0.f;
	TargetZoom = AutoZoom;
}

void AShinobiMenuStage::PlaceCamera()
{
	// The full view (chest height, slightly down) to straight at the face, which then sits mid-screen, clear of a side panel.
	const FVector Face(330.f, Driver->GetRelativeLocation().Y, Zoom > 0.f ? HeadHeight() + 2.f : 115.f);
	Camera->SetRelativeLocation(FMath::Lerp(FVector(560.f, 0.f, 115.f), Face, Zoom));
	Camera->SetRelativeRotation(FRotator(FMath::Lerp(-3.f, 0.f, Zoom), 180.f, 0.f));
	Camera->SetFieldOfView(FMath::Lerp(45.f, 22.f, Zoom));
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
	Yaw = FMath::FInterpTo(Yaw, TargetYaw, DeltaSeconds, 8.f);
	Zoom = FMath::FInterpTo(Zoom, TargetZoom, DeltaSeconds, 6.f);
	if (!bPortrait)
	{
		Driver->SetRelativeRotation(FRotator(0.f, -90.f + Yaw, 0.f));
		PlaceCamera();
	}
}
