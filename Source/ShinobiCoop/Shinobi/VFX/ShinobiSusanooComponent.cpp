#include "Shinobi/VFX/ShinobiSusanooComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr float SusanooFadeInSeconds = 0.8f;
	constexpr float SusanooFadeOutSeconds = 0.5f;
}

UShinobiSusanooComponent::UShinobiSusanooComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only while the figure shows or fades
}

void UShinobiSusanooComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	if (const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		AbilitySystem = Owner->GetAbilitySystemComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		Handle = ASC->RegisterGameplayTagEvent(ShinobiTags::State_Susanoo).AddUObject(this, &UShinobiSusanooComponent::OnSusanooChanged);
		bWanted = ASC->HasMatchingGameplayTag(ShinobiTags::State_Susanoo); // a client that joins mid-armour
		if (bWanted)
		{
			EnsureFigure();
		}
		SetComponentTickEnabled(bWanted);
	}
}

void UShinobiSusanooComponent::EnsureFigure()
{
	if (Figure)
	{
		return;
	}
	// Built the first time the armour goes up, not at spawn: most characters (every enemy and villager) never wear it.
	// The ribcage model (Scripts/character/make_susanoo.py, imported by Scripts/setup_susanoo.py) has its origin at the feet.
	UStaticMesh* Model = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Shinobi/Jutsu/SM_Susanoo.SM_Susanoo"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Model)
	{
		return;
	}
	Figure = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), TEXT("SusanooFigure")));
	Figure->SetStaticMesh(Model);
	Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Figure->SetCastShadow(false);
	Figure->SetVisibility(false);
	Material = ShinobiVfx::MakeSusanoo(this, 0.f);
	Figure->SetMaterial(0, Material);
	Figure->SetupAttachment(GetOwner()->GetRootComponent());
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	Figure->SetRelativeLocation(FVector(0.f, 0.f, Character ? -Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f));
	Figure->RegisterComponent();
}

void UShinobiSusanooComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->UnregisterGameplayTagEvent(Handle, ShinobiTags::State_Susanoo);
	}
	if (Figure)
	{
		Figure->DestroyComponent();
		Figure = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UShinobiSusanooComponent::OnSusanooChanged(const FGameplayTag Tag, int32 Count)
{
	bWanted = Count > 0;
	if (bWanted)
	{
		EnsureFigure();
	}
	SetComponentTickEnabled(true); // fade in or out
}

void UShinobiSusanooComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Age += DeltaTime;
	Fade = bWanted ? FMath::Min(Fade + DeltaTime / SusanooFadeInSeconds, 1.f) : FMath::Max(Fade - DeltaTime / SusanooFadeOutSeconds, 0.f);
	if (Figure)
	{
		Figure->SetVisibility(Fade > 0.f);
		if (Material)
		{
			Material->SetScalarParameterValue(TEXT("Fade"), Fade);
		}
		// Rises as it fades in and breathes slowly while it stands.
		const float Rise = 0.85f + 0.15f * Fade;
		Figure->SetRelativeScale3D(FVector(1.f + 0.02f * FMath::Sin(Age * 2.5f), 1.f + 0.02f * FMath::Sin(Age * 2.5f), Rise));
	}
	if (!bWanted && Fade <= 0.f)
	{
		SetComponentTickEnabled(false);
	}
}
