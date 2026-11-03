#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiStudioSet.h"

namespace
{
	const TCHAR* const MenuStageManny = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	const TCHAR* const MenuStageIdle = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");

	/** FramePortrait's lens: 3 m from the face, 24 degrees across a 16:9 frame (the camera keeps that height of view
	 * whatever the window's shape). */
	constexpr float MenuStagePortraitDistance = 300.f;
	constexpr float MenuStagePortraitFOV = 24.f;
	/** Seconds a requested look stands ready before its picture is taken; the last MenuStagePortraitWarmup of them are
	 * filmed every frame so exposure, temporal AA, streamed textures and grooms settle in the capture too. */
	constexpr float MenuStagePortraitSettle = 1.f;
	constexpr float MenuStagePortraitWarmup = 0.5f;
	/** Seconds a requested look may be neither worn nor building before the request gives up (it was never asked for, or
	 * the creator catalog isn't there). */
	constexpr float MenuStagePortraitGiveUp = 2.f;
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

	// Made characters' portraits (RequestPortrait): off screen, only when asked.
	PortraitCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortraitCapture"));
	PortraitCapture->SetupAttachment(Root);
	PortraitCapture->bCaptureEveryFrame = false;
	PortraitCapture->bCaptureOnMovement = false;
	PortraitCapture->bAlwaysPersistRenderingState = true; // (exposure and temporal AA carry over the warm-up frames)
	PortraitCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR; // tone-mapped, as the screen shows it
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
	if (bPortraitPending && !(PortraitLook == Look))
	{
		CancelPortrait(); // (someone else stands here now)
	}
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
	CancelPortrait(); // (a roster body: no made character to film)
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

void AShinobiMenuStage::RequestPortrait(const FShinobiAppearance& Look, TFunction<void(TArray<uint8>&&)> Done)
{
	if (!(bPortraitPending && PortraitLook == Look))
	{
		PortraitReadyTime = PortraitStuckTime = 0.f; // (the same look asked again keeps waiting where it got to)
	}
	bPortraitPending = true;
	PortraitLook = Look;
	PortraitDone = MoveTemp(Done);
}

void AShinobiMenuStage::CancelPortrait()
{
	bPortraitPending = false;
	PortraitDone = nullptr;
	PortraitReadyTime = PortraitStuckTime = 0.f;
}

void AShinobiMenuStage::PlacePortraitCapture()
{
	const FVector Up = GetActorUpVector();
	const FVector Forward = Driver->GetRightVector(); // the character faces the driver's +Y (the actor's +X, unturned)
	// The face: a little above the head bone, which sits at the skull's base (as -ShinobiPortraitShot aims).
	static const FName HeadBone(TEXT("head"));
	const FVector Head = Body->GetBoneIndex(HeadBone) != INDEX_NONE ? Body->GetBoneLocation(HeadBone)
		: GetActorTransform().TransformPosition(FVector(0.f, Driver->GetRelativeLocation().Y, HeadHeight()));
	const FVector Face = Head + Up * 8.f;
	// FramePortrait's height of view in a 3:4 picture, the face MadePortraitFaceV down from its top.
	const float HalfHeight = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(MenuStagePortraitFOV * 0.5f)) / FMath::Max(Camera->AspectRatio, 0.1f));
	const float HalfWidth = FMath::Atan(FMath::Tan(HalfHeight) * float(PortraitWidth) / float(PortraitHeight));
	const float FrameHeight = 2.f * MenuStagePortraitDistance * FMath::Tan(HalfHeight);
	const FVector Eye = Face + Forward * MenuStagePortraitDistance - Up * (0.5f - ShinobiMenuArt::MadePortraitFaceV) * FrameHeight;
	PortraitCapture->SetWorldLocationAndRotation(Eye, (-Forward).Rotation());
	PortraitCapture->FOVAngle = FMath::RadiansToDegrees(2.f * HalfWidth);
}

TArray<uint8> AShinobiMenuStage::ReadPortrait() const
{
	FTextureRenderTargetResource* Resource = PortraitTarget ? PortraitTarget->GameThread_GetRenderTargetResource() : nullptr;
	TArray<FColor> Pixels;
	if (!Resource || !Resource->ReadPixels(Pixels) || Pixels.Num() != PortraitWidth * PortraitHeight)
	{
		return {};
	}
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255; // (the capture's alpha isn't opacity)
	}
	TArray64<uint8> Png;
	if (!FImageUtils::CompressImage(Png, TEXT("png"), FImageView(Pixels.GetData(), PortraitWidth, PortraitHeight)))
	{
		return {};
	}
	return TArray<uint8>(Png.GetData(), int32(Png.Num()));
}

void AShinobiMenuStage::TickPortrait(float DeltaSeconds)
{
	if (!bPortraitPending)
	{
		return;
	}
	// Ready: the look is on (built, no live edit since) and the character has turned to face front.
	const bool bBuilding = Body->IsMutableBuildPending();
	const bool bWorn = !bBuilding && Body->GetAppearance() == PortraitLook && Body->GetSkeletalMeshAsset();
	const bool bReady = bWorn && FMath::Abs(Yaw - TargetYaw) < 1.f;
	PortraitReadyTime = bReady ? PortraitReadyTime + DeltaSeconds : 0.f;
	PortraitStuckTime = bWorn || bBuilding ? 0.f : PortraitStuckTime + DeltaSeconds;
	if (PortraitStuckTime >= MenuStagePortraitGiveUp)
	{
		TFunction<void(TArray<uint8>&&)> Done = MoveTemp(PortraitDone);
		CancelPortrait();
		if (Done)
		{
			Done(TArray<uint8>()); // (no picture: the look isn't coming)
		}
		return;
	}
	if (PortraitReadyTime < MenuStagePortraitSettle - MenuStagePortraitWarmup)
	{
		return;
	}
	if (!PortraitTarget)
	{
		PortraitTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("PortraitTarget"));
		PortraitTarget->RenderTargetFormat = RTF_RGBA8;
		PortraitTarget->ClearColor = FLinearColor::Black;
		PortraitTarget->InitAutoFormat(PortraitWidth, PortraitHeight);
		PortraitTarget->UpdateResourceImmediate(true);
		PortraitCapture->TextureTarget = PortraitTarget;
	}
	PlacePortraitCapture();
	PortraitCapture->HideComponent(Outfit); // (the roster's portraits wear no outfit either)
	PortraitCapture->CaptureScene();
	if (PortraitReadyTime < MenuStagePortraitSettle)
	{
		return; // warming up
	}
	TArray<uint8> Png = ReadPortrait();
	TFunction<void(TArray<uint8>&&)> Done = MoveTemp(PortraitDone);
	CancelPortrait();
	if (Done)
	{
		Done(MoveTemp(Png)); // (empty when it couldn't be read)
	}
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
	TickPortrait(DeltaSeconds);
}
