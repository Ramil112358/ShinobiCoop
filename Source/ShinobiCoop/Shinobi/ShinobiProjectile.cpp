#include "Shinobi/ShinobiProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h" // full type: TSubclassOf<UGameplayEffect>'s bool/Get() checks IsChildOf(StaticClass())
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"

AShinobiProjectile::AShinobiProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 3.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(20.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	// Characters overlap instead of block, so hostility (not collision) decides what the projectile explodes on;
	// walls and the floor still stop it.
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AShinobiProjectile::OnOverlap);
	Collision->OnComponentHit.AddDynamic(this, &AShinobiProjectile::OnHit);
	RootComponent = Collision;

	// ponytail: a grey ball until M2b's fire VFX
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.4f)); // the engine sphere is 100 units across
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 2500.f;
	Movement->MaxSpeed = 2500.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
}

void AShinobiProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const AShinobiCharacterBase* Caster = Cast<AShinobiCharacterBase>(GetInstigator());
	AShinobiCharacterBase* Target = Cast<AShinobiCharacterBase>(OtherActor);
	if (HasAuthority() && Caster && Caster->IsHostileTo(Target))
	{
		// A sweep overlap reports the point of first contact; GetActorLocation() is the end of the move, up to
		// 125 units past it.
		Explode(Target, bFromSweep ? FVector(SweepResult.Location) : GetActorLocation());
	}
}

void AShinobiProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (HasAuthority())
	{
		Explode(nullptr, Hit.ImpactPoint);
	}
}

void AShinobiProjectile::Explode(AShinobiCharacterBase* DirectTarget, const FVector& Location)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;

	const AShinobiCharacterBase* Caster = Cast<AShinobiCharacterBase>(GetInstigator());
	if (UAbilitySystemComponent* Source = Caster ? Caster->GetAbilitySystemComponent() : nullptr)
	{
		if (DirectTarget && ShinobiCombat::ApplyDamage(Source, DirectTarget, DirectDamage) && StatusEffect)
		{
			ShinobiCombat::ApplyStatus(Source, DirectTarget, StatusEffect, StatusDamagePerTick);
		}
		for (const AShinobiCharacterBase* Other : ShinobiCombat::FindHostilesInRadius(Caster, Location, SplashRadius))
		{
			if (Other != DirectTarget)
			{
				ShinobiCombat::ApplyDamage(Source, Other, SplashDamage);
			}
		}
	}
	Destroy();
}
