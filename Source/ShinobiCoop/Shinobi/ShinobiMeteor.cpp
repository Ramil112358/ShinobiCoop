#include "Shinobi/ShinobiMeteor.h"
#include "AbilitySystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiFirePatch.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr float MeteorDropHeight = 2600.f;
	constexpr float MeteorDropSideways = 500.f; // they come in at a slant, from the same side for everyone
}

AShinobiMeteor::AShinobiMeteor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	for (UStaticMeshComponent* Part : { Core.Get(), Shell.Get() })
	{
		Part->SetupAttachment(RootComponent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Core->SetStaticMesh(Sphere.Object);
		Shell->SetStaticMesh(Sphere.Object);
	}
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetCastShadows(false);
	Light->SetSpecularScale(0.f);
}

AShinobiMeteor* AShinobiMeteor::Spawn(UWorld* World, AShinobiCharacterBase* Caster, EShinobiMeteor Kind, const FVector& ImpactPoint, float FallSeconds, float VisualScale, const FShinobiMeteorImpact& Impact, float StartDelay)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FVector Start = ImpactPoint + FVector(-MeteorDropSideways, MeteorDropSideways * 0.5f, MeteorDropHeight);
	const FTransform At((ImpactPoint - Start).Rotation(), Start);
	AShinobiMeteor* Meteor = World->SpawnActorDeferred<AShinobiMeteor>(StaticClass(), At, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Meteor)
	{
		Meteor->Kind = Kind;
		Meteor->StartPoint = Start;
		Meteor->ImpactPoint = ImpactPoint;
		Meteor->FallSeconds = FMath::Max(FallSeconds, 0.1f);
		Meteor->VisualScale = VisualScale;
		Meteor->StartDelay = FMath::Max(StartDelay, 0.f);
		Meteor->Impact = Impact;
		Meteor->Caster = Caster;
		Meteor->FinishSpawning(At);
	}
	return Meteor;
}

void AShinobiMeteor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiMeteor, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiMeteor, StartPoint, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiMeteor, ImpactPoint, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiMeteor, FallSeconds, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiMeteor, VisualScale, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiMeteor, StartDelay, COND_InitialOnly);
}

void AShinobiMeteor::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(StartPoint);
	if (IsNetMode(NM_DedicatedServer))
	{
		return; // nothing to draw; the tick still runs the fall and the landing
	}
	if (Kind == EShinobiMeteor::Ice)
	{
		// A faceted ice shard (Scripts/setup_ice_hail.py), point first and turning about its length so its facets glint,
		// a soft frost streak behind it and two small chips of hail following it. No glow, no light: it is ice, not fire.
		const float Size = VisualScale * FMath::FRandRange(0.85f, 1.15f); // the shard mesh is 1 m long, 22 cm across
		auto Shard = [](UStaticMeshComponent* Part)
		{
			const int32 Variant = FMath::RandRange(0, 2);
			Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Shinobi/VFX/SM_IceShard_%d.SM_IceShard_%d"), Variant, Variant)));
		};
		Shard(Core);
		Core->SetRelativeScale3D(FVector(Size));
		Shell->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Shinobi/VFX/SM_IceTrail.SM_IceTrail"))); // 1 m long, 1 cm radius
		Shell->SetRelativeLocation(FVector(-30.f * Size, 0.f, 0.f));
		Shell->SetRelativeScale3D(FVector(4.f, 6.f, 6.f) * Size);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UStaticMeshComponent* Chip = NewObject<UStaticMeshComponent>(this);
			Shard(Chip);
			Chip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Chip->SetCastShadow(false);
			Chip->SetupAttachment(RootComponent);
			Chip->SetRelativeLocation(FVector(FMath::FRandRange(-260.f, -130.f), Index ? 30.f : -30.f, FMath::FRandRange(-25.f, 25.f)) * Size);
			Chip->SetRelativeRotation(FRotator(FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(0.f, 360.f)));
			Chip->SetRelativeScale3D(FVector(FMath::FRandRange(0.2f, 0.3f) * Size));
			Chip->RegisterComponent();
		}
		Spin = FMath::FRandRange(360.f, 720.f) * (FMath::RandBool() ? 1.f : -1.f);
		Light->DestroyComponent(); // ice gives off no light
		return;
	}
	// ponytail: glowing shells, no trail; a Flame/Ember card trail would read better (sprite-vfx-guide section 3d)
	const FLinearColor Colour = ShinobiVfx::Colors::Fire;
	const FVector Size(2.6f); // the engine sphere is 100 units across
	Core->SetRelativeScale3D(Size * VisualScale);
	Core->SetMaterial(0, ShinobiVfx::MakeGlow(this, Colour, 6.f, 1.f));
	Shell->SetRelativeScale3D(Size * VisualScale * 1.35f);
	Shell->SetMaterial(0, ShinobiVfx::MakeFlame(this, Colour, 4.f));
	Light->SetLightColor(Colour);
	Light->SetIntensity(60000.f);
	Light->SetAttenuationRadius(900.f * VisualScale);
}

void AShinobiMeteor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Falling = Age - StartDelay;
	// Queued behind the others: out of sight in the sky. The parts hide, not the actor: a hidden actor with no collision
	// is not relevant to the other clients, who would only get it when it starts to fall and then wait out the delay again.
	const bool bQueued = Falling < 0.f;
	if (bQueued != bPartsHidden)
	{
		bPartsHidden = bQueued;
		RootComponent->SetVisibility(!bQueued, true);
	}
	if (Spin != 0.f)
	{
		Core->SetRelativeRotation(FRotator(0.f, 0.f, Age * Spin)); // rolling about its length, point first
	}
	const float Alpha = FMath::Clamp(Falling / FallSeconds, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(StartPoint, ImpactPoint, FMath::Pow(Alpha, 1.6f))); // speeding up as it falls
	if (Falling >= FallSeconds && HasAuthority())
	{
		Land();
		Destroy();
	}
}

void AShinobiMeteor::Land()
{
	UWorld* World = GetWorld();
	AShinobiCharacterBase* Thrower = Caster.Get();
	FVector Ground = ImpactPoint; // it lands on the floor at the spot, which may be a slope or a roof: the fire and the crater follow it
	FVector Surface = FVector::UpVector;
	ShinobiVfx::FindSurface(World, ImpactPoint, Ground, Surface);
	if (Thrower)
	{
		UAbilitySystemComponent* Source = Thrower->GetAbilitySystemComponent();
		for (AShinobiCharacterBase* Victim : ShinobiCombat::FindHostilesInRadius(Thrower, ImpactPoint, Impact.Radius))
		{
			if (!ShinobiCombat::ApplyDamage(Source, Victim, Impact.Damage))
			{
				continue;
			}
			if (Impact.Status)
			{
				ShinobiCombat::ApplyStatus(Source, Victim, Impact.Status, Impact.StatusDamagePerTick);
			}
			if (Impact.FirstHitStatus && Impact.FirstHitVictims && !Impact.FirstHitVictims->Contains(Victim))
			{
				Impact.FirstHitVictims->Add(Victim);
				ShinobiCombat::ApplyStatus(Source, Victim, Impact.FirstHitStatus);
			}
		}
		if (Impact.FirePatchScale > 0.f)
		{
			AShinobiFirePatch::Spawn(World, EShinobiFirePatch::Fire, Ground, Thrower, Impact.FirePatchScale, Impact.StatusDamagePerTick, Surface);
		}
	}
	AShinobiBurst::Spawn(World, Kind == EShinobiMeteor::Fire ? EShinobiBurst::MeteorImpact : EShinobiBurst::FrostImpact, Ground, VisualScale, FRotator::ZeroRotator, Surface);
}
