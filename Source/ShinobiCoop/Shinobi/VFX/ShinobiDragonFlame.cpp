#include "Shinobi/VFX/ShinobiDragonFlame.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiDragonMount.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr float FlameFadeOutSeconds = 0.7f; // the last flames still in the air when the breath stops
	constexpr float FlameLightIntensity = 60000.f;
}

AShinobiDragonFlame::AShinobiDragonFlame()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // the dragon's head is posed by then
	bReplicates = true;
	SetReplicatingMovement(true); // the server turns it with the aim
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(GetRootComponent());
	Light->SetLightColor(FLinearColor(0.45f, 0.65f, 1.f));
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(1800.f);
	Light->SetCastShadows(false);
}

void AShinobiDragonFlame::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiDragonFlame, Rider, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiDragonFlame, Kind, COND_InitialOnly);
	DOREPLIFETIME(AShinobiDragonFlame, bStopped);
}

AShinobiDragonFlame* AShinobiDragonFlame::Spawn(AShinobiCharacterBase* Rider, EShinobiDragonKind Kind)
{
	UWorld* World = Rider ? Rider->GetWorld() : nullptr;
	if (!World || !Rider->HasAuthority())
	{
		return nullptr;
	}
	const FTransform Where(AShinobiDragonMount::BreathDirection(*Rider).Rotation(), AShinobiDragonMount::MouthLocation(*Rider, Kind));
	AShinobiDragonFlame* Flame = World->SpawnActorDeferred<AShinobiDragonFlame>(StaticClass(), Where, Rider, Rider, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Flame)
	{
		Flame->Rider = Rider;
		Flame->Kind = Kind;
		Flame->FinishSpawning(Where);
	}
	return Flame;
}

void AShinobiDragonFlame::Stop()
{
	if (HasAuthority() && StoppedAt < 0.f)
	{
		StoppedAt = Age;
		bStopped = true;
		SetLifeSpan(FlameFadeOutSeconds + 0.2f);
	}
}

void AShinobiDragonFlame::OnRep_Stopped()
{
	if (StoppedAt < 0.f)
	{
		StoppedAt = Age;
	}
}

void AShinobiDragonFlame::AddParticle(UStaticMesh* Plane, uint8 Sprite, const FLinearColor& Tint, float Glow, float Phase, float Life, float Width, float Spread, float ReachFraction)
{
	FParticle Particle;
	Particle.Sprite = Sprite;
	Particle.Variant = FMath::RandRange(0, 7);
	Particle.Material = ShinobiVfx::MakeSprite(this, static_cast<ShinobiVfx::ESprite>(Sprite), Particle.Variant, Tint, Glow);
	Particle.Mesh = NewObject<UStaticMeshComponent>(this);
	Particle.Mesh->SetStaticMesh(Plane);
	Particle.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Particle.Mesh->SetCastShadow(false);
	Particle.Mesh->SetMaterial(0, Particle.Material);
	Particle.Mesh->SetVisibility(false);
	Particle.Mesh->SetupAttachment(GetRootComponent());
	Particle.Mesh->SetUsingAbsoluteLocation(true);
	Particle.Mesh->SetUsingAbsoluteRotation(true);
	Particle.Mesh->SetUsingAbsoluteScale(true);
	Particle.Mesh->RegisterComponent();
	Particle.Phase = Phase;
	Particle.Life = Life;
	Particle.Width = Width;
	Particle.Spread = Spread;
	Particle.Reach = ReachFraction;
	Particle.Side = FVector(0.f, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal() * FMath::FRandRange(0.3f, 1.f);
	Particles.Add(Particle);
}

void AShinobiDragonFlame::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		SetActorTickEnabled(HasAuthority()); // only the server's following of the aim is needed
		return;
	}
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Plane)
	{
		return;
	}
	using ShinobiVfx::ESprite;
	const bool bTairn = Kind == EShinobiDragonKind::Tairn;
	const FLinearColor White(1.f, 1.f, 1.f);
	Light->SetLightColor(bTairn ? FLinearColor(0.45f, 0.65f, 1.f) : FLinearColor(1.f, 0.55f, 0.2f));
	// Each dragon has its own SpriteCook sheet of tall flame jets (T_TairnFire: blue-white plasma flame laced with lightning; T_CrimsonFire:
	// orange dragonfire with flying embers), flame at the bottom of each cell and the tip at the top. A tongue is born at the mouth and flies
	// down the jet with its base leading... its long axis along the fire, widening as it goes, so the overlapping tongues read as one stream
	// that starts at the mouth and goes straight out in a narrow cone.
	const uint8 Fire = static_cast<uint8>(bTairn ? ESprite::TairnFire : ESprite::CrimsonFire);
	for (int32 Index = 0; Index < 44; ++Index)
	{
		AddParticle(Plane, Fire, White, 2.4f, Index / 44.f, FMath::FRandRange(0.5f, 0.7f), FMath::FRandRange(300.f, 440.f), 0.05f, FMath::FRandRange(0.88f, 1.f));
	}
	// A hotter, narrower core near the mouth.
	for (int32 Index = 0; Index < 12; ++Index)
	{
		AddParticle(Plane, Fire, FLinearColor(1.4f, 1.4f, 1.4f), 3.2f, Index / 12.f, 0.3f, FMath::FRandRange(120.f, 190.f), 0.02f, 0.5f);
	}
	if (bTairn)
	{
		// Bolts of lightning forking along the jet.
		for (int32 Index = 0; Index < 12; ++Index)
		{
			AddParticle(Plane, static_cast<uint8>(ESprite::Lightning), FLinearColor(0.8f, 0.9f, 1.f), 3.5f, Index / 12.f, FMath::FRandRange(0.16f, 0.3f), FMath::FRandRange(160.f, 260.f), 0.06f, FMath::FRandRange(0.6f, 1.f));
		}
	}
	else
	{
		for (int32 Index = 0; Index < 10; ++Index)
		{
			AddParticle(Plane, static_cast<uint8>(ESprite::Ember), White, 2.f, Index / 10.f, FMath::FRandRange(0.6f, 0.9f), FMath::FRandRange(120.f, 200.f), 0.08f, 1.1f);
		}
		for (int32 Index = 0; Index < 6; ++Index)
		{
			AddParticle(Plane, static_cast<uint8>(ESprite::Smoke), FLinearColor(0.25f, 0.2f, 0.18f), 1.f, Index / 6.f, 0.9f, FMath::FRandRange(380.f, 560.f), 0.07f, 1.05f);
		}
	}
}

void AShinobiDragonFlame::FollowRider()
{
	if (IsValid(Rider))
	{
		SetActorLocationAndRotation(AShinobiDragonMount::MouthLocation(*Rider, Kind), AShinobiDragonMount::BreathDirection(*Rider).Rotation());
	}
	else if (HasAuthority())
	{
		Destroy();
	}
}

void AShinobiDragonFlame::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (HasAuthority() && StoppedAt < 0.f)
	{
		FollowRider(); // the stopped jet stays where it was and dies away
	}
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	const FVector Forward = GetActorForwardVector();
	if (!Dragon.IsValid() && IsValid(Rider))
	{
		Dragon = AShinobiDragonMount::Find(*Rider);
	}
	if (StoppedAt < 0.f || MouthAt.IsZero())
	{
		MouthAt = Dragon.IsValid() ? Dragon->GetMouthLocation() : GetActorLocation(); // a stopped jet dies away where it was
	}
	const FVector Mouth = MouthAt;
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraAt = Camera ? Camera->GetCameraLocation() : Mouth - Forward * 1000.f;
	const float Dying = StoppedAt >= 0.f ? FMath::Clamp(1.f - (Age - StoppedAt) / FlameFadeOutSeconds, 0.f, 1.f) : 1.f;

	for (FParticle& Particle : Particles)
	{
		const float Cycle = FMath::Fmod(Age / Particle.Life + Particle.Phase, 1.f); // 0 at the mouth, 1 at the end of its flight
		const float Distance = Reach * Particle.Reach * FMath::Pow(Cycle, 0.7f);
		const FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, Forward);
		const FVector Drift = (Right * Particle.Side.Y + Up * Particle.Side.Z) * (Particle.Spread * Distance);
		const FVector At = Mouth + Forward * Distance + Drift;

		const float Width = Particle.Width * FMath::Lerp(0.3f, 1.f, FMath::Sqrt(Cycle));
		// A billboard that turns about the jet's own axis to face the camera: its long axis stays along the fire, however you view it.
		const FVector ToCamera = (CameraAt - At).GetSafeNormal();
		FVector Across = FVector::CrossProduct(Forward, ToCamera).GetSafeNormal();
		if (Across.IsNearlyZero())
		{
			Across = Right; // looking straight down the jet
		}
		const FVector Normal = FVector::CrossProduct(Across, Forward);
		const FMatrix Basis(Across, Forward, Normal, FVector::ZeroVector);
		const uint8 Kind8 = Particle.Sprite;
		const bool bBolt = Kind8 == static_cast<uint8>(ShinobiVfx::ESprite::Lightning);
		const bool bTongue = Kind8 == static_cast<uint8>(ShinobiVfx::ESprite::TairnFire) || Kind8 == static_cast<uint8>(ShinobiVfx::ESprite::CrimsonFire) || Kind8 == static_cast<uint8>(ShinobiVfx::ESprite::Ember);
		const float Length = Width * (bBolt ? 3.2f : bTongue ? 2.2f : 1.f);

		Particle.Mesh->SetVisibility(Dying > 0.f);
		// The sprites' flame is at the bottom of the cell: the base sits at the particle and the tip points down the jet.
		Particle.Mesh->SetWorldLocationAndRotation(At + Forward * (bTongue || bBolt ? Length * 0.5f : 0.f), Basis.Rotator());
		Particle.Mesh->SetWorldScale3D(FVector(Width / 100.f, Length / 100.f, 1.f));
		if (Particle.Material)
		{
			const float Born = FMath::Min(Cycle * 8.f, 1.f);
			Particle.Material->SetScalarParameterValue(TEXT("Fade"), Born * FMath::Pow(1.f - Cycle, (Particle.Sprite == static_cast<uint8>(ShinobiVfx::ESprite::TairnFire) || Particle.Sprite == static_cast<uint8>(ShinobiVfx::ESprite::CrimsonFire)) ? 0.5f : 1.2f) * Dying);
		}
		if (Age >= Particle.NextSwap && Particle.Sprite != static_cast<uint8>(ShinobiVfx::ESprite::Burst))
		{
			const ShinobiVfx::ESprite Sprite = static_cast<ShinobiVfx::ESprite>(Particle.Sprite);
			Particle.Variant = (Particle.Variant + 1 + FMath::RandRange(0, 2)) % ShinobiVfx::SpriteVariants(Sprite);
			ShinobiVfx::SetSpriteVariant(Particle.Material, Sprite, Particle.Variant);
			Particle.NextSwap = Age + 0.07f * FMath::FRandRange(0.7f, 1.3f);
		}
	}
	// The light rides a third of the way down the jet and gutters.
	Light->SetWorldLocation(Mouth + Forward * (Reach * 0.3f));
	Light->SetIntensity(FlameLightIntensity * Dying * FMath::FRandRange(0.8f, 1.2f));
}
