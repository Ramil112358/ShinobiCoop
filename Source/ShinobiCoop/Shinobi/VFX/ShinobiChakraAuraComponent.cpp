#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	struct FAuraLook
	{
		FGameplayTag Tag;
		FLinearColor Color;
		float Intensity;
		bool bFlicker;
	};

	const FLinearColor ChargeColor(0.15f, 0.7f, 1.f); // the Storm games' cyan chakra

	/** Highest priority first: the first tag the owner has is the one shown. Built on first use, after the
	 * native tags are registered (a file-scope table would copy them before they have values). */
	const TArray<FAuraLook>& Looks()
	{
		static const TArray<FAuraLook> Table = {
			{ ShinobiTags::State_Burning_Black, ShinobiVfx::Colors::BlackFlame, 1.5f, false }, // a faint rim: the flame tongues (UShinobiBodyFlameComponent) carry the look
			{ ShinobiTags::State_Burning_Normal, ShinobiVfx::Colors::Fire, 2.5f, false },
			{ ShinobiTags::State_Shocked, ShinobiVfx::Colors::Lightning, 4.f, true },
			{ ShinobiTags::State_ChakraCharging, ChargeColor, 1.4f, false }, // a rim: the column and flames carry the look
			{ ShinobiTags::State_Chilled, ShinobiVfx::Colors::Ice, 2.f, false },
		};
		return Table;
	}

	constexpr float BodyInflate = 2.5f;

	// The charge column (M_ChakraPillar on the engine cylinder, 100 units across and tall, pivot at its centre).
	constexpr float PillarRadius = 55.f;
	constexpr float PillarTop = 70.f; // above the capsule top

	// Flame tongues: camera-facing wisps rising just behind the body, off to the sides of the column (never behind
	// the head: hair cards don't hide what is behind them, so a flame there looked to come out of the head). The engine
	// plane is 100 units across; X is the card's width, Y its height; a wisp cell is twice as tall as wide.
	constexpr int32 NumFlames = 20;
	const FLinearColor FlameTint(0.5f, 1.1f, 1.2f); // pushes the blue sprites toward the charge's cyan
	const FVector FlameScale(0.45f, 0.9f, 1.f);
	constexpr float FlameRise = 70.f;   // units a flame climbs over its life
	constexpr float FlameBehind = 22.f; // from the capsule centre away from the camera: past the back
	constexpr float FlameInner = 26.f;  // nearest and furthest sideways from the body's centre line, as seen from the camera
	constexpr float FlameOuter = 50.f;

	void RespawnFlame(float& Side, float& Height, float& Duration, float HalfHeight)
	{
		Side = FMath::FRandRange(FlameInner, FlameOuter) * (FMath::RandBool() ? 1.f : -1.f);
		Height = FMath::FRandRange(40.f - HalfHeight, 0.6f * HalfHeight); // a newborn card's bottom stays off the floor, which would cut it flat
		Duration = FMath::FRandRange(0.5f, 0.9f);
	}
}

UShinobiChakraAuraComponent::UShinobiChakraAuraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while shown or fading
}

int32 UShinobiChakraAuraComponent::NumVisibleFlowCards() const
{
	int32 Count = 0;
	for (const UStaticMeshComponent* Card : FlowCards)
	{
		Count += Card->IsVisible() ? 1 : 0;
	}
	return Count;
}

FGameplayTag UShinobiChakraAuraComponent::GetShownTag() const
{
	return ShownIndex == INDEX_NONE ? FGameplayTag() : Looks()[ShownIndex].Tag;
}

void UShinobiChakraAuraComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		return; // purely cosmetic: nothing to render on a dedicated server, and no gameplay depends on it
	}

	const AShinobiCharacterBase* OwnerCharacter = Cast<AShinobiCharacterBase>(GetOwner());
	BodyMesh = OwnerCharacter ? OwnerCharacter->GetBodyMesh() : nullptr;

	AuraMaterial = ShinobiVfx::MakeGlow(this, ShinobiVfx::Colors::Chakra, 2.5f, 0.f, BodyInflate);
	if (AuraMaterial)
	{
		AuraMaterial->SetScalarParameterValue(TEXT("Fade"), 0.f);
		if (BodyMesh)
		{
			// ponytail: owns the mesh's single overlay slot; if another effect needs it later (hit flash),
			// move the aura to a leader-posed child mesh. Applied once here and never toggled again.
			BodyMesh->SetOverlayMaterial(AuraMaterial);
		}
	}

	Light = NewObject<UPointLightComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UPointLightComponent::StaticClass(), TEXT("AuraLight")));
	Light->SetupAttachment(this);
	// Stays at the capsule centre, inside the body (shadowless): a soft floor pool around the feet from any camera
	// angle. A chest-front offset (20, 0, 30) sat ~20 uu off the chest and blew it out to a white hotspot.
	// No specular: its floor glint lands between pawn and camera and follows the camera around.
	Light->SetSpecularScale(0.f);
	Light->SetAttenuationRadius(LightRadius);
	ShinobiVfx::ApplyEffectLight(Light); // shadows while MegaLights is on
	Light->SetVisibility(false);
	Light->RegisterComponent();

	if (const ACharacter* OwnerAsCharacter = Cast<ACharacter>(GetOwner()))
	{
		HalfHeight = OwnerAsCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	CreateFlowCards();

	if (const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		AbilitySystem = Owner->GetAbilitySystemComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		for (const FAuraLook& Look : Looks())
		{
			TagEventHandles.Add(ASC->RegisterGameplayTagEvent(Look.Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UShinobiChakraAuraComponent::OnStateTagChanged));
		}
	}
	RefreshShownState();
	SetComponentTickEnabled(ShownIndex != INDEX_NONE);
	UpdateAura();
}

void UShinobiChakraAuraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		for (int32 Index = 0; Index < TagEventHandles.Num(); ++Index)
		{
			ASC->UnregisterGameplayTagEvent(TagEventHandles[Index], Looks()[Index].Tag, EGameplayTagEventType::NewOrRemoved);
		}
	}
	TagEventHandles.Reset();
	if (BodyMesh && BodyMesh->GetOverlayMaterial() == AuraMaterial)
	{
		BodyMesh->SetOverlayMaterial(nullptr);
	}
	BodyMesh = nullptr;
	AuraMaterial = nullptr;
	if (Light)
	{
		Light->DestroyComponent();
		Light = nullptr;
	}
	for (UStaticMeshComponent* Card : FlowCards)
	{
		Card->DestroyComponent();
	}
	if (Pillar)
	{
		Pillar->DestroyComponent();
		Pillar = nullptr;
	}
	PillarMaterial = nullptr;
	FlowCards.Reset();
	FlowMaterials.Reset();
	Flow.Reset();
	Fade = 0.f;
	FlowFade = 0.f;
	bCharging = false;
	ShownIndex = INDEX_NONE;
	Super::EndPlay(EndPlayReason);
}

void UShinobiChakraAuraComponent::OnStateTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshShownState();
	SetComponentTickEnabled(true); // fade in, switch look, or fade out
}

void UShinobiChakraAuraComponent::RefreshShownState()
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	bCharging = ASC && ASC->HasMatchingGameplayTag(ShinobiTags::State_ChakraCharging);
	int32 NewIndex = INDEX_NONE;
	for (int32 Index = 0; ASC && Index < Looks().Num(); ++Index)
	{
		if (ASC->HasMatchingGameplayTag(Looks()[Index].Tag))
		{
			NewIndex = Index;
			break;
		}
	}
	ShownIndex = NewIndex;
	if (NewIndex == INDEX_NONE)
	{
		return; // the material and light keep the last look's colour while fading out
	}
	if (AuraMaterial)
	{
		AuraMaterial->SetVectorParameterValue(TEXT("Color"), Looks()[NewIndex].Color);
	}
	if (Light)
	{
		Light->SetLightColor(Looks()[NewIndex].Color);
	}
}

void UShinobiChakraAuraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const bool bShown = ShownIndex != INDEX_NONE;
	Fade = FMath::FInterpConstantTo(Fade, bShown ? 1.f : 0.f, DeltaTime, 1.f / FadeTime);
	Age += DeltaTime;
	UpdateAura();
	UpdateFlow(DeltaTime);
	if (!bShown && Fade <= 0.f && FlowFade <= 0.f)
	{
		SetComponentTickEnabled(false);
	}
}

void UShinobiChakraAuraComponent::UpdateAura()
{
	// ponytail: two-sine flicker; swap for a curve asset if art direction wants a specific rhythm
	const float Flicker = 0.8f + 0.2f * FMath::Sin(Age * 23.f) * FMath::Sin(Age * 7.f);
	if (AuraMaterial)
	{
		AuraMaterial->SetScalarParameterValue(TEXT("Fade"), Fade);
		if (ShownIndex != INDEX_NONE) // while fading out, keep the last look's intensity
		{
			const FAuraLook& Look = Looks()[ShownIndex];
			// Shock crackles: a hard random flicker instead of the soft sine. Chakra surges in slow pulses.
			const float Pulse = Look.Tag == ShinobiTags::State_ChakraCharging ? 0.85f + 0.3f * FMath::Sin(Age * 6.f) : 1.f;
			AuraMaterial->SetScalarParameterValue(TEXT("Intensity"), Look.bFlicker ? Look.Intensity * FMath::FRandRange(0.3f, 1.6f) : Look.Intensity * Pulse);
		}
	}
	if (Light)
	{
		Light->SetVisibility(Fade > 0.f);
		Light->SetIntensity(LightIntensity * Fade * Flicker);
	}
}

void UShinobiChakraAuraComponent::CreateFlowCards()
{
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* PillarParent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Shinobi/VFX/M_ChakraPillar.M_ChakraPillar"));
	if (ensure(Cylinder && PillarParent))
	{
		// From the floor to above the head.
		const float Height = 2.f * HalfHeight + PillarTop;
		PillarMaterial = UMaterialInstanceDynamic::Create(PillarParent, this);
		PillarMaterial->SetVectorParameterValue(TEXT("Color"), ChargeColor);
		PillarMaterial->SetScalarParameterValue(TEXT("Height"), Height);
		Pillar = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), TEXT("ChakraPillar")));
		Pillar->SetStaticMesh(Cylinder);
		Pillar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pillar->SetCastShadow(false);
		Pillar->SetMaterial(0, PillarMaterial);
		Pillar->SetUsingAbsoluteScale(true);
		Pillar->SetRelativeLocation(FVector(0.f, 0.f, PillarTop / 2.f));
		Pillar->SetWorldScale3D(FVector(PillarRadius / 50.f, PillarRadius / 50.f, Height / 100.f));
		Pillar->SetVisibility(false);
		Pillar->SetupAttachment(this);
		Pillar->RegisterComponent();
	}

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Plane)
	{
		return;
	}
	for (int32 Index = 0; Index < NumFlames; ++Index)
	{
		UMaterialInstanceDynamic* Material = ShinobiVfx::MakeSprite(this, ShinobiVfx::ESprite::ChakraWisp, Index, FlameTint, 1.5f);
		if (!Material)
		{
			break; // asset missing (MakeSprite ensured): no flow, the body glow still works
		}
		UStaticMeshComponent* Card = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), TEXT("ChakraFlow")));
		Card->SetStaticMesh(Plane);
		Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Card->SetCastShadow(false);
		Card->SetMaterial(0, Material);
		Card->SetUsingAbsoluteRotation(true);
		Card->SetUsingAbsoluteScale(true);
		Card->SetVisibility(false);
		Card->SetupAttachment(this);
		Card->RegisterComponent();
		FlowCards.Add(Card);
		FlowMaterials.Add(Material);
		// Staggered lives, so the flow is already continuous on the first frame of a charge.
		FFlowCard State;
		State.Life = FMath::FRand();
		RespawnFlame(State.Side, State.Height, State.Duration, HalfHeight);
		Flow.Add(State);
	}
}

void UShinobiChakraAuraComponent::UpdateFlow(float DeltaTime)
{
	FlowFade = FMath::FInterpConstantTo(FlowFade, bCharging ? 1.f : 0.f, DeltaTime, 1.f / FadeTime);
	const bool bFlowing = FlowFade > 0.f;
	for (UStaticMeshComponent* Card : FlowCards)
	{
		Card->SetVisibility(bFlowing);
	}
	if (Pillar)
	{
		Pillar->SetVisibility(bFlowing);
		PillarMaterial->SetScalarParameterValue(TEXT("Fade"), FlowFade * (0.85f + 0.15f * FMath::Sin(Age * 5.f)));
	}
	if (!bFlowing || FlowCards.Num() != NumFlames)
	{
		return;
	}
	// Each machine places the flames for its own camera (cosmetic only), so they stay behind the body from any angle.
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector Centre = GetComponentLocation();
	const FVector ToCamera = Camera ? (Camera->GetCameraLocation() - Centre).GetSafeNormal2D() : GetForwardVector().GetSafeNormal2D();
	const FVector Sideways(-ToCamera.Y, ToCamera.X, 0.f);
	const FRotator FaceCamera(0.f, ToCamera.Rotation().Yaw - 90.f, 90.f);
	for (int32 Index = 0; Index < NumFlames; ++Index)
	{
		FFlowCard& State = Flow[Index];
		State.Life += DeltaTime / State.Duration;
		if (State.Life >= 1.f)
		{
			State.Life = FMath::Fractional(State.Life);
			RespawnFlame(State.Side, State.Height, State.Duration, HalfHeight);
			ShinobiVfx::SetSpriteVariant(FlowMaterials[Index], ShinobiVfx::ESprite::ChakraWisp, FMath::RandRange(0, 3));
		}
		const float T = State.Life;
		// Flames lean a little outward as they climb, and stretch: chakra streaming up off the body.
		UStaticMeshComponent* Card = FlowCards[Index];
		Card->SetWorldLocation(Centre + Sideways * State.Side * (1.f + 0.25f * T) - ToCamera * FlameBehind + FVector(0.f, 0.f, State.Height + FlameRise * T));
		Card->SetWorldRotation(FaceCamera);
		Card->SetWorldScale3D(FlameScale * FVector(1.f - 0.3f * T, 0.8f + 0.6f * T, 1.f));
		FlowMaterials[Index]->SetScalarParameterValue(TEXT("Fade"), FlowFade * FMath::Sin(PI * T));
	}
}
