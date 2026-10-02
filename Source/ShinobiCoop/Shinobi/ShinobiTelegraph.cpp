#include "Shinobi/ShinobiTelegraph.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr float PlaneSize = 100.f; // the engine plane is 100 units across
	constexpr float RingArt = 1.25f;   // the ring sprite fills most of its quad; this stretches the quad to match the radius
}

AShinobiTelegraph::AShinobiTelegraph()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Outer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Outer"));
	Inner = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Inner"));
	for (UStaticMeshComponent* Ring : { Outer.Get(), Inner.Get() })
	{
		Ring->SetupAttachment(RootComponent);
		Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ring->SetCastShadow(false);
		Ring->SetRelativeLocation(FVector(0.f, 0.f, 6.f)); // just above the ground
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		Outer->SetStaticMesh(Plane.Object);
		Inner->SetStaticMesh(Plane.Object);
	}
}

AShinobiTelegraph* AShinobiTelegraph::Spawn(UWorld* World, const FVector& Where, float Radius, float Seconds, const FLinearColor& Tint)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FTransform At(FRotator::ZeroRotator, Where);
	AShinobiTelegraph* Ring = World->SpawnActorDeferred<AShinobiTelegraph>(StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Ring)
	{
		Ring->Radius = Radius;
		Ring->Seconds = Seconds;
		Ring->Tint = Tint;
		Ring->InitialLifeSpan = Seconds;
		Ring->FinishSpawning(At);
	}
	return Ring;
}

void AShinobiTelegraph::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiTelegraph, Radius, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiTelegraph, Seconds, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiTelegraph, Tint, COND_InitialOnly);
}

void AShinobiTelegraph::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		SetActorTickEnabled(false);
		return;
	}
	using ShinobiVfx::ESprite;
	Outer->SetMaterial(0, ShinobiVfx::MakeSprite(this, ESprite::Ring, 0, Tint, 1.2f));
	Inner->SetMaterial(0, ShinobiVfx::MakeSprite(this, ESprite::Ring, 0, Tint, 2.f));
	Outer->SetRelativeScale3D(FVector(Radius * 2.f * RingArt / PlaneSize));
	Inner->SetRelativeScale3D(FVector(Radius * 2.f * RingArt / PlaneSize));
}

void AShinobiTelegraph::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// The inner ring closes on the centre exactly when the jutsu lands.
	const float Left = FMath::Clamp(1.f - Age / FMath::Max(Seconds, 0.01f), 0.f, 1.f);
	Inner->SetRelativeScale3D(FVector(Radius * 2.f * RingArt / PlaneSize * FMath::Max(Left, 0.05f)));
}
