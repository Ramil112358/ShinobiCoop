#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* PlanePath = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* AuraMaterialPath = TEXT("/Game/Shinobi/VFX/M_ChakraAura.M_ChakraAura");
	const TCHAR* RingMaterialPath = TEXT("/Game/Shinobi/VFX/M_ChakraRing.M_ChakraRing");
	const FName ColorParam(TEXT("Color"));
	const FName IntensityParam(TEXT("Intensity"));
	const FName FadeParam(TEXT("Fade"));
}

UShinobiChakraAuraComponent::UShinobiChakraAuraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while charging or fading
}

void UShinobiChakraAuraComponent::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, CylinderPath);
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, PlanePath);
	UMaterialInterface* AuraMaterial = LoadObject<UMaterialInterface>(nullptr, AuraMaterialPath);
	UMaterialInterface* RingMaterial = LoadObject<UMaterialInterface>(nullptr, RingMaterialPath);

	// Engine basic shapes are 100 uu across with the pivot at the centre; this component sits at the
	// capsule centre (192 uu tall), so -94 is just above the floor.
	Pieces.Add(AddPiece(TEXT("AuraInnerShell"), Cylinder, AuraMaterial, FVector(0.f, 0.f, 10.f), FVector(1.1f, 1.1f, 2.2f)));
	Pieces.Add(AddPiece(TEXT("AuraOuterShell"), Cylinder, AuraMaterial, FVector(0.f, 0.f, 25.f), FVector(1.5f, 1.5f, 2.6f)));
	Pieces.Add(AddPiece(TEXT("AuraGroundRing"), Plane, RingMaterial, FVector(0.f, 0.f, -94.f), FVector(3.f, 3.f, 1.f)));

	Light = NewObject<UPointLightComponent>(GetOwner(), TEXT("AuraLight"));
	Light->SetupAttachment(this);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(450.f);
	Light->SetCastShadows(false);
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
	UpdatePieces();
}

void UShinobiChakraAuraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->UnregisterGameplayTagEvent(TagEventHandle, ShinobiTags::State_ChakraCharging, EGameplayTagEventType::NewOrRemoved);
	}
	Super::EndPlay(EndPlayReason);
}

UStaticMeshComponent* UShinobiChakraAuraComponent::AddPiece(FName Name, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Offset, const FVector& Scale)
{
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(GetOwner(), Name);
	Piece->SetupAttachment(this);
	Piece->SetStaticMesh(Mesh);
	Piece->SetRelativeLocation(Offset);
	Piece->SetRelativeScale3D(Scale);
	Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetCastShadow(false);
	Piece->RegisterComponent();

	UMaterialInstanceDynamic* Dynamic = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	if (Dynamic)
	{
		Dynamic->SetVectorParameterValue(ColorParam, Color);
		Piece->SetMaterial(0, Dynamic);
	}
	PieceMaterials.Add(Dynamic);
	return Piece;
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
	if (Pieces.Num() >= 2)
	{
		Pieces[0]->AddRelativeRotation(FRotator(0.f, SpinSpeed * DeltaTime, 0.f));
		Pieces[1]->AddRelativeRotation(FRotator(0.f, -SpinSpeed * DeltaTime, 0.f));
	}
	UpdatePieces();
	if (!bCharging && Fade <= 0.f)
	{
		SetComponentTickEnabled(false);
	}
}

void UShinobiChakraAuraComponent::UpdatePieces()
{
	const bool bShouldShow = Fade > 0.f;
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		Pieces[Index]->SetVisibility(bShouldShow);
		if (UMaterialInstanceDynamic* Dynamic = PieceMaterials[Index])
		{
			Dynamic->SetScalarParameterValue(FadeParam, Fade);
			Dynamic->SetScalarParameterValue(IntensityParam, Intensity);
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
