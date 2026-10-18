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

namespace
{
	constexpr float SkyBoltHeight = 1500.f; // how far above its spot the owl's bolt starts
	constexpr int32 SkyBoltCards = 4;
	// Almighty Push: the dome's full radius at Scale 1 (UGA_AlmightyPush::Radius), how long it takes to swell to it,
	// when it starts to fade and when it is gone; the dust lingers to the end of the burst.
	constexpr float PushRadius = 600.f;
	constexpr float PushSwell = 0.4f;
	constexpr float PushFadeStart = 0.8f;
	constexpr float PushFadeEnd = 1.4f;
	float PushSwellAt(float Age) { return 1.f - FMath::Cube(1.f - FMath::Min(Age / PushSwell, 1.f)); } // fast out, easing to a stop
}

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
	static const EShinobiSfx Sounds[] = { EShinobiSfx::Explosion, EShinobiSfx::Explosion, EShinobiSfx::Zap, EShinobiSfx::Whirr, EShinobiSfx::Poof, EShinobiSfx::Whoosh, EShinobiSfx::Whoosh,
		EShinobiSfx::Explosion, EShinobiSfx::Whirr, EShinobiSfx::Zap, EShinobiSfx::Thud, EShinobiSfx::Poof,
		EShinobiSfx::Explosion, EShinobiSfx::Poof, EShinobiSfx::Whoosh,
		EShinobiSfx::Whirr, EShinobiSfx::Poof, EShinobiSfx::Zap, EShinobiSfx::Explosion };
	static_assert(UE_ARRAY_COUNT(Sounds) == static_cast<int32>(EShinobiBurst::AlmightyPush) + 1, "a sound for every burst kind");
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
	case EShinobiBurst::Shockwave:    Color = ShinobiVfx::Colors::Smoke;     Duration = 0.5f; MaxScale = 6.f;  Intensity = 1.5f;  LightIntensity = 0.f;     Core = 0.1f; break;
	case EShinobiBurst::Swirl:        Color = ShinobiVfx::Colors::Chakra;    Duration = 0.5f; MaxScale = 2.f;  Intensity = 3.f;   LightIntensity = 10000.f; Core = 0.4f; break;
	case EShinobiBurst::TeleportFlash: Color = ShinobiVfx::Colors::Lightning; Duration = 0.3f; MaxScale = 2.5f; Intensity = 4.f;   LightIntensity = 30000.f; break;
	case EShinobiBurst::Crumble:      Color = FLinearColor(0.35f, 0.22f, 0.1f); Duration = 0.5f; MaxScale = 3.f; Intensity = 1.f;  LightIntensity = 0.f;     Core = 0.5f; break;
	case EShinobiBurst::Splash:       Color = ShinobiVfx::Colors::Chakra;    Duration = 0.5f; MaxScale = 3.f;  Intensity = 2.f;   LightIntensity = 0.f;     Core = 0.4f; break;
	case EShinobiBurst::MeteorImpact: Color = ShinobiVfx::Colors::Fire;      Duration = 0.8f; MaxScale = 8.f;  Intensity = 3.f;   LightIntensity = 60000.f; Core = 0.2f; break;
	case EShinobiBurst::FrostImpact:  Color = ShinobiVfx::Colors::Ice;       Duration = 0.5f; MaxScale = 2.f;  Intensity = 3.f;   LightIntensity = 12000.f; Core = 0.4f; break;
	case EShinobiBurst::SusanooRise:  Color = FLinearColor(0.1f, 0.45f, 1.f); Duration = 0.9f; MaxScale = 6.f; Intensity = 2.5f; LightIntensity = 40000.f; Core = 0.3f; break;
	case EShinobiBurst::SummonPoof:   Color = ShinobiVfx::Colors::Smoke;     Duration = 0.7f; MaxScale = 4.f;  Intensity = 0.8f;  LightIntensity = 0.f;     Core = 0.6f; break;
	case EShinobiBurst::SkyBolt:      Color = ShinobiVfx::Colors::Lightning; Duration = 0.3f; MaxScale = 1.5f; Intensity = 4.f;   LightIntensity = 40000.f; break;
	case EShinobiBurst::DragonSplash: Color = ShinobiVfx::Colors::Chakra;    Duration = 0.7f; MaxScale = 5.f;  Intensity = 2.f;   LightIntensity = 10000.f; Core = 0.4f; break;
	case EShinobiBurst::AlmightyPush: Color = ShinobiVfx::Colors::Smoke;     Duration = 2.f;  MaxScale = 1.f;  Intensity = 1.f;   LightIntensity = 0.f;     break;
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
	// The ground takes the hit: a cracked dent (Chidori) or a spiral crater (Rasengan) and chunks of rock thrown up.
	// ponytail: the marks are unlit; the tint darkens their dirt to sit on daylit ground. A lit decal if night maps need it.
	const FLinearColor DirtTint(0.12f, 0.12f, 0.12f);
	if (Kind == EShinobiBurst::Lightning)
	{
		ShinobiVfx::SpawnScorch(GetWorld(), GetActorLocation(), 70.f * Scale, DirtTint, 10.f, ShinobiVfx::ChidoriDent);
		ShinobiVfx::SpawnDebris(GetWorld(), GetActorLocation(), 6, 320.f, 0.12f, 4.f);
	}
	else if (Kind == EShinobiBurst::Spiral)
	{
		ShinobiVfx::SpawnScorch(GetWorld(), GetActorLocation(), 130.f * Scale, DirtTint, 12.f, ShinobiVfx::RasenganCrater);
		ShinobiVfx::SpawnDebris(GetWorld(), GetActorLocation(), 12, 500.f * FMath::Sqrt(Scale), 0.16f, 5.f);
	}
	else if (Kind == EShinobiBurst::AlmightyPush)
	{
		// A milky sphere of force swelling out from the caster (its lower part under the floor), a ring of cracks where
		// its edge meets the ground, and grit thrown out.
		Dome = NewObject<UStaticMeshComponent>(this);
		Dome->SetStaticMesh(Sphere->GetStaticMesh());
		Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Dome->SetCastShadow(false);
		Dome->SetupAttachment(RootComponent);
		Dome->SetUsingAbsoluteScale(true);
		Dome->SetRelativeLocation(FVector(0.f, 0.f, 0.2f * PushRadius * Scale));
		DomeMaterial = ShinobiVfx::MakePushDome(this);
		Dome->SetMaterial(0, DomeMaterial);
		Dome->RegisterComponent();
		const float Edge = PushRadius * Scale * FMath::Sqrt(1.f - FMath::Square(0.2f)); // where the sphere meets the floor
		ShinobiVfx::SpawnScorch(GetWorld(), GetActorLocation(), Edge / 0.77f, FLinearColor(0.3f, 0.3f, 0.3f), 10.f, ShinobiVfx::PushCrackRing);
		ShinobiVfx::SpawnDebris(GetWorld(), GetActorLocation(), 6, 700.f * FMath::Sqrt(Scale), 0.07f, 4.f);
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
	case EShinobiBurst::SkyBolt:
		// Chidori-blue bolt cards stacked from SkyBoltHeight down to the ground, each a moment after the one above, so the
		// bolt falls rather than grows up; a narrower white core down the middle; the Lightning flash where it lands.
		Duration = 0.45f;
		for (int32 Index = 0; Index < SkyBoltCards; ++Index)
		{
			const float Bottom = SkyBoltHeight * (SkyBoltCards - 1 - Index) / SkyBoltCards;
			const float Delay = 0.025f * Index;
			const FVector Jitter = Around(Random.FRandRange(0.f, 25.f), Random.FRandRange(0.f, 2.f * PI));
			AddCard(Plane, ESprite::Lightning, FLinearColor(0.45f, 0.65f, 1.f), 3.f, Jitter + FVector(0.f, 0.f, Bottom), FVector2D(260.f, SkyBoltHeight / SkyBoltCards + 40.f), 0.f, false, true, Delay);
			AddCard(Plane, ESprite::Lightning, White, 2.5f, Jitter + FVector(0.f, 0.f, Bottom), FVector2D(110.f, SkyBoltHeight / SkyBoltCards + 40.f), 0.f, false, true, Delay);
		}
		AddCard(Plane, ESprite::Burst, FLinearColor(0.5f, 0.6f, 1.f), 1.5f, FVector::ZeroVector, FVector2D(380.f, 380.f) * S, 0.f, false, false, 0.025f * SkyBoltCards);
		AddCard(Plane, ESprite::Ring, FLinearColor(0.5f, 0.6f, 1.f), 1.5f, FVector::ZeroVector, FVector2D(520.f, 520.f) * S, 0.f, true, false, 0.025f * SkyBoltCards);
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
		// Dust kicked up round the crater; the burst sits at the victim's middle, so drop it to the feet.
		for (int32 Index = 0; Index < 4; ++Index)
		{
			AddCard(Plane, ESprite::Smoke, FLinearColor(0.62f, 0.55f, 0.46f), 1.f, Around(Random.FRandRange(80.f, 200.f) * S, Index * 1.57f + Random.FRandRange(0.f, 0.5f)) - FVector(0.f, 0.f, 80.f), FVector2D(260.f, 300.f) * S, 90.f * S);
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
	case EShinobiBurst::Shockwave:
		// Almighty Push: a ring of force rolling out along the ground and dust thrown up around the caster.
		Duration = 0.7f;
		AddCard(Plane, ESprite::Ring, White, 1.5f, FVector::ZeroVector, FVector2D(1300.f, 1300.f) * S, 0.f, true);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			AddCard(Plane, ESprite::Smoke, FLinearColor(0.72f, 0.66f, 0.58f), 1.f, Around(Random.FRandRange(150.f, 320.f) * S, Index * 1.26f + Random.FRandRange(0.f, 0.5f)), FVector2D(300.f, 380.f) * S, 90.f * S);
		}
		break;
	case EShinobiBurst::AlmightyPush:
	{
		// Dust churning inside the dome while it holds, a bank of dust rolling out along the floor under its edge, and
		// clouds left hanging round where it stopped, all low on the ground: the top of the dome stays clear. (The dome
		// itself is a mesh, built in BeginPlay.)
		const float R = PushRadius * S;
		const FLinearColor Dust(0.62f, 0.58f, 0.52f);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			AddCard(Plane, ESprite::Dust, FLinearColor(0.6f, 0.59f, 0.57f), 1.f, Around(Random.FRandRange(0.3f, 0.6f) * R, Index * 1.57f + Random.FRandRange(0.f, 0.5f)), FVector2D(0.6f, 0.25f) * R, 0.f, false, false, 0.05f);
			Cards.Last().Life = PushFadeEnd;
			Cards.Last().bRollOut = true;
		}
		for (int32 Index = 0; Index < 16; ++Index)
		{
			AddCard(Plane, ESprite::Dust, Dust, 1.f, Around(Random.FRandRange(0.85f, 1.f) * R, Index * 2.f * PI / 16.f + Random.FRandRange(-0.15f, 0.15f)), FVector2D(0.65f, 0.3f) * R, 0.05f * R);
			Cards.Last().Life = 1.2f;
			Cards.Last().bRollOut = true;
		}
		for (int32 Index = 0; Index < 10; ++Index)
		{
			AddCard(Plane, ESprite::Dust, Dust * 0.9f, 1.f, Around(Random.FRandRange(0.95f, 1.2f) * R, Index * 2.f * PI / 10.f + Random.FRandRange(0.f, 0.5f)), FVector2D(0.8f, 0.35f) * R, 0.08f * R, false, false, PushSwell);
		}
		break;
	}
	case EShinobiBurst::TeleportFlash:
		// Flying Raijin: a bright flash and a few bolts where the caster lands.
		Duration = 0.45f;
		AddCard(Plane, ESprite::Burst, FLinearColor(1.f, 0.95f, 0.6f), 2.f, FVector(0.f, 0.f, 20.f), FVector2D(440.f, 440.f) * S);
		AddCard(Plane, ESprite::Ring, FLinearColor(1.f, 0.9f, 0.5f), 1.6f, FVector::ZeroVector, FVector2D(480.f, 480.f) * S, 0.f, true);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			AddCard(Plane, ESprite::Lightning, White, 2.5f, Around(Random.FRandRange(10.f, 60.f) * S, Random.FRandRange(0.f, 2.f * PI)), FVector2D(110.f, 270.f) * S, 0.f, false, true);
		}
		break;
	case EShinobiBurst::Crumble:
		// Earth Wall breaking: dust and grit along the wall's width (the burst is spawned with the wall's rotation).
		Duration = 0.8f;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			AddCard(Plane, ESprite::Smoke, FLinearColor(0.38f, 0.28f, 0.18f), 1.f, GetActorRightVector() * (Index - 2) * 90.f * S, FVector2D(240.f, 320.f) * S, 110.f * S);
		}
		AddCard(Plane, ESprite::Ember, FLinearColor(0.5f, 0.38f, 0.25f), 1.f, FVector::ZeroVector, FVector2D(300.f, 500.f) * S, 80.f * S);
		break;
	case EShinobiBurst::Splash:
		// Water Wall breaking: a ring of spray and a burst of light.
		Duration = 0.6f;
		AddCard(Plane, ESprite::Ring, FLinearColor(0.4f, 0.7f, 1.f), 1.5f, FVector::ZeroVector, FVector2D(700.f, 700.f) * S, 0.f, true);
		AddCard(Plane, ESprite::Burst, FLinearColor(0.5f, 0.8f, 1.f), 1.2f, FVector(0.f, 0.f, 80.f), FVector2D(380.f, 380.f) * S);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			AddCard(Plane, ESprite::Ember, FLinearColor(0.5f, 0.8f, 1.f), 1.5f, GetActorRightVector() * (Index ? 120.f : -120.f) * S, FVector2D(260.f, 440.f) * S, 90.f * S, false, true);
		}
		break;
	case EShinobiBurst::MeteorImpact:
		// Fire Meteor: a white-hot flash, a ground ring, a crown of flames and a spray of embers.
		Duration = 0.9f;
		AddCard(Plane, ESprite::Burst, FLinearColor(1.f, 0.75f, 0.35f), 1.8f, FVector(0.f, 0.f, 40.f), FVector2D(700.f, 700.f) * S);
		AddCard(Plane, ESprite::Ring, FLinearColor(1.f, 0.55f, 0.2f), 1.6f, FVector::ZeroVector, FVector2D(1500.f, 1500.f) * S, 0.f, true);
		for (int32 Index = 0; Index < 9; ++Index)
		{
			AddCard(Plane, ESprite::Flame, White, 1.2f, Around(Random.FRandRange(60.f, 260.f) * S, Random.FRandRange(0.f, 2.f * PI)), FVector2D(220.f, 440.f) * S, 120.f * S, false, true);
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			AddCard(Plane, ESprite::Ember, White, 2.f, Around(120.f * S, Index * 2.1f), FVector2D(420.f, 840.f) * S, 180.f * S, false, true);
		}
		break;
	case EShinobiBurst::FrostImpact:
		// An ice shard landing: a pale flash, a small ring and a puff of frost.
		Duration = 0.6f;
		AddCard(Plane, ESprite::Burst, FLinearColor(0.7f, 0.9f, 1.f), 1.6f, FVector(0.f, 0.f, 20.f), FVector2D(300.f, 300.f) * S);
		AddCard(Plane, ESprite::Ring, FLinearColor(0.6f, 0.85f, 1.f), 1.6f, FVector::ZeroVector, FVector2D(420.f, 420.f) * S, 0.f, true);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			AddCard(Plane, ESprite::Smoke, FLinearColor(0.75f, 0.9f, 1.f), 1.f, Around(Random.FRandRange(20.f, 80.f) * S, Random.FRandRange(0.f, 2.f * PI)), FVector2D(160.f, 200.f) * S, 60.f * S);
		}
		break;
	case EShinobiBurst::SusanooRise:
		// Susanoo rising: a blue chakra spiral on the ground, a wide ring and a column of light.
		Duration = 1.0f;
		AddCard(Plane, ESprite::Vortex, FLinearColor(0.2f, 0.55f, 1.f), 2.f, FVector::ZeroVector, FVector2D(1100.f, 1100.f) * S, 0.f, true);
		Cards.Last().Spin = 240.f;
		AddCard(Plane, ESprite::Ring, FLinearColor(0.15f, 0.5f, 1.f), 1.8f, FVector::ZeroVector, FVector2D(1500.f, 1500.f) * S, 0.f, true);
		AddCard(Plane, ESprite::Burst, FLinearColor(0.3f, 0.7f, 1.f), 1.6f, FVector(0.f, 0.f, 220.f) * S, FVector2D(900.f, 900.f) * S); // a smaller burst (a guarded hit) keeps it on the body
		break;
	case EShinobiBurst::SummonPoof:
		// A summon arriving or leaving: a ring of smoke puffs billowing out from the ground.
		Duration = 0.9f;
		for (int32 Index = 0; Index < 7; ++Index)
		{
			AddCard(Plane, ESprite::Smoke, FLinearColor(0.78f, 0.76f, 0.72f), 1.f, Around(Random.FRandRange(120.f, 380.f) * S, Index * 0.9f + Random.FRandRange(0.f, 0.5f)), FVector2D(420.f, 520.f) * S, 110.f * S);
		}
		break;
	case EShinobiBurst::DragonSplash:
		// The Water Dragon breaking: a wide ring, a burst of spray and a crown of mist.
		Duration = 0.8f;
		AddCard(Plane, ESprite::Ring, FLinearColor(0.4f, 0.7f, 1.f), 1.6f, FVector::ZeroVector, FVector2D(900.f, 900.f) * S, 0.f, true);
		AddCard(Plane, ESprite::Burst, FLinearColor(0.5f, 0.8f, 1.f), 1.4f, FVector(0.f, 0.f, 60.f), FVector2D(520.f, 520.f) * S);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			AddCard(Plane, ESprite::Smoke, FLinearColor(0.8f, 0.92f, 1.f), 1.f, Around(Random.FRandRange(60.f, 200.f) * S, Random.FRandRange(0.f, 2.f * PI)), FVector2D(280.f, 340.f) * S, 140.f * S);
		}
		AddCard(Plane, ESprite::Ember, FLinearColor(0.55f, 0.8f, 1.f), 1.5f, FVector::ZeroVector, FVector2D(300.f, 520.f) * S, 120.f * S, false, true);
		break;
	case EShinobiBurst::Swirl:
		// Kamui: space twisting into a spiral out of the caster's eye (spawned at the eye), facing the camera. Two layers
		// turning against each other read as a warp rather than a flat decal.
		Duration = 0.45f;
		AddCard(Plane, ESprite::Vortex, FLinearColor(0.55f, 0.6f, 0.75f), 1.6f, FVector::ZeroVector, FVector2D(520.f, 520.f) * S);
		Cards.Last().Spin = -540.f;
		AddCard(Plane, ESprite::Vortex, FLinearColor(0.45f, 0.7f, 1.f), 2.2f, FVector::ZeroVector, FVector2D(300.f, 300.f) * S);
		Cards.Last().Spin = 900.f;
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
			const float Life = Card.Life > 0.f ? Card.Life : Duration - Card.Delay;
			const float Local = FMath::Clamp((Age - Card.Delay) / FMath::Max(Life, 0.05f), 0.f, 1.f);
			const bool bShown = Age >= Card.Delay && Local < 1.f;
			Card.Mesh->SetVisibility(bShown);
			if (!bShown)
			{
				continue;
			}
			const float CardGrow = 1.f - FMath::Square(1.f - Local);
			const FVector2D Size = Card.Size * FMath::Lerp(0.35f, 1.f, CardGrow);
			// Rolling out: from near the centre to the offset with the dome, then still drifting out a little.
			const FVector Origin = GetActorLocation() + Card.Offset * (Card.bRollOut ? FMath::Lerp(0.15f, 1.f, PushSwellAt(Age)) + 0.15f * Local : 1.f);
			if (Card.bFlat)
			{
				Card.Mesh->SetWorldLocation(Origin + FVector(0.f, 0.f, 8.f));
				Card.Mesh->SetWorldRotation(FRotator(0.f, Card.Spin * Age, 0.f));
				Card.Mesh->SetWorldScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, 1.f));
			}
			else if (Card.Spin != 0.f)
			{
				// Fully facing the camera, centred on the origin and turning in the view plane (Kamui's spiral).
				const FVector ToCamera = (CameraAt - Origin).GetSafeNormal();
				Card.Mesh->SetWorldLocation(Origin);
				Card.Mesh->SetWorldRotation(FQuat(ToCamera, FMath::DegreesToRadians(Card.Spin * Age)) * FRotationMatrix::MakeFromZ(ToCamera).ToQuat());
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
	if (Dome)
	{
		const float Radius = PushRadius * Scale * FMath::Lerp(0.08f, 1.f, PushSwellAt(Age));
		Dome->SetWorldScale3D(FVector(Radius / 50.f)); // the engine sphere is 100 units across
		Dome->SetVisibility(Age < PushFadeEnd);
		if (DomeMaterial)
		{
			DomeMaterial->SetScalarParameterValue(TEXT("Fade"), 1.f - FMath::SmoothStep(PushFadeStart, PushFadeEnd, Age));
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
