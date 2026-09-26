#include "Shinobi/VFX/ShinobiCastFxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	struct FOrbLook
	{
		FGameplayTag Tag;
		FLinearColor Color;
		float Scale; // the engine sphere is 100 units across
		bool bCrackle;
	};

	/** Built on first use, after the native tags are registered. */
	const TArray<FOrbLook>& OrbLooks()
	{
		static const TArray<FOrbLook> Table = {
			{ ShinobiTags::State_Casting_Fireball, ShinobiVfx::Colors::Fire, 0.22f, false },
			{ ShinobiTags::State_Casting_Chidori, ShinobiVfx::Colors::Lightning, 0.2f, true },
			{ ShinobiTags::State_Casting_Rasengan, ShinobiVfx::Colors::Chakra, 0.32f, false },
			{ ShinobiTags::State_Casting_Amaterasu, ShinobiVfx::Colors::BlackFlame, 0.2f, false },
		};
		return Table;
	}

	constexpr float OrbIntensity = 6.f;
	constexpr float OrbLightIntensity = 800.f;
}

UShinobiCastFxComponent::UShinobiCastFxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while an orb is shown
}

FGameplayTag UShinobiCastFxComponent::GetShownTag() const
{
	return ShownIndex == INDEX_NONE ? FGameplayTag() : OrbLooks()[ShownIndex].Tag;
}

void UShinobiCastFxComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Body = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	Orb = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), TEXT("CastOrb")));
	Orb->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Orb->SetCastShadow(false);
	OrbMaterial = ShinobiVfx::MakeGlow(this, ShinobiVfx::Colors::Fire, OrbIntensity, 1.f);
	Orb->SetMaterial(0, OrbMaterial);
	Orb->SetVisibility(false);
	if (Body)
	{
		Orb->SetupAttachment(Body, TEXT("hand_r")); // a bone name works as a socket
	}
	else
	{
		Orb->SetupAttachment(this);
	}
	Orb->RegisterComponent();

	Light = NewObject<UPointLightComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UPointLightComponent::StaticClass(), TEXT("CastLight")));
	Light->SetupAttachment(Orb);
	Light->SetAttenuationRadius(250.f);
	Light->SetCastShadows(false);
	Light->SetSpecularScale(0.f);
	Light->SetVisibility(false);
	Light->RegisterComponent();

	if (const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		AbilitySystem = Owner->GetAbilitySystemComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		for (const FOrbLook& Look : OrbLooks())
		{
			TagEventHandles.Add(ASC->RegisterGameplayTagEvent(Look.Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UShinobiCastFxComponent::OnCastingTagChanged));
		}
	}
	Refresh();
}

void UShinobiCastFxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		for (int32 Index = 0; Index < TagEventHandles.Num(); ++Index)
		{
			ASC->UnregisterGameplayTagEvent(TagEventHandles[Index], OrbLooks()[Index].Tag, EGameplayTagEventType::NewOrRemoved);
		}
	}
	TagEventHandles.Reset();
	if (Light)
	{
		Light->DestroyComponent();
		Light = nullptr;
	}
	if (Orb)
	{
		Orb->DestroyComponent();
		Orb = nullptr;
	}
	OrbMaterial = nullptr;
	ShownIndex = INDEX_NONE;
	Super::EndPlay(EndPlayReason);
}

void UShinobiCastFxComponent::OnCastingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	Refresh();
}

void UShinobiCastFxComponent::Refresh()
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	ShownIndex = INDEX_NONE;
	for (int32 Index = 0; ASC && Index < OrbLooks().Num(); ++Index)
	{
		if (ASC->HasMatchingGameplayTag(OrbLooks()[Index].Tag))
		{
			ShownIndex = Index;
			break;
		}
	}
	const bool bShown = ShownIndex != INDEX_NONE && Orb;
	if (Orb)
	{
		Orb->SetVisibility(bShown);
	}
	if (Light)
	{
		Light->SetVisibility(bShown);
	}
	if (bShown)
	{
		const FOrbLook& Look = OrbLooks()[ShownIndex];
		Orb->SetRelativeScale3D(FVector(Look.Scale));
		if (OrbMaterial)
		{
			OrbMaterial->SetVectorParameterValue(TEXT("Color"), Look.Color);
		}
		Light->SetLightColor(Look.Color);
		Light->SetIntensity(OrbLightIntensity);
	}
	SetComponentTickEnabled(bShown);
}

void UShinobiCastFxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (ShownIndex == INDEX_NONE || !Orb)
	{
		return;
	}
	Age += DeltaTime;
	const FOrbLook& Look = OrbLooks()[ShownIndex];
	// Lightning crackles (random size and brightness every frame); the others pulse gently.
	const float Pulse = Look.bCrackle ? FMath::FRandRange(0.6f, 1.5f) : 1.f + 0.12f * FMath::Sin(Age * 12.f);
	Orb->SetRelativeScale3D(FVector(Look.Scale * Pulse));
	if (OrbMaterial)
	{
		OrbMaterial->SetScalarParameterValue(TEXT("Intensity"), OrbIntensity * Pulse);
	}
	if (Light)
	{
		Light->SetIntensity(OrbLightIntensity * Pulse);
	}
}
