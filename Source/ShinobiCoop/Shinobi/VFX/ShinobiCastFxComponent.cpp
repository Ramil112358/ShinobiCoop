#include "Shinobi/VFX/ShinobiCastFxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "AnimationRuntime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr uint8 PartBit(EShinobiCastPart Part) { return 1 << static_cast<uint8>(Part); }

	struct FCastLook
	{
		FGameplayTag Tag;
		uint8 Parts;
		FLinearColor OrbColor;
		float OrbScale; // uncharged; the engine sphere is 100 units across
	};

	/** Built on first use, after the native tags are registered. */
	const TArray<FCastLook>& CastLooks()
	{
		static const TArray<FCastLook> Table = {
			{ ShinobiTags::State_Casting_Fireball, PartBit(EShinobiCastPart::MouthFlame), ShinobiVfx::Colors::Fire, 0.f },
			{ ShinobiTags::State_Casting_Chidori, PartBit(EShinobiCastPart::HandOrb) | PartBit(EShinobiCastPart::LightningArcs), ShinobiVfx::Colors::Lightning, 0.2f },
			{ ShinobiTags::State_Casting_Rasengan, PartBit(EShinobiCastPart::HandOrb) | PartBit(EShinobiCastPart::RasenganShells), ShinobiVfx::Colors::Chakra, 0.25f },
			{ ShinobiTags::State_Casting_Amaterasu, PartBit(EShinobiCastPart::EyeGlow) | PartBit(EShinobiCastPart::BloodStreaks), ShinobiVfx::Colors::BlackFlame, 0.f },
		};
		return Table;
	}

	const FLinearColor EyeRed(1.f, 0.02f, 0.02f);
	constexpr float OrbIntensity = 6.f;
	constexpr float OrbLightIntensity = 800.f;
	constexpr int32 NumArcs = 8;
	constexpr int32 SegmentsPerArc = 3;
	constexpr float ArcShuffleInterval = 0.05f;
	constexpr float StreakGrowTime = 0.4f; // blood runs down over the seals

	// Face offsets from the head bone, in the actor's frame (forward, right, up), per body. The eye offset is the
	// left eye; the right mirrors it.
	// ponytail: hand-tuned against the -ShinobiJutsuShot screenshots; retune if the hero mesh changes
	struct FFaceOffsets
	{
		FVector Mouth;
		FVector Eye;
	};
	const FFaceOffsets MannyFace = { FVector(14.f, 0.f, 2.f), FVector(10.f, -3.5f, 7.f) };
	const FFaceOffsets HeroFace = { FVector(12.f, 0.f, 4.f), FVector(12.f, -3.f, 10.5f) }; // eyes in front of the glasses
}

UShinobiCastFxComponent::UShinobiCastFxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while a cast is shown
}

FGameplayTag UShinobiCastFxComponent::GetShownTag() const
{
	return ShownIndex == INDEX_NONE ? FGameplayTag() : CastLooks()[ShownIndex].Tag;
}

bool UShinobiCastFxComponent::IsPartVisible(EShinobiCastPart Part) const
{
	const UStaticMeshComponent* Piece = nullptr;
	switch (Part)
	{
	case EShinobiCastPart::HandOrb:        Piece = Orb; break;
	case EShinobiCastPart::RasenganShells: Piece = Shells.Num() ? Shells[0] : nullptr; break;
	case EShinobiCastPart::LightningArcs:  Piece = ArcSegments.Num() ? ArcSegments[0] : nullptr; break;
	case EShinobiCastPart::MouthFlame:     Piece = MouthFlame; break;
	case EShinobiCastPart::EyeGlow:        Piece = Eyes.Num() ? Eyes[0] : nullptr; break;
	case EShinobiCastPart::BloodStreaks:   Piece = Streaks.Num() ? Streaks[0] : nullptr; break;
	}
	return Piece && Piece->IsVisible();
}

float UShinobiCastFxComponent::GetChargeVisualScale() const
{
	return 1.f + 0.8f * FMath::Min(ChargeAge / UShinobiJutsuAbility::MaxChargeTime, 1.f);
}

UStaticMeshComponent* UShinobiCastFxComponent::MakeMesh(const TCHAR* Name, const TCHAR* MeshPath, UMaterialInstanceDynamic* Material, USceneComponent* Parent, FName Socket)
{
	AActor* Owner = GetOwner();
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), Name));
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetMaterial(0, Material);
	Mesh->SetVisibility(false);
	Mesh->SetupAttachment(Parent, Socket);
	Mesh->RegisterComponent();
	return Mesh;
}

void UShinobiCastFxComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	static const TCHAR* SphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	const AShinobiCharacterBase* OwnerCharacter = Cast<AShinobiCharacterBase>(GetOwner());
	USkeletalMeshComponent* Body = OwnerCharacter ? OwnerCharacter->GetBodyMesh() : nullptr;
	OrbMaterial = ShinobiVfx::MakeGlow(this, ShinobiVfx::Colors::Chakra, OrbIntensity, 1.f);
	// A bone name works as a socket.
	Orb = MakeMesh(TEXT("CastOrb"), SphereMesh, OrbMaterial, Body ? static_cast<USceneComponent*>(Body) : this, Body ? OwnerCharacter->GetRightHandBone() : NAME_None);
	// World-space size: a hand bone can carry a scale of its own (a Mixamo rig exported from Blender has 100x bones).
	Orb->SetUsingAbsoluteScale(true);

	Light = NewObject<UPointLightComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UPointLightComponent::StaticClass(), TEXT("CastLight")));
	Light->SetupAttachment(Orb);
	Light->SetAttenuationRadius(250.f);
	Light->SetCastShadows(false);
	Light->SetSpecularScale(0.f);
	Light->SetVisibility(false);
	Light->RegisterComponent();

	// Rasengan: two rim-lit shells around the orb, spun on different axes in Tick.
	for (const float ShellScale : { 1.25f, 1.45f })
	{
		UStaticMeshComponent* Shell = MakeMesh(TEXT("CastShell"), SphereMesh, ShinobiVfx::MakeGlow(this, ShinobiVfx::Colors::Chakra, 3.f, 0.15f), Orb);
		Shell->SetRelativeScale3D(FVector(ShellScale));
		Shells.Add(Shell);
	}

	// Everything below is placed in world space every tick, so it ignores its parent's (bone) scale and rotation.
	auto Free = [](UStaticMeshComponent* Mesh)
	{
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		return Mesh;
	};
	UMaterialInstanceDynamic* ArcMaterial = ShinobiVfx::MakeGlow(this, ShinobiVfx::Colors::Lightning, 12.f, 1.f);
	for (int32 Index = 0; Index < NumArcs * SegmentsPerArc; ++Index)
	{
		ArcSegments.Add(Free(MakeMesh(TEXT("CastArc"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), ArcMaterial, this)));
	}

	MouthFlame = Free(MakeMesh(TEXT("CastMouthFlame"), SphereMesh, ShinobiVfx::MakeFlame(this, ShinobiVfx::Colors::Fire, 4.f), this));
	MouthLight = NewObject<UPointLightComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UPointLightComponent::StaticClass(), TEXT("CastMouthLight")));
	MouthLight->SetupAttachment(MouthFlame);
	MouthLight->SetLightColor(ShinobiVfx::Colors::Fire);
	MouthLight->SetAttenuationRadius(150.f);
	MouthLight->SetCastShadows(false);
	MouthLight->SetSpecularScale(0.f);
	MouthLight->SetVisibility(false);
	MouthLight->RegisterComponent();

	UMaterialInstanceDynamic* EyeMaterial = ShinobiVfx::MakeGlow(this, EyeRed, 8.f, 1.f);
	UMaterialInstanceDynamic* BloodMaterial = ShinobiVfx::MakeStreak(this, ShinobiVfx::Colors::Blood);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Eyes.Add(Free(MakeMesh(TEXT("CastEye"), SphereMesh, EyeMaterial, this)));
		Streaks.Add(Free(MakeMesh(TEXT("CastStreak"), TEXT("/Engine/BasicShapes/Cube.Cube"), BloodMaterial, this)));
	}

	if (const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		AbilitySystem = Owner->GetAbilitySystemComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		for (const FCastLook& Look : CastLooks())
		{
			TagEventHandles.Add(ASC->RegisterGameplayTagEvent(Look.Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UShinobiCastFxComponent::OnCastingTagChanged));
		}
		ChargingHandle = ASC->RegisterGameplayTagEvent(ShinobiTags::State_Casting_Charging, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UShinobiCastFxComponent::OnChargingTagChanged);
	}
	Refresh();
}

void UShinobiCastFxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		for (int32 Index = 0; Index < TagEventHandles.Num(); ++Index)
		{
			ASC->UnregisterGameplayTagEvent(TagEventHandles[Index], CastLooks()[Index].Tag, EGameplayTagEventType::NewOrRemoved);
		}
		ASC->UnregisterGameplayTagEvent(ChargingHandle, ShinobiTags::State_Casting_Charging, EGameplayTagEventType::NewOrRemoved);
	}
	TagEventHandles.Reset();
	auto Destroy = [](USceneComponent* Piece)
	{
		if (Piece)
		{
			Piece->DestroyComponent();
		}
	};
	Destroy(Light);
	Destroy(MouthLight);
	for (const TArray<TObjectPtr<UStaticMeshComponent>>* Group : { &Shells, &ArcSegments, &Eyes, &Streaks })
	{
		for (UStaticMeshComponent* Piece : *Group)
		{
			Destroy(Piece);
		}
	}
	Destroy(Orb); // after the shells and the light attached to it
	Destroy(MouthFlame);
	Light = MouthLight = nullptr;
	Orb = MouthFlame = nullptr;
	Shells.Reset();
	ArcSegments.Reset();
	Eyes.Reset();
	Streaks.Reset();
	OrbMaterial = nullptr;
	ShownIndex = INDEX_NONE;
	Super::EndPlay(EndPlayReason);
}

void UShinobiCastFxComponent::OnCastingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	Refresh();
}

void UShinobiCastFxComponent::OnChargingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bCharging = NewCount > 0;
	if (bCharging)
	{
		ChargeAge = 0.f;
	}
	Refresh();
}

void UShinobiCastFxComponent::Refresh()
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	const int32 Previous = ShownIndex;
	ShownIndex = INDEX_NONE;
	for (int32 Index = 0; ASC && Index < CastLooks().Num(); ++Index)
	{
		if (ASC->HasMatchingGameplayTag(CastLooks()[Index].Tag))
		{
			ShownIndex = Index;
			break;
		}
	}
	if (ShownIndex != Previous)
	{
		Age = 0.f;
		NextArcShuffle = 0.f;
	}
	if (ShownIndex == INDEX_NONE)
	{
		ChargeAge = 0.f;
	}
	if (!Orb)
	{
		return; // dedicated server
	}

	const uint8 Parts = ShownIndex == INDEX_NONE ? 0 : CastLooks()[ShownIndex].Parts;
	auto Has = [Parts](EShinobiCastPart Part) { return (Parts & PartBit(Part)) != 0; };
	Orb->SetVisibility(Has(EShinobiCastPart::HandOrb));
	Light->SetVisibility(Has(EShinobiCastPart::HandOrb));
	for (UStaticMeshComponent* Shell : Shells)
	{
		Shell->SetVisibility(Has(EShinobiCastPart::RasenganShells));
	}
	for (UStaticMeshComponent* Segment : ArcSegments)
	{
		Segment->SetVisibility(Has(EShinobiCastPart::LightningArcs));
	}
	const bool bMouth = Has(EShinobiCastPart::MouthFlame) && bCharging; // the breath burst takes over at the release
	MouthFlame->SetVisibility(bMouth);
	MouthLight->SetVisibility(bMouth);
	for (UStaticMeshComponent* Eye : Eyes)
	{
		Eye->SetVisibility(Has(EShinobiCastPart::EyeGlow));
	}
	for (UStaticMeshComponent* Streak : Streaks)
	{
		Streak->SetVisibility(Has(EShinobiCastPart::BloodStreaks));
	}
	if (Has(EShinobiCastPart::HandOrb))
	{
		const FLinearColor& Color = CastLooks()[ShownIndex].OrbColor;
		if (OrbMaterial)
		{
			OrbMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		}
		Light->SetLightColor(Color);
	}
	SetComponentTickEnabled(Parts != 0);
	if (Parts != 0)
	{
		TickComponent(0.f, LEVELTICK_All, nullptr); // place everything now, not a frame late
	}
}

void UShinobiCastFxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (ThisTickFunction)
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}
	if (ShownIndex == INDEX_NONE || !Orb)
	{
		return;
	}
	Age += DeltaTime;
	if (bCharging)
	{
		ChargeAge += DeltaTime;
	}
	const float ChargeScale = GetChargeVisualScale();
	const FCastLook& Look = CastLooks()[ShownIndex];

	if (Orb->IsVisible())
	{
		// Lightning crackles (random size and brightness every frame); the Rasengan pulses gently.
		const bool bLightning = (Look.Parts & PartBit(EShinobiCastPart::LightningArcs)) != 0;
		const float Pulse = bLightning ? FMath::FRandRange(0.85f, 1.2f) : 1.f + 0.08f * FMath::Sin(Age * 14.f);
		Orb->SetRelativeScale3D(FVector(Look.OrbScale * ChargeScale * Pulse));
		if (OrbMaterial)
		{
			OrbMaterial->SetScalarParameterValue(TEXT("Intensity"), OrbIntensity * (bLightning ? 1.f : 1.5f) * Pulse);
		}
		Light->SetIntensity(OrbLightIntensity * ChargeScale * (bLightning ? FMath::FRandRange(0.6f, 1.5f) : Pulse));
	}
	if (Shells.Num() == 2 && Shells[0]->IsVisible())
	{
		Shells[0]->SetRelativeRotation(FRotator(Age * 720.f, Age * 360.f, 0.f));
		Shells[1]->SetRelativeRotation(FRotator(-Age * 540.f, 0.f, Age * 720.f));
	}
	if (ArcSegments.Num() && ArcSegments[0]->IsVisible())
	{
		UpdateArcs(ChargeScale);
	}
	UpdateHead(ChargeScale);
}

void UShinobiCastFxComponent::UpdateArcs(float ChargeScale)
{
	// Re-randomised every ArcShuffleInterval, but always drawn from the orb's current position so they keep up with
	// the hand during the dash. Each arc is a chain of short, bent cylinder segments.
	if (Age < NextArcShuffle)
	{
		return;
	}
	NextArcShuffle = Age + ArcShuffleInterval;
	const FVector Origin = Orb->GetComponentLocation();
	for (int32 Arc = 0; Arc < NumArcs; ++Arc)
	{
		FVector Direction = FMath::VRand();
		FVector Start = Origin;
		const float SegmentLength = FMath::FRandRange(35.f, 60.f) * ChargeScale / SegmentsPerArc;
		for (int32 Segment = 0; Segment < SegmentsPerArc; ++Segment)
		{
			Direction = (Direction + FMath::VRand() * 0.6f).GetSafeNormal();
			const FVector End = Start + Direction * SegmentLength;
			UStaticMeshComponent* Piece = ArcSegments[Arc * SegmentsPerArc + Segment];
			// The engine cylinder is 100 units tall along Z, around its centre.
			Piece->SetWorldLocationAndRotation((Start + End) * 0.5f, FRotationMatrix::MakeFromZ(Direction).Rotator());
			Piece->SetWorldScale3D(FVector(0.025f, 0.025f, SegmentLength / 100.f));
			Start = End;
		}
	}
}

void UShinobiCastFxComponent::UpdateHead(float ChargeScale)
{
	const bool bMouth = MouthFlame->IsVisible();
	const bool bEyes = Eyes[0]->IsVisible() || Streaks[0]->IsVisible();
	const AShinobiCharacterBase* OwnerCharacter = Cast<AShinobiCharacterBase>(GetOwner());
	if ((!bMouth && !bEyes) || !OwnerCharacter)
	{
		return;
	}
	const USkeletalMeshComponent* Body = OwnerCharacter->GetBodyMesh();
	const FFaceOffsets& Face = Body == OwnerCharacter->GetMesh() ? MannyFace : HeroFace;
	const FName HeadBone = OwnerCharacter->GetHeadBone();
	const FVector Head = Body->GetSocketLocation(HeadBone);
	// The offsets are measured with the head upright and facing the actor's way; turn them with the head, by how far
	// the head bone has rotated from its reference pose, so the eyes stay on the face when it bows or turns.
	FQuat HeadTurn = FQuat::Identity;
	const int32 HeadIndex = Body->GetBoneIndex(HeadBone);
	if (HeadIndex != INDEX_NONE && Body->GetSkinnedAsset())
	{
		const FQuat Reference = FAnimationRuntime::GetComponentSpaceTransformRefPose(Body->GetSkinnedAsset()->GetRefSkeleton(), HeadIndex).GetRotation();
		const FQuat Current = Body->GetSocketTransform(HeadBone, RTS_Component).GetRotation();
		const FQuat ToWorld = Body->GetComponentQuat();
		HeadTurn = ToWorld * (Current * Reference.Inverse()) * ToWorld.Inverse();
	}
	const FQuat FacingQuat = HeadTurn * OwnerCharacter->GetActorQuat();
	const FRotator Facing = FacingQuat.Rotator();
	auto At = [&](const FVector& Offset) { return Head + FacingQuat.RotateVector(Offset); };

	if (bMouth)
	{
		const float Flicker = FMath::FRandRange(0.85f, 1.15f);
		MouthFlame->SetWorldLocationAndRotation(At(Face.Mouth), Facing);
		MouthFlame->SetWorldScale3D(FVector(0.12f, 0.12f, 0.16f) * ChargeScale * Flicker);
		MouthLight->SetIntensity(600.f * ChargeScale * Flicker);
	}
	if (bEyes)
	{
		const float Grown = FMath::Clamp(Age / StreakGrowTime, 0.f, 1.f);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FVector Eye = At(FVector(Face.Eye.X, Side == 0 ? Face.Eye.Y : -Face.Eye.Y, Face.Eye.Z));
			const FVector Down = -FacingQuat.GetUpVector();
			Eyes[Side]->SetWorldLocation(Eye);
			Eyes[Side]->SetWorldScale3D(FVector(0.03f));
			// The engine cube is 100 units around its centre: hang it from just under the eye as it grows.
			const float Length = FMath::Max(9.f * Grown, 0.1f);
			Streaks[Side]->SetWorldLocationAndRotation(Eye + Down * (1.5f + Length * 0.5f), Facing);
			Streaks[Side]->SetWorldScale3D(FVector(0.012f, 0.012f, Length / 100.f));
		}
	}
}
