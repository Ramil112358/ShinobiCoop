#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
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
			{ ShinobiTags::State_Burning_Black, ShinobiVfx::Colors::BlackFlame, 4.f, false },
			{ ShinobiTags::State_Burning_Normal, ShinobiVfx::Colors::Fire, 2.5f, false },
			{ ShinobiTags::State_Shocked, ShinobiVfx::Colors::Lightning, 4.f, true },
			{ ShinobiTags::State_ChakraCharging, ShinobiVfx::Colors::Chakra, 2.5f, false },
		};
		return Table;
	}

	constexpr float BodyInflate = 2.5f;
}

UShinobiChakraAuraComponent::UShinobiChakraAuraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while shown or fading
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
	Fade = 0.f;
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
	if (!bShown && Fade <= 0.f)
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
			// Shock crackles: a hard random flicker instead of the soft sine.
			AuraMaterial->SetScalarParameterValue(TEXT("Intensity"), Look.bFlicker ? Look.Intensity * FMath::FRandRange(0.3f, 1.6f) : Look.Intensity);
		}
	}
	if (Light)
	{
		Light->SetVisibility(Fade > 0.f);
		Light->SetIntensity(LightIntensity * Fade * Flicker);
	}
}
