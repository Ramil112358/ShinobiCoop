#include "Shinobi/VFX/ShinobiBurst.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

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

	Cone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cone"));
	Cone->SetupAttachment(Sphere);
	Cone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cone->SetCastShadow(false);
	Cone->SetUsingAbsoluteScale(true); // sized in Tick, independent of the (hidden) sphere
	Cone->SetRelativeRotation(FRotator(90.f, 0.f, 0.f)); // the engine cone's tip (+Z) turned back toward the actor
	Cone->SetVisibility(false);
	if (ConeMesh.Succeeded())
	{
		Cone->SetStaticMesh(ConeMesh.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Sphere);
	Light->SetCastShadows(false);
	Light->SetSpecularScale(0.f);
	Light->SetVisibility(false);
}

AShinobiBurst* AShinobiBurst::Spawn(UWorld* World, EShinobiBurst Kind, const FVector& Location, float Scale, const FRotator& Rotation)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FTransform Where(Rotation, Location);
	AShinobiBurst* Burst = World->SpawnActorDeferred<AShinobiBurst>(AShinobiBurst::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Burst)
	{
		Burst->Kind = Kind;
		Burst->Scale = Scale;
		Burst->FinishSpawning(Where);
	}
	return Burst;
}

void AShinobiBurst::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiBurst, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiBurst, Scale, COND_InitialOnly);
}

void AShinobiBurst::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		SetActorTickEnabled(false);
		return;
	}
	static const EShinobiSfx Sounds[] = { EShinobiSfx::Explosion, EShinobiSfx::Explosion, EShinobiSfx::Zap, EShinobiSfx::Whirr, EShinobiSfx::Poof, EShinobiSfx::Whoosh, EShinobiSfx::Whoosh };
	ShinobiSfx::Play(this, Sounds[static_cast<uint8>(Kind)], GetActorLocation(), Kind == EShinobiBurst::Fire ? 0.7f : 1.f);

	// Mostly-rim shells (low Core) at modest intensity: a solid bright sphere blows out to a white wall under bloom.
	FLinearColor Color = ShinobiVfx::Colors::Fire;
	float Core = 0.3f;
	switch (Kind)
	{
	case EShinobiBurst::Fire:         Color = ShinobiVfx::Colors::Fire;      Duration = 0.4f; MaxScale = 3.f;  Intensity = 4.f;   LightIntensity = 20000.f; break;
	case EShinobiBurst::Firestorm:    Color = ShinobiVfx::Colors::Fire;      Duration = 0.6f; MaxScale = 10.f; Intensity = 2.5f;  LightIntensity = 60000.f; Core = 0.15f; break;
	case EShinobiBurst::Lightning:    Color = ShinobiVfx::Colors::Lightning; Duration = 0.3f; MaxScale = 1.8f; Intensity = 4.f;   LightIntensity = 30000.f; break;
	case EShinobiBurst::Spiral:       Color = ShinobiVfx::Colors::Chakra;    Duration = 0.4f; MaxScale = 3.f;  Intensity = 4.f;   LightIntensity = 20000.f; break;
	case EShinobiBurst::Substitution: Color = ShinobiVfx::Colors::Smoke;     Duration = 0.5f; MaxScale = 2.2f; Intensity = 0.8f;  LightIntensity = 0.f;     Core = 0.6f; break;
	case EShinobiBurst::FireBreath:   Color = ShinobiVfx::Colors::Fire;      Duration = 0.4f; MaxScale = 2.2f; Intensity = 5.f;   LightIntensity = 15000.f; break;
	case EShinobiBurst::BlackFlameIgnite: Color = ShinobiVfx::Colors::BlackFlame; Duration = 0.5f; MaxScale = 2.5f; Intensity = 3.f; LightIntensity = 15000.f; Core = 0.5f; break;
	}
	MaxScale *= Scale;
	SphereMaterial = Kind == EShinobiBurst::FireBreath || Kind == EShinobiBurst::BlackFlameIgnite
		? ShinobiVfx::MakeFlame(this, Color, Intensity, Kind == EShinobiBurst::BlackFlameIgnite)
		: ShinobiVfx::MakeGlow(this, Color, Intensity, Core);
	Sphere->SetMaterial(0, SphereMaterial);
	if (Kind == EShinobiBurst::FireBreath)
	{
		Sphere->SetVisibility(false, false); // the cone is the whole effect; the light stays
		Sphere->SetRelativeScale3D(FVector::OneVector); // unscaled root, so the cone's offset is in plain units
		Cone->SetMaterial(0, SphereMaterial);
		Cone->SetVisibility(true);
	}
	else
	{
		Sphere->SetRelativeScale3D(FVector(0.2f * MaxScale));
	}
	if (BuildCards())
	{
		Sphere->SetVisibility(false, false);
		Cone->SetVisibility(false);
	}
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

bool AShinobiBurst::BuildCards()
{
	using ShinobiVfx::ESprite;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Plane || Kind == EShinobiBurst::Substitution)
	{
		return false;
	}
	FRandomStream Random(GetUniqueID()); // cosmetic: each machine may scatter the cards differently
	const FLinearColor White = FLinearColor::White;
	const float S = Scale;
	// Offsets are on the ground plane around the burst; Size is width x height of the quad.
	const auto Around = [&](float Radius, float Angle) { return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius; };
	switch (Kind)
	{
	case EShinobiBurst::Fire:
		Duration = 0.7f;
		for (int32 Index = 0; Index < 7; ++Index)
		{
			AddCard(Plane, ESprite::Flame, White, 1.2f, Around(Random.FRandRange(0.f, 80.f * S), Random.FRandRange(0.f, 2.f * PI)), FVector2D(130.f, 260.f) * S, 60.f * S, false, true);
		}
		AddCard(Plane, ESprite::Burst, FLinearColor(1.f, 0.7f, 0.3f), 1.5f, FVector::ZeroVector, FVector2D(420.f, 420.f) * S);
		AddCard(Plane, ESprite::Ember, White, 2.f, FVector::ZeroVector, FVector2D(260.f, 520.f) * S, 80.f * S);
		break;
	case EShinobiBurst::Firestorm:
		Duration = 0.9f;
		for (int32 Index = 0; Index < 14; ++Index)
		{
			AddCard(Plane, ESprite::Flame, White, 1.2f, Around(Random.FRandRange(60.f, 380.f) * S, Random.FRandRange(0.f, 2.f * PI)), FVector2D(320.f, 640.f) * S, 150.f * S, false, true);
		}
		AddCard(Plane, ESprite::Ring, FLinearColor(1.f, 0.55f, 0.2f), 1.5f, FVector::ZeroVector, FVector2D(1100.f, 1100.f) * S, 0.f, true);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			AddCard(Plane, ESprite::Ember, White, 2.f, Around(150.f * S, Index * 2.1f), FVector2D(500.f, 1000.f) * S, 200.f * S, false, true);
		}
		break;
	case EShinobiBurst::Lightning:
		Duration = 0.45f;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			AddCard(Plane, ESprite::Lightning, White, 2.5f, Around(Random.FRandRange(0.f, 70.f * S), Random.FRandRange(0.f, 2.f * PI)), FVector2D(170.f, 400.f) * S, 0.f, false, true);
		}
		AddCard(Plane, ESprite::Burst, FLinearColor(0.5f, 0.6f, 1.f), 1.5f, FVector::ZeroVector, FVector2D(380.f, 380.f) * S);
		AddCard(Plane, ESprite::Ring, FLinearColor(0.5f, 0.6f, 1.f), 1.5f, FVector::ZeroVector, FVector2D(520.f, 520.f) * S, 0.f, true);
		break;
	case EShinobiBurst::Spiral:
		Duration = 0.55f;
		AddCard(Plane, ESprite::Vortex, White, 2.f, FVector::ZeroVector, FVector2D(420.f, 420.f) * S, 0.f, true);
		AddCard(Plane, ESprite::Ring, FLinearColor(0.5f, 0.85f, 1.f), 1.8f, FVector::ZeroVector, FVector2D(640.f, 640.f) * S, 0.f, true);
		AddCard(Plane, ESprite::Burst, FLinearColor(0.6f, 0.9f, 1.f), 1.5f, FVector::ZeroVector, FVector2D(460.f, 460.f) * S);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			AddCard(Plane, ESprite::Lightning, FLinearColor(0.5f, 0.85f, 1.f), 2.f, Around(Random.FRandRange(20.f, 90.f) * S, Random.FRandRange(0.f, 2.f * PI)), FVector2D(110.f, 260.f) * S, 0.f, false, true);
		}
		break;
	case EShinobiBurst::BlackFlameIgnite:
		Duration = 0.7f;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			AddCard(Plane, ESprite::BlackFlame, ShinobiVfx::Colors::BlackFlame, 4.f, Around(Random.FRandRange(0.f, 60.f * S), Random.FRandRange(0.f, 2.f * PI)), FVector2D(130.f, 260.f) * S, 50.f * S, false, true);
		}
		break;
	case EShinobiBurst::FireBreath:
		// A stream of flame from the mouth along the burst's forward, chasing the ball (2500 u/s): the far cards show later.
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const float Along = (40.f + Index * 70.f) * S;
			AddCard(Plane, ESprite::Flame, White, 1.3f, GetActorForwardVector() * Along, FVector2D(110.f + 28.f * Index, 220.f + 56.f * Index) * S, 0.f, false, true, Along / 2500.f);
		}
		break;
	default:
		return false;
	}
	return Cards.Num() > 0;
}

void AShinobiBurst::AddCard(UStaticMesh* Plane, ShinobiVfx::ESprite Sprite, const FLinearColor& Tint, float Glow, const FVector& Offset, const FVector2D& Size, float Rise, bool bFlat, bool bFlicker, float Delay)
{
	FCard Card;
	Card.Variant = FMath::RandRange(0, 7);
	Card.Material = ShinobiVfx::MakeSprite(this, Sprite, Card.Variant, Tint, Glow);
	Card.Mesh = NewObject<UStaticMeshComponent>(this);
	Card.Mesh->SetStaticMesh(Plane);
	Card.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Card.Mesh->SetCastShadow(false);
	Card.Mesh->SetMaterial(0, Card.Material);
	Card.Mesh->SetVisibility(false);
	Card.Mesh->SetupAttachment(RootComponent);
	Card.Mesh->SetUsingAbsoluteLocation(true); // the glow sphere under it grows; the cards are sized and placed in Tick
	Card.Mesh->SetUsingAbsoluteRotation(true);
	Card.Mesh->SetUsingAbsoluteScale(true);
	Card.Mesh->RegisterComponent();
	Card.Sprite = static_cast<uint8>(Sprite);
	Card.Offset = Offset;
	Card.Size = Size;
	Card.Rise = Rise;
	Card.Delay = Delay;
	Card.bFlat = bFlat;
	Card.bFlicker = bFlicker;
	Card.Spin = bFlat ? 240.f : 0.f;
	Cards.Add(Card);
}

void AShinobiBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Duration, 0.f, 1.f);
	const float Grow = 1.f - FMath::Square(1.f - T); // fast out, slowing down
	if (!Cone->IsVisible())
	{
		Sphere->SetRelativeScale3D(FVector(FMath::Lerp(0.2f, 1.f, Grow) * MaxScale));
	}
	else
	{
		// A stream from the mouth chasing the ball (2500 u/s) for a moment, widening as it goes, then fading. The engine
		// cone is 100 units long around its centre: shift it half its length forward so the tip stays at the mouth.
		const float Length = (30.f + FMath::Min(Age * 2500.f, 450.f * Scale)) / 100.f;
		const float Width = (0.15f + 0.12f * Length) * Scale;
		Cone->SetWorldScale3D(FVector(Width, Width, Length));
		Cone->SetRelativeLocation(FVector(50.f * Length, 0.f, 0.f));
	}
	if (SphereMaterial)
	{
		SphereMaterial->SetScalarParameterValue(TEXT("Fade"), 1.f - T);
	}
	if (Cards.Num() > 0)
	{
		const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
		const FVector CameraAt = Camera ? Camera->GetCameraLocation() : GetActorLocation() + FVector(-1000.f, 0.f, 0.f);
		for (FCard& Card : Cards)
		{
			const float Local = FMath::Clamp((Age - Card.Delay) / FMath::Max(Duration - Card.Delay, 0.05f), 0.f, 1.f);
			const bool bShown = Age >= Card.Delay && Local < 1.f;
			Card.Mesh->SetVisibility(bShown);
			if (!bShown)
			{
				continue;
			}
			const float CardGrow = 1.f - FMath::Square(1.f - Local);
			const FVector2D Size = Card.Size * FMath::Lerp(0.35f, 1.f, CardGrow);
			const FVector Origin = GetActorLocation() + Card.Offset;
			if (Card.bFlat)
			{
				Card.Mesh->SetWorldLocation(Origin + FVector(0.f, 0.f, 8.f));
				Card.Mesh->SetWorldRotation(FRotator(0.f, Card.Spin * Age, 0.f));
				Card.Mesh->SetWorldScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, 1.f));
			}
			else
			{
				const float FaceYaw = FMath::RadiansToDegrees(FMath::Atan2(CameraAt.Y - Origin.Y, CameraAt.X - Origin.X)) - 90.f;
				Card.Mesh->SetWorldLocation(Origin + FVector(0.f, 0.f, Size.Y * 0.5f + Card.Rise * Local));
				Card.Mesh->SetWorldRotation(FRotator(0.f, FaceYaw, 90.f));
				Card.Mesh->SetWorldScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, 1.f));
			}
			if (Card.bFlicker && Age >= Card.NextSwap)
			{
				const ShinobiVfx::ESprite Sprite = static_cast<ShinobiVfx::ESprite>(Card.Sprite);
				Card.Variant = (Card.Variant + 1 + FMath::RandRange(0, 2)) % ShinobiVfx::SpriteVariants(Sprite);
				ShinobiVfx::SetSpriteVariant(Card.Material, Sprite, Card.Variant);
				Card.NextSwap = Age + 0.06f * FMath::FRandRange(0.7f, 1.3f);
			}
			if (Card.Material)
			{
				Card.Material->SetScalarParameterValue(TEXT("Fade"), FMath::Min(Local * 10.f, 1.f) * FMath::Square(1.f - Local));
			}
		}
	}
	Light->SetIntensity(LightIntensity * (1.f - T));
	if (T >= 1.f)
	{
		Sphere->SetVisibility(false); // the log (a child with absolute scale) keeps its own visibility
		Cone->SetVisibility(false);
		for (FCard& Card : Cards)
		{
			Card.Mesh->SetVisibility(false);
		}
		Light->SetVisibility(false);
		SetActorTickEnabled(false);
	}
}
