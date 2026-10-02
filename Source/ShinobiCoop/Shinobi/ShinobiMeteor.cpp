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
	constexpr float DropHeight = 2600.f;
	constexpr float DropSideways = 500.f; // they come in at a slant, from the same side for everyone
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

AShinobiMeteor* AShinobiMeteor::Spawn(UWorld* World, AShinobiCharacterBase* Caster, EShinobiMeteor Kind, const FVector& ImpactPoint, float FallSeconds, float VisualScale, const FShinobiMeteorImpact& Impact)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FVector Start = ImpactPoint + FVector(-DropSideways, DropSideways * 0.5f, DropHeight);
	const FTransform At((ImpactPoint - Start).Rotation(), Start);
	AShinobiMeteor* Meteor = World->SpawnActorDeferred<AShinobiMeteor>(StaticClass(), At, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Meteor)
	{
		Meteor->Kind = Kind;
		Meteor->StartPoint = Start;
		Meteor->ImpactPoint = ImpactPoint;
		Meteor->FallSeconds = FMath::Max(FallSeconds, 0.1f);
		Meteor->VisualScale = VisualScale;
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
}

void AShinobiMeteor::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(StartPoint);
	if (IsNetMode(NM_DedicatedServer))
	{
		return; // nothing to draw; the tick still runs the fall and the landing
	}
	// ponytail: glowing shells, no trail; a Flame/Ember card trail would read better (sprite-vfx-guide section 3d)
	const bool bFire = Kind == EShinobiMeteor::Fire;
	const FLinearColor Colour = bFire ? ShinobiVfx::Colors::Fire : ShinobiVfx::Colors::Ice;
	const FVector Size = bFire ? FVector(2.6f) : FVector(0.45f, 0.45f, 1.5f); // the engine sphere is 100 units across
	Core->SetRelativeScale3D(Size * VisualScale);
	Core->SetMaterial(0, ShinobiVfx::MakeGlow(this, Colour, bFire ? 6.f : 4.f, 1.f));
	Shell->SetRelativeScale3D(Size * VisualScale * 1.35f);
	Shell->SetMaterial(0, ShinobiVfx::MakeFlame(this, Colour, bFire ? 4.f : 2.f));
	Light->SetLightColor(Colour);
	Light->SetIntensity(bFire ? 60000.f : 15000.f);
	Light->SetAttenuationRadius((bFire ? 900.f : 400.f) * VisualScale);
}

void AShinobiMeteor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / FallSeconds, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(StartPoint, ImpactPoint, FMath::Pow(Alpha, 1.6f))); // speeding up as it falls
	if (Alpha >= 1.f && HasAuthority())
	{
		Land();
		Destroy();
	}
}

void AShinobiMeteor::Land()
{
	UWorld* World = GetWorld();
	AShinobiCharacterBase* Thrower = Caster.Get();
	if (Thrower)
	{
		UAbilitySystemComponent* Source = Thrower->GetAbilitySystemComponent();
		for (AShinobiCharacterBase* Victim : ShinobiCombat::FindHostilesInRadius(Thrower, ImpactPoint, Impact.Radius))
		{
			if (ShinobiCombat::ApplyDamage(Source, Victim, Impact.Damage) && Impact.Status)
			{
				ShinobiCombat::ApplyStatus(Source, Victim, Impact.Status, Impact.StatusDamagePerTick);
			}
		}
		if (Impact.FirePatchScale > 0.f)
		{
			AShinobiFirePatch::Spawn(World, EShinobiFirePatch::Fire, ImpactPoint, Thrower, Impact.FirePatchScale, Impact.StatusDamagePerTick);
		}
	}
	AShinobiBurst::Spawn(World, Kind == EShinobiMeteor::Fire ? EShinobiBurst::MeteorImpact : EShinobiBurst::FrostImpact, ImpactPoint, VisualScale);
}
