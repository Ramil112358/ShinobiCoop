#include "Shinobi/ShinobiMarkerKunai.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiCharacterBase.h"

AShinobiMarkerKunai::AShinobiMarkerKunai()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = Lifetime;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // flies through characters
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore); // and through its own fan: all three leave one muzzle point
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = Collision;

	// ponytail: an engine cone laid along the flight until Scripts/setup_marker_kunai.py imports the real model
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (Cone.Succeeded())
	{
		Mesh->SetStaticMesh(Cone.Object);
	}
	Mesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // the cone points up (+Z); lay it along +X
	Mesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.5f));

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->OnProjectileStop.AddDynamic(this, &AShinobiMarkerKunai::OnStopped);
}

AShinobiMarkerKunai* AShinobiMarkerKunai::Spawn(UWorld* World, AShinobiCharacterBase* Caster, const FVector& Start, const FRotator& Direction)
{
	if (!World || World->GetNetMode() == NM_Client || !Caster)
	{
		return nullptr;
	}
	const FTransform Where(Direction, Start);
	AShinobiMarkerKunai* Kunai = World->SpawnActorDeferred<AShinobiMarkerKunai>(StaticClass(), Where, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Kunai)
	{
		Kunai->FinishSpawning(Where);
	}
	return Kunai;
}

void AShinobiMarkerKunai::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	ThrownAt = GetWorld()->GetTimeSeconds();
	// Out of range without having hit anything: gone (a throw at open sky leaves nothing to jump to).
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!bStuck)
		{
			Destroy();
		}
	}), Range / Speed + 0.02f, false);
}

void AShinobiMarkerKunai::OnStopped(const FHitResult& ImpactResult)
{
	if (HasAuthority())
	{
		bStuck = true;
		StuckNormal = ImpactResult.ImpactNormal;
	}
}

FVector AShinobiMarkerKunai::GetArrivalPoint(float CapsuleRadius, float CapsuleHalfHeight) const
{
	return GetActorLocation() + StuckNormal * (CapsuleRadius + 5.f) + FVector(0.f, 0.f, StuckNormal.Z * CapsuleHalfHeight);
}
