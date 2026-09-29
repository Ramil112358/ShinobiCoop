#include "Shinobi/ShinobiProjectile.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffect.h" // full type: TSubclassOf<UGameplayEffect>'s bool/Get() checks IsChildOf(StaticClass())
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiFirePatch.h"
#include "Shinobi/VFX/ShinobiVfx.h"

AShinobiProjectile::AShinobiProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 3.f;
	GlowColor = ShinobiVfx::Colors::Fire;

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

	// Fire look: a glowing sphere (material set in BeginPlay) plus an orange light.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCastShadow(false);
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.4f)); // the engine sphere is 100 units across
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Collision);
	Light->SetLightColor(ShinobiVfx::Colors::Fire);
	Light->SetIntensity(5000.f);
	Light->SetAttenuationRadius(400.f);
	Light->SetCastShadows(false);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only the Fireball's trail ticks

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 2500.f;
	Movement->MaxSpeed = 2500.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
}

void AShinobiProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (Scale != 1.f)
	{
		Collision->SetSphereRadius(Collision->GetUnscaledSphereRadius() * Scale);
		Mesh->SetRelativeScale3D(Mesh->GetRelativeScale3D() * Scale);
		Light->SetAttenuationRadius(Light->AttenuationRadius * Scale);
	}
	if (!IsNetMode(NM_DedicatedServer))
	{
		Mesh->SetMaterial(0, ShinobiVfx::MakeGlow(this, GlowColor, GlowIntensity, 1.f));
		if (bFlameShell)
		{
			UStaticMeshComponent* Shell = NewObject<UStaticMeshComponent>(this, TEXT("FlameShell"));
			Shell->SetStaticMesh(Mesh->GetStaticMesh());
			Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Shell->SetCastShadow(false);
			Shell->SetMaterial(0, ShinobiVfx::MakeFlame(this, GlowColor, 4.f));
			Shell->SetupAttachment(Mesh);
			Shell->SetRelativeScale3D(FVector(1.3f));
			Shell->RegisterComponent();

			UMaterialInstanceDynamic* PuffMaterial = ShinobiVfx::MakeFlame(this, GlowColor, 4.f);
			for (int32 Index = 0; Index < 16; ++Index)
			{
				UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(this);
				Puff->SetStaticMesh(Mesh->GetStaticMesh());
				Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Puff->SetCastShadow(false);
				Puff->SetMaterial(0, PuffMaterial);
				Puff->SetupAttachment(Collision);
				Puff->SetUsingAbsoluteLocation(true); // dropped in the world, left behind as the ball flies on
				Puff->SetUsingAbsoluteScale(true);
				Puff->SetVisibility(false);
				Puff->RegisterComponent();
				TrailPuffs.Add(Puff);
				TrailAges.Add(1.f);
			}
			SetActorTickEnabled(true);
		}
	}
}

void AShinobiProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	constexpr float PuffLife = 0.45f;
	constexpr float PuffInterval = 0.03f;
	SinceLastPuff += DeltaSeconds;
	if (SinceLastPuff >= PuffInterval && TrailPuffs.Num())
	{
		SinceLastPuff = 0.f;
		TrailPuffs[NextPuff]->SetWorldLocation(GetActorLocation() + FMath::VRand() * 6.f * Scale);
		TrailAges[NextPuff] = 0.f;
		NextPuff = (NextPuff + 1) % TrailPuffs.Num();
	}
	for (int32 Index = 0; Index < TrailPuffs.Num(); ++Index)
	{
		TrailAges[Index] += DeltaSeconds;
		const float Left = 1.f - TrailAges[Index] / PuffLife;
		TrailPuffs[Index]->SetVisibility(Left > 0.f);
		if (Left > 0.f)
		{
			TrailPuffs[Index]->SetWorldScale3D(FVector(0.6f * Scale * Left));
		}
	}
}

void AShinobiProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiProjectile, Scale, COND_InitialOnly);
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
	if (bBurstOnImpact)
	{
		AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::Fire, Location, Scale);
	}

	AShinobiCharacterBase* Caster = Cast<AShinobiCharacterBase>(GetInstigator());
	if (bGroundFire)
	{
		AShinobiFirePatch::Spawn(GetWorld(), EShinobiFirePatch::Fire, Location, Caster, Scale, StatusDamagePerTick);
	}
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

AShinobiKunai::AShinobiKunai()
{
	GlowColor = FLinearColor(0.35f, 0.37f, 0.4f);
	GlowIntensity = 0.6f;
	bBurstOnImpact = false;
	bFlameShell = false;
	bGroundFire = false;
	Collision->SetSphereRadius(12.f);
	Movement->InitialSpeed = 3000.f;
	Movement->MaxSpeed = 3000.f;
	Light->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (Cone.Succeeded())
	{
		Mesh->SetStaticMesh(Cone.Object);
	}
	// The cone points up (+Z); lay it along the flight direction (+X) as a thin blade.
	Mesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Mesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.45f));
}
