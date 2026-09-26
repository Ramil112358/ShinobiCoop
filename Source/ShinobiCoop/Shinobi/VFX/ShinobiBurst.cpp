#include "Shinobi/VFX/ShinobiBurst.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/VFX/ShinobiSfx.h"
#include "Shinobi/VFX/ShinobiVfx.h"

AShinobiBurst::AShinobiBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true; // brief and cheap; everyone nearby should see it
	InitialLifeSpan = 2.5f; // the server's copy; its destruction removes it on clients

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Sphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sphere"));
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sphere->SetCastShadow(false);
	if (SphereMesh.Succeeded())
	{
		Sphere->SetStaticMesh(SphereMesh.Object);
	}
	RootComponent = Sphere;

	Log = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Log"));
	Log->SetupAttachment(Sphere);
	Log->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Log->SetUsingAbsoluteScale(true);
	Log->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.6f)); // a standing log about as tall as a character
	Log->SetVisibility(false);
	if (CylinderMesh.Succeeded())
	{
		Log->SetStaticMesh(CylinderMesh.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Sphere);
	Light->SetCastShadows(false);
	Light->SetSpecularScale(0.f);
	Light->SetVisibility(false);
}

AShinobiBurst* AShinobiBurst::Spawn(UWorld* World, EShinobiBurst Kind, const FVector& Location)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FTransform Where(Location);
	AShinobiBurst* Burst = World->SpawnActorDeferred<AShinobiBurst>(AShinobiBurst::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Burst)
	{
		Burst->Kind = Kind;
		Burst->FinishSpawning(Where);
	}
	return Burst;
}

void AShinobiBurst::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiBurst, Kind, COND_InitialOnly);
}

void AShinobiBurst::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		SetActorTickEnabled(false);
		return;
	}
	static const EShinobiSfx Sounds[] = { EShinobiSfx::Explosion, EShinobiSfx::Explosion, EShinobiSfx::Zap, EShinobiSfx::Whirr, EShinobiSfx::Poof };
	ShinobiSfx::Play(this, Sounds[static_cast<uint8>(Kind)], GetActorLocation(), Kind == EShinobiBurst::Fire ? 0.7f : 1.f);

	// Mostly-rim shells (low Core) at modest intensity: a solid bright sphere blows out to a white wall under bloom.
	FLinearColor Color = ShinobiVfx::Colors::Fire;
	float Core = 0.3f;
	switch (Kind)
	{
	case EShinobiBurst::Fire:         Color = ShinobiVfx::Colors::Fire;      Duration = 0.4f; MaxScale = 3.f;  Intensity = 4.f;   LightIntensity = 20000.f; break;
	case EShinobiBurst::Firestorm:    Color = ShinobiVfx::Colors::Fire;      Duration = 0.6f; MaxScale = 10.f; Intensity = 2.5f;  LightIntensity = 60000.f; Core = 0.15f; break;
	case EShinobiBurst::Lightning:    Color = ShinobiVfx::Colors::Lightning; Duration = 0.3f; MaxScale = 2.5f; Intensity = 4.f;   LightIntensity = 30000.f; break;
	case EShinobiBurst::Spiral:       Color = ShinobiVfx::Colors::Chakra;    Duration = 0.4f; MaxScale = 3.f;  Intensity = 4.f;   LightIntensity = 20000.f; break;
	case EShinobiBurst::Substitution: Color = ShinobiVfx::Colors::Smoke;     Duration = 0.5f; MaxScale = 2.2f; Intensity = 0.8f;  LightIntensity = 0.f;     Core = 0.6f; break;
	}
	SphereMaterial = ShinobiVfx::MakeGlow(this, Color, Intensity, Core);
	Sphere->SetMaterial(0, SphereMaterial);
	Sphere->SetRelativeScale3D(FVector(0.2f * MaxScale));
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(60.f * MaxScale);
	Light->SetVisibility(LightIntensity > 0.f);

	if (Kind == EShinobiBurst::Substitution)
	{
		// ponytail: an engine cylinder in the basic shape material, tinted wood-brown, until a real log mesh
		if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* Wood = UMaterialInstanceDynamic::Create(Basic, this);
			Wood->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.13f, 0.05f));
			Log->SetMaterial(0, Wood);
		}
		Log->SetVisibility(true);
	}
}

void AShinobiBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Duration, 0.f, 1.f);
	const float Grow = 1.f - FMath::Square(1.f - T); // fast out, slowing down
	Sphere->SetRelativeScale3D(FVector(FMath::Lerp(0.2f, 1.f, Grow) * MaxScale));
	if (SphereMaterial)
	{
		SphereMaterial->SetScalarParameterValue(TEXT("Fade"), 1.f - T);
	}
	Light->SetIntensity(LightIntensity * (1.f - T));
	if (T >= 1.f)
	{
		Sphere->SetVisibility(false); // the log (a child with absolute scale) keeps its own visibility
		Light->SetVisibility(false);
		SetActorTickEnabled(false);
	}
}
