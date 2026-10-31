#include "Shinobi/VFX/ShinobiFirePatch.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
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
	constexpr float FireOutTime = 1.2f; // the Niagara fire stops feeding this long before the patch goes, and dies down
	constexpr float FireWidth = 400.f;  // how wide the Fab ground fires are at scale 1
	/** The Fab fire system for each kind: the three differ in size and shape; Amaterasu's and Tairn's are recoloured, not separate systems. */
	const TCHAR* FabFire(EShinobiFirePatch Kind)
	{
		switch (Kind)
		{
		case EShinobiFirePatch::Fire:
		case EShinobiFirePatch::BlackFlame: return TEXT("/Game/M5VFXVOL2/Niagara/Fire/NFire_01.NFire_01");
		case EShinobiFirePatch::CrimsonFire: return TEXT("/Game/M5VFXVOL2/Niagara/Fire/NFire_02.NFire_02");
		case EShinobiFirePatch::TairnFire: return TEXT("/Game/M5VFXVOL2/Niagara/Fire/NFire_03.NFire_03");
		default: return nullptr;
		}
	}

	/**
	 * Recolours a Fab fire through the user variables its Color modules' Scale Color are linked to (the pack's own flames get their orange
	 * from an HDR red-green-blue curve over the particle's life, which these scale). 1,1,1 leaves the pack's fire as it is.
	 */
	void SetFireLook(UNiagaraComponent& Fire, EShinobiFirePatch Kind)
	{
		FVector Flames(1.f), Smoke(1.f), Embers(1.f);
		float Heat = 1.f; // how hot the smoke glows
		if (Kind == EShinobiFirePatch::BlackFlame)
		{
			Flames = FVector(0.f, 0.f, 0.06f); // near black, a little violet in the young flames (the pack's red runs up to 20: it must go)
			Smoke = FVector(0.05f, 0.04f, 0.08f);
			Heat = 0.f; // the pack's smoke glows red-hot on its own (a blackbody in its material): cold, it is just dark smoke
			Embers = FVector(0.f, 0.1f, 1.f);
		}
		else if (Kind == EShinobiFirePatch::TairnFire)
		{
			Flames = FVector(0.f, 0.12f, 2.f); // blue-heavy: blue has little luminance, and any red left shows as orange
			Smoke = FVector(0.1f, 0.14f, 0.3f);
			Heat = 0.f;
			Embers = FVector(0.f, 0.2f, 1.5f);
		}
		Fire.SetVariableVec3(TEXT("User.FireScale"), Flames);
		Fire.SetVariableVec3(TEXT("User.SmokeScale"), Smoke);
		Fire.SetVariableVec3(TEXT("User.EmberScale"), Embers);
		Fire.SetVariableFloat(TEXT("User.SmokeHeat"), Heat);
	}
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

AShinobiFirePatch* AShinobiFirePatch::Spawn(UWorld* World, EShinobiFirePatch Kind, const FVector& Location, AShinobiCharacterBase* Caster, float Scale, float DamagePerTick, const FVector& Normal)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FVector Point = Location;
	FVector Up = Normal.GetSafeNormal();
	if (Up.IsZero())
	{
		ShinobiVfx::FindSurface(World, Location, Point, Up); // a hit can land at body height: burn on the floor under it
	}
	const FTransform Where(ShinobiVfx::SurfaceFrame(Up), Point);
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
	const bool bTairn = Kind == EShinobiFirePatch::TairnFire;
	const bool bDragon = bTairn || Kind == EShinobiFirePatch::CrimsonFire;
	Duration = bBlack ? 5.f : bDragon ? DragonDuration : 3.f;
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

	const FLinearColor Color = bBlack ? ShinobiVfx::Colors::BlackFlame : bTairn ? FLinearColor(0.45f, 0.65f, 1.f) : ShinobiVfx::Colors::Fire;
	using ShinobiVfx::ESprite;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	FRandomStream Random(GetUniqueID()); // cosmetic: each machine may scatter the cards differently
	const ESprite FlameKind = FlameSprite();
	if (const TCHAR* Path = FabFire(Kind))
	{
		// Flames rise whatever the patch burns on. Against a wall the fire stands upright with its base on the wall (a tenth of its
		// radius out, so most of the flame body is in front of it; the part of the base behind the wall is hidden by it, which is how a
		// fire on the ground meets the floor). The pack's depth fade is shortened for this (Scripts/setup_fab_fire_fade.py).
		const FVector Up = GetActorQuat().GetUpVector();
		const FVector Away = Up - FVector::UpVector * Up.Z;
		Fire = NewObject<UNiagaraComponent>(this);
		Fire->SetAsset(LoadObject<UNiagaraSystem>(nullptr, Path));
		Fire->SetAutoActivate(true);
		SetFireLook(*Fire, Kind);
		Fire->SetupAttachment(RootComponent);
		Fire->SetUsingAbsoluteRotation(true);
		Fire->SetUsingAbsoluteScale(true);
		Fire->SetWorldRotation(FQuat::Identity);
		Fire->SetWorldScale3D(FVector(GetRadius() * 2.f / FireWidth));
		Fire->SetWorldLocation(GetActorLocation() + Away * GetRadius() * 0.1f);
		Fire->RegisterComponent();
		Fire->AdvanceSimulationByTime(1.6f, 1.f / 30.f); // the fire takes a couple of seconds to build, and a patch lasts only a few: start it going
	}
	for (int32 Index = 0; Index < (Fire ? 0 : PatchFlameCount); ++Index)
	{
		// Spread over most of the radius, taller in the middle.
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		const FVector2D Spot = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Random.FRandRange(0.f, 0.7f) * GetRadius();
		UMaterialInstanceDynamic* Material = ShinobiVfx::MakeSprite(this, FlameKind, Random.RandRange(0, 7), bBlack ? Color : FLinearColor::White, bBlack ? 6.f : 1.8f);
		Flames.Add(MakeCard(Plane, Material, Spot, Random.FRand(), Random.RandRange(0, 7)));
	}
	if (!bBlack && !Fire)
	{
		for (int32 Index = 0; Index < (bDragon ? PatchSmokeCount + 2 : PatchSmokeCount); ++Index)
		{
			Smokes.Add(MakeCard(Plane, ShinobiVfx::MakeSprite(this, ESprite::Smoke, Index, bTairn ? FLinearColor(0.07f, 0.08f, 0.12f) : FLinearColor(0.08f, 0.08f, 0.09f), 1.f), FVector2D::ZeroVector, Index / static_cast<float>(PatchSmokeCount), Index));
		}
	}
	for (int32 Index = 0; Index < (Fire ? 0 : bDragon ? PatchEmberCount + 3 : PatchEmberCount); ++Index)
	{
		UMaterialInstanceDynamic* Material = ShinobiVfx::MakeSprite(this, ESprite::Ember, Index, bBlack ? FLinearColor(0.6f, 0.3f, 1.f) : bTairn ? FLinearColor(0.55f, 0.75f, 1.f) : FLinearColor::White, 2.f);
		Embers.Add(MakeCard(Plane, Material, FVector2D(Random.FRandRange(-0.3f, 0.3f) * GetRadius(), Random.FRandRange(-0.3f, 0.3f) * GetRadius()), Random.FRand(), Index));
	}
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(GetRadius() * 3.f);
}

void AShinobiFirePatch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The surface is charred once the flames have gone out (a wall too); Amaterasu leaves a longer, cooler scar. Only a patch
	// that burned to the end leaves one: not one that is torn down with the level, or that never got going on this client.
	if (EndPlayReason == EEndPlayReason::Destroyed && !IsNetMode(NM_DedicatedServer) && Age >= Duration - 1.f)
	{
		const bool bBlack = Kind == EShinobiFirePatch::BlackFlame;
		const bool bTairn = Kind == EShinobiFirePatch::TairnFire;
		ShinobiVfx::SpawnScorch(GetWorld(), GetActorLocation(), GetRadius(), bBlack ? FLinearColor(0.75f, 0.65f, 1.f) : bTairn ? FLinearColor(0.8f, 0.88f, 1.f) : FLinearColor::White, bBlack ? 14.f : 9.f, nullptr, GetActorQuat().GetUpVector());
	}
	Super::EndPlay(EndPlayReason);
}

ShinobiVfx::ESprite AShinobiFirePatch::FlameSprite() const
{
	switch (Kind)
	{
	case EShinobiFirePatch::BlackFlame: return ShinobiVfx::ESprite::BlackFlame;
	case EShinobiFirePatch::TairnFire: return ShinobiVfx::ESprite::TairnFire;
	case EShinobiFirePatch::CrimsonFire: return ShinobiVfx::ESprite::CrimsonFire;
	default: return ShinobiVfx::ESprite::Flame;
	}
}

AShinobiFirePatch::FCard AShinobiFirePatch::MakeCard(UStaticMesh* Plane, UMaterialInstanceDynamic* Material, const FVector2D& Spot, float Phase, int32 Variant)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	Mesh->SetStaticMesh(Plane);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetMaterial(0, Material);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeLocation(FVector(Spot, 0.f)); // the patch is turned to its surface: its XY is the surface plane; Tick keeps it there
	Mesh->RegisterComponent();
	FCard Card;
	Card.Mesh = Mesh;
	Card.Spot = Spot;
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
	if (Fire && Age >= Duration - FireOutTime && Fire->IsActive())
	{
		Fire->Deactivate(); // stop feeding it: what is burning dies down over the last moments
	}
	// Rise in quickly, flicker, shrink away over the last PatchFadeTime.
	const float Life = FMath::Min(FMath::Clamp(Age / 0.2f, 0.f, 1.f), FMath::Clamp((Duration - Age) / PatchFadeTime, 0.f, 1.f));
	const bool bBlack = Kind == EShinobiFirePatch::BlackFlame;
	using ShinobiVfx::ESprite;

	// Cards stand upright (the engine plane lies flat: roll it up) and turn about the vertical to face the camera. Fire rises
	// whatever it burns on: a card stands at its spot in the surface plane and lifts along world up, and on a wall (Away is
	// the horizontal part of the surface normal, nothing on the floor) it stands clear of the surface, the smoke drifting off it.
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraAt = Camera ? Camera->GetCameraLocation() : GetActorLocation() + FVector(-1000.f, 0.f, 0.f);
	const FVector SurfaceUp = GetActorQuat().GetUpVector();
	const FVector Away = SurfaceUp - FVector::UpVector * SurfaceUp.Z;
	const auto Place = [&](const FCard& Card, const FVector2D& Size, float Lift, float Clear)
	{
		const FVector Here = GetActorTransform().TransformPositionNoScale(FVector(Card.Spot, 0.f));
		const float FaceYaw = FMath::RadiansToDegrees(FMath::Atan2(CameraAt.Y - Here.Y, CameraAt.X - Here.X)) - 90.f;
		Card.Mesh->SetWorldRotation(FRotator(0.f, FaceYaw, 90.f));
		Card.Mesh->SetRelativeScale3D(FVector(Size.X, Size.Y, 1.f));
		Card.Mesh->SetWorldLocation(Here + FVector(0.f, 0.f, Lift) + Away * Clear);
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
		Swap(Card, FlameSprite(), FlameSwapTime);
		const float Centre = 1.f - Card.Spot.Size() / FMath::Max(GetRadius(), 1.f);
		const float Flicker = 1.f + 0.08f * FMath::Sin(Age * 9.f + Card.Phase * 2.f * PI);
		const FVector2D Size = FlameCardSize * Scale * Life * FVector2D(0.9f + 0.2f * Centre, Flicker * (0.6f + 0.6f * Centre));
		Place(Card, Size, Size.Y * 50.f, Size.X * 25.f);
		SetFade(Card, FMath::Clamp(Life * 1.2f, 0.f, 1.f));
	}
	for (FCard& Card : Smokes)
	{
		// Each plume rises and thins out, then starts again from the flames.
		const float Cycle = FMath::Frac(Age / SmokeCycle + Card.Phase);
		const FVector2D Size = SmokeCardSize * Scale * FMath::Lerp(0.5f, 1.f, Cycle);
		Place(Card, Size, FMath::Lerp(80.f, 380.f, Cycle) * Scale, Size.X * 25.f + FMath::Lerp(0.f, 150.f, Cycle) * Scale);
		SetFade(Card, FMath::Sin(Cycle * PI) * 0.7f * Life);
	}
	for (FCard& Card : Embers)
	{
		Swap(Card, ESprite::Ember, 0.18f);
		const FVector2D Size = EmberCardSize * Scale * Life;
		Place(Card, Size, Size.Y * 50.f, Size.X * 25.f);
		SetFade(Card, Life);
	}
	Light->SetIntensity((bBlack ? 800.f : 4000.f) * Life * FMath::FRandRange(0.8f, 1.1f));
}
