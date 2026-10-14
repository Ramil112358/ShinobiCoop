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

	/** Highest priority first: the first tag the owner has is the one shown. Built on first use, after the
	 * native tags are registered (a file-scope table would copy them before they have values). */
	const TArray<FAuraLook>& Looks()
	{
		static const TArray<FAuraLook> Table = {
			{ ShinobiTags::State_Burning_Black, ShinobiVfx::Colors::BlackFlame, 1.5f, false }, // a faint rim: the flame tongues (UShinobiBodyFlameComponent) carry the look
			{ ShinobiTags::State_Burning_Normal, ShinobiVfx::Colors::Fire, 2.5f, false },
			{ ShinobiTags::State_Shocked, ShinobiVfx::Colors::Lightning, 4.f, true },
			{ ShinobiTags::State_ChakraCharging, ShinobiVfx::Colors::Chakra, 2.5f, false },
			{ ShinobiTags::State_Chilled, ShinobiVfx::Colors::Ice, 2.f, false },
		};
		return Table;
	}

	constexpr float BodyInflate = 2.5f;

	// Chakra flow while charging (the engine plane is 100 units across; X is the card's width, Y its height).
	constexpr int32 NumWisps = 14;  // soft wisps rising off the body, camera-facing
	constexpr int32 NumSwirls = 4;  // streams spiralling from the feet up to the head, lying along their orbit
	const FVector WispScale(0.4f, 0.8f, 1.f);
	const FVector SwirlScale(0.7f, 1.4f, 1.f);
	constexpr float WispRise = 55.f;      // units a wisp climbs over its life
	constexpr float SwirlTurn = 330.f;    // degrees a stream orbits over its life
	constexpr float SwirlRadius = 45.f;   // tightens to two thirds of this as it climbs
	constexpr float SwirlPitch = -15.f;   // leading head tilted up, so the stream reads as climbing
	constexpr float GroundSpin = -140.f;  // degrees per second of the swirl under the feet
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
	Light->SetCastShadows(false);
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
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Plane)
	{
		return;
	}
	using ShinobiVfx::ESprite;
	for (int32 Index = 0; Index < NumWisps + NumSwirls + 1; ++Index)
	{
		const ESprite Kind = Index < NumWisps ? ESprite::ChakraWisp : Index < NumWisps + NumSwirls ? ESprite::ChakraSwirl : ESprite::Vortex;
		UMaterialInstanceDynamic* Material = ShinobiVfx::MakeSprite(this, Kind, Index, FLinearColor::White, Kind == ESprite::Vortex ? 0.8f : 1.5f);
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
		State.Duration = Index < NumWisps ? FMath::FRandRange(0.6f, 1.f) : 1.2f;
		State.Angle = Index < NumWisps ? FMath::FRandRange(0.f, 360.f) : (Index - NumWisps) * 360.f / NumSwirls;
		State.Radius = Index < NumWisps ? FMath::FRandRange(14.f, 30.f) : SwirlRadius;
		State.Height = Index < NumWisps ? FMath::FRandRange(15.f - HalfHeight, 0.7f * HalfHeight) : 10.f - HalfHeight;
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
	if (!bFlowing || FlowCards.Num() != NumWisps + NumSwirls + 1)
	{
		return;
	}
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraAt = Camera ? Camera->GetCameraLocation() : FVector::ZeroVector;
	const float OwnerYaw = GetComponentRotation().Yaw;
	for (int32 Index = 0; Index < NumWisps + NumSwirls; ++Index)
	{
		const bool bWisp = Index < NumWisps;
		FFlowCard& State = Flow[Index];
		State.Life += DeltaTime / State.Duration;
		if (State.Life >= 1.f)
		{
			// Reborn somewhere new with another shape; streams keep their slot on the orbit and wobble around it.
			State.Life = FMath::Fractional(State.Life);
			if (bWisp)
			{
				State.Angle = FMath::FRandRange(0.f, 360.f);
				State.Radius = FMath::FRandRange(14.f, 30.f);
				State.Height = FMath::FRandRange(15.f - HalfHeight, 0.7f * HalfHeight);
				State.Duration = FMath::FRandRange(0.6f, 1.f);
			}
			else
			{
				State.Angle = (Index - NumWisps) * 360.f / NumSwirls + FMath::FRandRange(-25.f, 25.f);
			}
			ShinobiVfx::SetSpriteVariant(FlowMaterials[Index], bWisp ? ShinobiVfx::ESprite::ChakraWisp : ShinobiVfx::ESprite::ChakraSwirl, FMath::RandRange(0, 3));
		}
		const float T = State.Life;
		// Wisps drift a little around the body as they rise; streams orbit from the feet up to the head.
		const float Angle = State.Angle + (bWisp ? 50.f : SwirlTurn) * T;
		const float Radius = bWisp ? State.Radius * (1.f + 0.4f * T) : State.Radius * (1.f - T / 3.f);
		const float Z = State.Height + (bWisp ? WispRise : 2.f * HalfHeight - 20.f) * T;
		UStaticMeshComponent* Card = FlowCards[Index];
		Card->SetRelativeLocation(FVector(Radius * FMath::Cos(FMath::DegreesToRadians(Angle)), Radius * FMath::Sin(FMath::DegreesToRadians(Angle)), Z));
		float Envelope;
		if (bWisp)
		{
			const FVector Here = Card->GetComponentLocation();
			const float FaceYaw = Camera ? FMath::RadiansToDegrees(FMath::Atan2(CameraAt.Y - Here.Y, CameraAt.X - Here.X)) - 90.f : 0.f;
			Card->SetWorldRotation(FRotator(0.f, FaceYaw, 90.f));
			Card->SetWorldScale3D(WispScale * (0.8f + 0.5f * T));
			Envelope = FMath::Sin(PI * T);
		}
		else
		{
			// The card lies along its orbit (normal pointing out from the body), head first, so it wraps around the body.
			Card->SetWorldRotation(FRotator(SwirlPitch, OwnerYaw + Angle - 90.f, 90.f));
			Card->SetWorldScale3D(SwirlScale);
			Envelope = FMath::Min(T / 0.2f, 1.f) * FMath::Min((1.f - T) / 0.3f, 1.f);
		}
		FlowMaterials[Index]->SetScalarParameterValue(TEXT("Fade"), FlowFade * Envelope);
	}
	// The ground swirl: chakra gathered from around the feet, turning slowly.
	UStaticMeshComponent* Ground = FlowCards.Last();
	Ground->SetRelativeLocation(FVector(0.f, 0.f, 2.f - HalfHeight));
	Ground->SetWorldRotation(FRotator(0.f, GroundSpin * Age, 0.f));
	Ground->SetWorldScale3D(FVector(1.5f));
	FlowMaterials.Last()->SetScalarParameterValue(TEXT("Fade"), FlowFade * (0.6f + 0.15f * FMath::Sin(Age * 4.f)));
}
