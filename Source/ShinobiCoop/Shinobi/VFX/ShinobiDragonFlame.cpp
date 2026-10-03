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
	DOREPLIFETIME(AShinobiDragonFlame, bStopped);
}

AShinobiDragonFlame* AShinobiDragonFlame::Spawn(AShinobiCharacterBase* Rider)
{
	UWorld* World = Rider ? Rider->GetWorld() : nullptr;
	if (!World || !Rider->HasAuthority())
	{
		return nullptr;
	}
	const FTransform Where(Rider->GetBaseAimRotation(), AShinobiDragonMount::MouthLocation(*Rider));
	AShinobiDragonFlame* Flame = World->SpawnActorDeferred<AShinobiDragonFlame>(StaticClass(), Where, Rider, Rider, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Flame)
	{
		Flame->Rider = Rider;
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
	const FLinearColor White(1.f, 1.f, 1.f);
	// Tairn's fire is blue-white and crackles with lightning (he is the dragon Violet wields lightning through). The stock flame sheet is
	// orange and a tint only darkens it, so the blue comes from two other sheets: BlackFlame, whose tint is its glow colour, gives the
	// flames their shape and ragged edges; Vortex, an additive energy swirl, gives them their bright blue-white body; Lightning forks
	// through both.
	for (int32 Index = 0; Index < 20; ++Index)
	{
		AddParticle(Plane, static_cast<uint8>(ESprite::BlackFlame), FLinearColor(0.3f, 0.55f, 1.f), 4.f, Index / 20.f, FMath::FRandRange(0.5f, 0.7f), FMath::FRandRange(260.f, 420.f), 0.2f, FMath::FRandRange(0.85f, 1.f));
	}
	for (int32 Index = 0; Index < 26; ++Index)
	{
		AddParticle(Plane, static_cast<uint8>(ESprite::Vortex), White, 3.2f, Index / 26.f, FMath::FRandRange(0.45f, 0.65f), FMath::FRandRange(220.f, 380.f), 0.2f, FMath::FRandRange(0.85f, 1.f));
	}
	// The hot core and the flash at the mouth.
	for (int32 Index = 0; Index < 8; ++Index)
	{
		AddParticle(Plane, static_cast<uint8>(ESprite::Vortex), FLinearColor(1.f, 1.f, 1.f), 4.5f, Index / 8.f, 0.3f, FMath::FRandRange(100.f, 160.f), 0.05f, 0.5f);
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		AddParticle(Plane, static_cast<uint8>(ESprite::Vortex), White, 4.f, Index / 3.f, 0.22f, 460.f, 0.f, 0.02f);
	}
	// Bolts of lightning forking along the jet and off the mouth.
	for (int32 Index = 0; Index < 18; ++Index)
	{
		AddParticle(Plane, static_cast<uint8>(ESprite::Lightning), FLinearColor(0.8f, 0.9f, 1.f), 3.5f, Index / 18.f, FMath::FRandRange(0.16f, 0.3f), FMath::FRandRange(160.f, 260.f), 0.3f, FMath::FRandRange(0.6f, 1.f));
	}
}

void AShinobiDragonFlame::FollowRider()
{
	if (IsValid(Rider))
	{
		SetActorLocationAndRotation(AShinobiDragonMount::MouthLocation(*Rider), Rider->GetBaseAimRotation());
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
	const FVector Mouth = GetActorLocation();
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
		// A billboard facing the camera, its long axis along the jet as seen from there.
		const FVector ToCamera = (CameraAt - At).GetSafeNormal();
		FVector Along = (Forward - ToCamera * FVector::DotProduct(Forward, ToCamera)).GetSafeNormal();
		if (Along.IsNearlyZero())
		{
			Along = Up;
		}
		const FVector Across = FVector::CrossProduct(Along, ToCamera).GetSafeNormal();
		const FMatrix Basis(Across, Along, ToCamera, FVector::ZeroVector);
		const bool bBolt = Particle.Sprite == static_cast<uint8>(ShinobiVfx::ESprite::Lightning);
		const bool bTongue = bBolt || Particle.Sprite == static_cast<uint8>(ShinobiVfx::ESprite::BlackFlame) || Particle.Sprite == static_cast<uint8>(ShinobiVfx::ESprite::Ember);
		const float Length = Width * (bBolt ? 3.2f : bTongue ? 1.7f : 1.f);

		Particle.Mesh->SetVisibility(Dying > 0.f);
		Particle.Mesh->SetWorldLocationAndRotation(At + Along * (bTongue ? Length * 0.35f : 0.f), Basis.Rotator());
		Particle.Mesh->SetWorldScale3D(FVector(Width / 100.f, Length / 100.f, 1.f));
		if (Particle.Material)
		{
			const float Born = FMath::Min(Cycle * 8.f, 1.f);
			Particle.Material->SetScalarParameterValue(TEXT("Fade"), Born * FMath::Pow(1.f - Cycle, Particle.Sprite == static_cast<uint8>(ShinobiVfx::ESprite::BlackFlame) ? 0.3f : 1.2f) * Dying);
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
	Light->SetRelativeLocation(FVector(Reach * 0.3f, 0.f, 0.f));
	Light->SetIntensity(FlameLightIntensity * Dying * FMath::FRandRange(0.8f, 1.2f));
}
