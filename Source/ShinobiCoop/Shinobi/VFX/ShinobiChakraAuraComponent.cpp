#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	const TCHAR* BodyAuraMaterialPath = TEXT("/Game/Shinobi/VFX/M_ChakraBodyAura.M_ChakraBodyAura");
	const FName ColorParam(TEXT("Color"));
	const FName IntensityParam(TEXT("Intensity"));
	const FName FadeParam(TEXT("Fade"));
	// Chest height, slightly forward of the capsule centre, so the body's front picks up the blue light.
	const FVector LightOffset(20.f, 0.f, 30.f);
}

UShinobiChakraAuraComponent::UShinobiChakraAuraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while charging or fading
}

void UShinobiChakraAuraComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		return; // purely cosmetic: nothing to render on a dedicated server, and no gameplay depends on it
	}

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	BodyMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;

	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, BodyAuraMaterialPath))
	{
		AuraMaterial = UMaterialInstanceDynamic::Create(Material, this);
		AuraMaterial->SetVectorParameterValue(ColorParam, Color);
	}
	else
	{
		ensureMsgf(false, TEXT("UShinobiChakraAuraComponent: missing material %s"), BodyAuraMaterialPath);
	}

	// ponytail: owns the mesh's single overlay slot; if another effect needs it later (hit flash),
	// move the aura to a leader-posed child mesh.
	Light = NewObject<UPointLightComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UPointLightComponent::StaticClass(), TEXT("AuraLight")));
	Light->SetupAttachment(this);
	Light->SetRelativeLocation(LightOffset);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(LightRadius);
	Light->SetCastShadows(false);
	Light->SetVisibility(false);
	Light->RegisterComponent();

	if (const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		AbilitySystem = Owner->GetAbilitySystemComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		TagEventHandle = ASC->RegisterGameplayTagEvent(ShinobiTags::State_ChakraCharging, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UShinobiChakraAuraComponent::OnChargingTagChanged);
		bCharging = ASC->HasMatchingGameplayTag(ShinobiTags::State_ChakraCharging);
	}
	SetComponentTickEnabled(bCharging);
	UpdateAura();
}

void UShinobiChakraAuraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->UnregisterGameplayTagEvent(TagEventHandle, ShinobiTags::State_ChakraCharging, EGameplayTagEventType::NewOrRemoved);
	}
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
	Fade = 0.f;
	bCharging = false;
	Super::EndPlay(EndPlayReason);
}

void UShinobiChakraAuraComponent::OnChargingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bCharging = NewCount > 0;
	SetComponentTickEnabled(true); // fade in or out
}

void UShinobiChakraAuraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Fade = FMath::FInterpConstantTo(Fade, bCharging ? 1.f : 0.f, DeltaTime, 1.f / FadeTime);
	Age += DeltaTime;
	UpdateAura();
	if (!bCharging && Fade <= 0.f)
	{
		SetComponentTickEnabled(false);
	}
}

void UShinobiChakraAuraComponent::UpdateAura()
{
	const bool bShouldShow = Fade > 0.f;
	if (BodyMesh && AuraMaterial)
	{
		AuraMaterial->SetScalarParameterValue(FadeParam, Fade);
		AuraMaterial->SetScalarParameterValue(IntensityParam, Intensity);
		UMaterialInterface* const Desired = bShouldShow ? static_cast<UMaterialInterface*>(AuraMaterial) : nullptr;
		if (BodyMesh->GetOverlayMaterial() != Desired)
		{
			BodyMesh->SetOverlayMaterial(Desired);
		}
	}
	if (Light)
	{
		// ponytail: two-sine flicker; swap for a curve asset if art direction wants a specific rhythm
		const float Flicker = 0.8f + 0.2f * FMath::Sin(Age * 23.f) * FMath::Sin(Age * 7.f);
		Light->SetVisibility(bShouldShow);
		Light->SetIntensity(LightIntensity * Fade * Flicker);
	}
}
