#include "Shinobi/ShinobiBarrier.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/VFX/ShinobiBurst.h"

namespace
{
	struct FBarrierStats
	{
		float Health;
		float Lifetime;
		FLinearColor Tint;
	};

	FBarrierStats StatsFor(EShinobiBarrierKind Kind)
	{
		return Kind == EShinobiBarrierKind::Earth
			? FBarrierStats{ 400.f, 10.f, FLinearColor(0.28f, 0.2f, 0.12f) }
			: FBarrierStats{ 250.f, 6.f, FLinearColor(0.1f, 0.35f, 0.8f) };
	}

	constexpr float RiseSeconds = 0.4f;
	constexpr float RiseDepth = 300.f;
}

AShinobiBarrier::AShinobiBarrier()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Base = CreateDefaultSubobject<USceneComponent>(TEXT("Base"));
	RootComponent = Base;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(Base);
	Box->SetBoxExtent(FVector(30.f, 225.f, 150.f));
	Box->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap); // projectiles: AShinobiProjectile decides by team
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Box->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Box);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetRelativeScale3D(FVector(0.6f, 4.5f, 3.f)); // the engine cube is 100 units
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
}

AShinobiBarrier* AShinobiBarrier::Spawn(UWorld* World, const FTransform& BaseTransform, AShinobiCharacterBase* Caster, EShinobiBarrierKind Kind)
{
	if (!World || World->GetNetMode() == NM_Client || !Caster)
	{
		return nullptr;
	}
	for (TActorIterator<AShinobiBarrier> It(World); It; ++It)
	{
		if (It->GetOwner() == Caster)
		{
			It->Destroy();
		}
	}
	AShinobiBarrier* Wall = World->SpawnActorDeferred<AShinobiBarrier>(StaticClass(), BaseTransform, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Wall)
	{
		const FBarrierStats Stats = StatsFor(Kind);
		Wall->Kind = Kind;
		Wall->Team = Caster->GetTeam();
		Wall->Health = Stats.Health;
		Wall->InitialLifeSpan = Stats.Lifetime;
		Wall->FinishSpawning(BaseTransform);
	}
	return Wall;
}

AShinobiBarrier* AShinobiBarrier::SpawnInFront(AShinobiCharacterBase* Caster, EShinobiBarrierKind Kind)
{
	if (!Caster)
	{
		return nullptr;
	}
	const FVector Forward = Caster->GetActorForwardVector().GetSafeNormal2D();
	FVector Where = Caster->GetActorLocation() + Forward * 350.f;
	Where.Z -= Caster->GetSimpleCollisionHalfHeight(); // at their feet
	return Spawn(Caster->GetWorld(), FTransform(FRotator(0.f, Caster->GetActorRotation().Yaw, 0.f), Where), Caster, Kind);
}

void AShinobiBarrier::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiBarrier, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiBarrier, Team, COND_InitialOnly);
}

void AShinobiBarrier::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		SetActorTickEnabled(false);
		return;
	}
	// Earth: Poly Haven's rock_09 PBR material on the slab (stretched with the cube's UVs). Water: M_WaterWall (Scripts/setup_vfx.py).
	// Either missing: the engine's basic shape material, tinted.
	const TCHAR* LookPath = Kind == EShinobiBarrierKind::Earth
		? TEXT("/Game/Shinobi/World/Library/PH_Rock/rock_09/rock_09_2k/Materials/rock_09.rock_09")
		: TEXT("/Game/Shinobi/VFX/M_WaterWall.M_WaterWall");
	if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, LookPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Mesh->SetMaterial(0, UMaterialInstanceDynamic::Create(Look, this));
	}
	else if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(Basic, this);
		Tinted->SetVectorParameterValue(TEXT("Color"), StatsFor(Kind).Tint);
		Mesh->SetMaterial(0, Tinted);
	}
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -RiseDepth));
}

void AShinobiBarrier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Broken (full burst) or run out (half): the server spawns it and it replicates to every client.
	if (EndPlayReason == EEndPlayReason::Destroyed && HasAuthority() && GetWorld() && !GetWorld()->bIsTearingDown)
	{
		AShinobiBurst::Spawn(GetWorld(), Kind == EShinobiBarrierKind::Earth ? EShinobiBurst::Crumble : EShinobiBurst::Splash,
			GetActorLocation() + FVector(0.f, 0.f, 120.f), Health <= 0.f ? 1.f : 0.5f, GetActorRotation());
	}
	Super::EndPlay(EndPlayReason);
}

void AShinobiBarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// ponytail: each machine runs its own rise from when it first saw the wall, instead of a replicated start time
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -RiseDepth * FMath::Max(0.f, 1.f - Age / RiseSeconds)));
}

void AShinobiBarrier::ReceiveShinobiDamage(float Amount, const AActor* Source)
{
	if (!HasAuthority())
	{
		return;
	}
	Health -= Amount;
	if (Health <= 0.f)
	{
		Destroy();
	}
}
