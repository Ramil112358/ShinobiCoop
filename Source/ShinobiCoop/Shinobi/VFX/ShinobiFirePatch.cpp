#include "Shinobi/VFX/ShinobiFirePatch.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr int32 PatchFlameCount = 10;
	constexpr int32 PatchSmokeCount = 3;
	constexpr int32 PatchEmberCount = 2;
	constexpr float PatchFadeTime = 0.5f;
	const FVector2D FlameCardSize(1.1f, 2.2f); // the engine plane is 100 units across: a 110 x 220 card, the flame fills about 40% of its width
	const FVector2D SmokeCardSize(1.6f, 3.2f);
	const FVector2D EmberCardSize(1.8f, 3.6f);
	constexpr float FlameSwapTime = 0.11f; // flipbook flicker: each flame card changes shape this often (give or take)
	constexpr float SmokeCycle = 2.2f;
}

AShinobiFirePatch::AShinobiFirePatch()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	Light->SetCastShadows(false);
	Light->SetSpecularScale(0.f);
}

AShinobiFirePatch* AShinobiFirePatch::Spawn(UWorld* World, EShinobiFirePatch Kind, const FVector& Location, AShinobiCharacterBase* Caster, float Scale, float DamagePerTick)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FTransform Where(Location);
	AShinobiFirePatch* Patch = World->SpawnActorDeferred<AShinobiFirePatch>(AShinobiFirePatch::StaticClass(), Where, Caster, Caster ? Caster->GetInstigator() : nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Patch)
	{
		Patch->Kind = Kind;
		Patch->Scale = Scale;
		Patch->Caster = Caster;
		Patch->DamagePerTick = DamagePerTick;
		Patch->FinishSpawning(Where);
	}
	return Patch;
}

void AShinobiFirePatch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiFirePatch, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiFirePatch, Scale, COND_InitialOnly);
}

void AShinobiFirePatch::BeginPlay()
{
	Super::BeginPlay();
	const bool bBlack = Kind == EShinobiFirePatch::BlackFlame;
	Duration = bBlack ? 5.f : 3.f;
	if (HasAuthority())
	{
		SetLifeSpan(Duration); // the server's destruction removes it everywhere
		GetWorldTimerManager().SetTimer(BurnTimer, this, &AShinobiFirePatch::BurnInside, TickInterval, true, 0.05f);
	}
	if (IsNetMode(NM_DedicatedServer))
	{
		SetActorTickEnabled(false);
		return;
	}

	const FLinearColor Color = bBlack ? ShinobiVfx::Colors::BlackFlame : ShinobiVfx::Colors::Fire;
	using ShinobiVfx::ESprite;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	FRandomStream Random(GetUniqueID()); // cosmetic: each machine may scatter the cards differently
	const ESprite FlameKind = bBlack ? ESprite::BlackFlame : ESprite::Flame;
	for (int32 Index = 0; Index < PatchFlameCount; ++Index)
	{
		// Spread over most of the radius, taller in the middle.
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		const FVector2D Spot = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Random.FRandRange(0.f, 0.7f) * GetRadius();
		UMaterialInstanceDynamic* Material = ShinobiVfx::MakeSprite(this, FlameKind, Random.RandRange(0, 7), bBlack ? Color : FLinearColor::White, bBlack ? 6.f : 1.8f);
		Flames.Add(MakeCard(Plane, Material, FVector(Spot, 0.f), Random.FRand(), Random.RandRange(0, 7)));
	}
	if (!bBlack)
	{
		for (int32 Index = 0; Index < PatchSmokeCount; ++Index)
		{
			Smokes.Add(MakeCard(Plane, ShinobiVfx::MakeSprite(this, ESprite::Smoke, Index, FLinearColor(0.08f, 0.08f, 0.09f), 1.f), FVector::ZeroVector, Index / static_cast<float>(PatchSmokeCount), Index));
		}
	}
	for (int32 Index = 0; Index < PatchEmberCount; ++Index)
	{
		UMaterialInstanceDynamic* Material = ShinobiVfx::MakeSprite(this, ESprite::Ember, Index, bBlack ? FLinearColor(0.6f, 0.3f, 1.f) : FLinearColor::White, 2.f);
		Embers.Add(MakeCard(Plane, Material, FVector(Random.FRandRange(-0.3f, 0.3f) * GetRadius(), Random.FRandRange(-0.3f, 0.3f) * GetRadius(), 0.f), Random.FRand(), Index));
	}
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(GetRadius() * 3.f);
	// The ground stays charred a good while after the flames go out; Amaterasu leaves a longer, cooler scar.
	ShinobiVfx::SpawnScorch(GetWorld(), GetActorLocation(), GetRadius() * 1.3f, bBlack ? FLinearColor(0.75f, 0.65f, 1.f) : FLinearColor::White, Duration + (bBlack ? 14.f : 9.f));
}

AShinobiFirePatch::FCard AShinobiFirePatch::MakeCard(UStaticMesh* Plane, UMaterialInstanceDynamic* Material, const FVector& Where, float Phase, int32 Variant)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	Mesh->SetStaticMesh(Plane);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetMaterial(0, Material);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeLocation(Where);
	Mesh->RegisterComponent();
	FCard Card;
	Card.Mesh = Mesh;
	Card.Material = Material;
	Card.Phase = Phase;
	Card.Variant = Variant;
	return Card;
}

void AShinobiFirePatch::BurnInside()
{
	const AShinobiCharacterBase* Burner = Caster.Get();
	UAbilitySystemComponent* Source = Burner ? Burner->GetAbilitySystemComponent() : nullptr;
	if (!Source)
	{
		return;
	}
	const TSubclassOf<UGameplayEffect> Status = Kind == EShinobiFirePatch::BlackFlame ? TSubclassOf<UGameplayEffect>(UGE_BlackFlame::StaticClass()) : TSubclassOf<UGameplayEffect>(UGE_Burn::StaticClass());
	for (const AShinobiCharacterBase* Inside : ShinobiCombat::FindHostilesInRadius(Burner, GetActorLocation(), GetRadius()))
	{
		ShinobiCombat::ApplyStatus(Source, Inside, Status, DamagePerTick);
	}
}

void AShinobiFirePatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// Rise in quickly, flicker, shrink away over the last PatchFadeTime.
	const float Life = FMath::Min(FMath::Clamp(Age / 0.2f, 0.f, 1.f), FMath::Clamp((Duration - Age) / PatchFadeTime, 0.f, 1.f));
	const bool bBlack = Kind == EShinobiFirePatch::BlackFlame;
	using ShinobiVfx::ESprite;

	// Cards stand upright (the engine plane lies flat: roll it up) and turn about the vertical to face the camera.
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraAt = Camera ? Camera->GetCameraLocation() : GetActorLocation() + FVector(-1000.f, 0.f, 0.f);
	const auto Place = [&](const FCard& Card, const FVector2D& Size, float Lift)
	{
		const FVector Here = Card.Mesh->GetComponentLocation();
		const float FaceYaw = FMath::RadiansToDegrees(FMath::Atan2(CameraAt.Y - Here.Y, CameraAt.X - Here.X)) - 90.f;
		Card.Mesh->SetWorldRotation(FRotator(0.f, FaceYaw, 90.f));
		Card.Mesh->SetRelativeScale3D(FVector(Size.X, Size.Y, 1.f));
		const FVector Local = Card.Mesh->GetRelativeLocation();
		Card.Mesh->SetRelativeLocation(FVector(Local.X, Local.Y, Lift));
	};
	const auto SetFade = [](const FCard& Card, float Fade)
	{
		if (Card.Material)
		{
			Card.Material->SetScalarParameterValue(TEXT("Fade"), Fade);
		}
	};
	const auto Swap = [&](FCard& Card, ESprite Sprite, float Interval)
	{
		if (Age >= Card.NextSwap)
		{
			Card.Variant = (Card.Variant + 1 + FMath::RandRange(0, 2)) % ShinobiVfx::SpriteVariants(Sprite);
			ShinobiVfx::SetSpriteVariant(Card.Material, Sprite, Card.Variant);
			Card.NextSwap = Age + Interval * FMath::FRandRange(0.7f, 1.3f);
		}
	};

	for (FCard& Card : Flames)
	{
		Swap(Card, bBlack ? ESprite::BlackFlame : ESprite::Flame, FlameSwapTime);
		const float Centre = 1.f - Card.Mesh->GetRelativeLocation().Size2D() / FMath::Max(GetRadius(), 1.f);
		const float Flicker = 1.f + 0.08f * FMath::Sin(Age * 9.f + Card.Phase * 2.f * PI);
		const FVector2D Size = FlameCardSize * Scale * Life * FVector2D(0.9f + 0.2f * Centre, Flicker * (0.6f + 0.6f * Centre));
		Place(Card, Size, Size.Y * 50.f);
		SetFade(Card, FMath::Clamp(Life * 1.2f, 0.f, 1.f));
	}
	for (FCard& Card : Smokes)
	{
		// Each plume rises and thins out, then starts again from the flames.
		const float Cycle = FMath::Frac(Age / SmokeCycle + Card.Phase);
		const FVector2D Size = SmokeCardSize * Scale * FMath::Lerp(0.5f, 1.f, Cycle);
		Place(Card, Size, FMath::Lerp(80.f, 380.f, Cycle) * Scale);
		SetFade(Card, FMath::Sin(Cycle * PI) * 0.7f * Life);
	}
	for (FCard& Card : Embers)
	{
		Swap(Card, ESprite::Ember, 0.18f);
		const FVector2D Size = EmberCardSize * Scale * Life;
		Place(Card, Size, Size.Y * 50.f);
		SetFade(Card, Life);
	}
	Light->SetIntensity((bBlack ? 800.f : 4000.f) * Life * FMath::FRandRange(0.8f, 1.1f));
}
