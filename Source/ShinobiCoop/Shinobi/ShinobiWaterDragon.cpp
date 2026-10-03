#include "Shinobi/ShinobiWaterDragon.h"
#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr int32 DragonBodyParts = 14;
	constexpr float DragonPartSpacing = 85.f;
}

AShinobiWaterDragon::AShinobiWaterDragon()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AShinobiWaterDragon* AShinobiWaterDragon::Spawn(UWorld* World, AShinobiCharacterBase* Caster, const FVector& Start, const FVector& Direction, float Speed, float Lifetime, float VisualScale, const FShinobiDragonHit& Hit)
{
	if (!World || World->GetNetMode() == NM_Client || Direction.IsNearlyZero())
	{
		return nullptr;
	}
	const FTransform At(Direction.Rotation(), Start);
	AShinobiWaterDragon* Dragon = World->SpawnActorDeferred<AShinobiWaterDragon>(StaticClass(), At, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Dragon)
	{
		Dragon->StartPoint = Start;
		Dragon->Heading = Direction.GetSafeNormal();
		Dragon->Speed = Speed;
		Dragon->Lifetime = Lifetime;
		Dragon->VisualScale = VisualScale;
		Dragon->HitData = Hit;
		Dragon->Caster = Caster;
		Dragon->FinishSpawning(At);
	}
	return Dragon;
}

void AShinobiWaterDragon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiWaterDragon, StartPoint, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiWaterDragon, Heading, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiWaterDragon, Speed, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiWaterDragon, Lifetime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiWaterDragon, VisualScale, COND_InitialOnly);
}

FVector AShinobiWaterDragon::HeadAt(float Time) const
{
	return StartPoint + Heading * Speed * FMath::Clamp(Time, 0.f, Lifetime);
}

void AShinobiWaterDragon::BeginPlay()
{
	Super::BeginPlay();
	LastHead = StartPoint;
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	// A head (with two horns) and a tapering body of spheres in flowing water; the shapes are the engine's.
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterialInstanceDynamic* Water = ShinobiVfx::MakeWater(this, FLinearColor(0.06f, 0.35f, 1.f), 1.1f);
	for (int32 Index = 0; Index <= DragonBodyParts; ++Index)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetStaticMesh(Sphere);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetMaterial(0, Water);
		Part->SetupAttachment(RootComponent);
		Part->SetUsingAbsoluteLocation(true);
		Part->SetUsingAbsoluteRotation(true);
		const float Size = Index == 0 ? 1.5f : FMath::Max(1.25f - 0.06f * Index, 0.4f);
		Part->SetRelativeScale3D(FVector(Size * VisualScale));
		Part->RegisterComponent();
		Parts.Add(Part);
	}
	for (const float Side : { -1.f, 1.f })
	{
		UStaticMeshComponent* Horn = NewObject<UStaticMeshComponent>(this);
		Horn->SetStaticMesh(Cone);
		Horn->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Horn->SetCastShadow(false);
		Horn->SetMaterial(0, Water);
		Horn->SetupAttachment(Parts[0]);
		Horn->SetRelativeLocation(FVector(-25.f, 45.f * Side, 55.f)); // swept back and up from the head
		Horn->SetRelativeRotation(FRotator(-50.f, 0.f, 25.f * Side));
		Horn->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.9f));
		Horn->RegisterComponent();
	}
}

void AShinobiWaterDragon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const FVector Head = HeadAt(Age);

	if (HasAuthority())
	{
		// Stop at the first piece of the world in the way: by object type, so characters (which are caught, not blocking)
		// and the caster's own capsule never stop it.
		FHitResult Block;
		const FCollisionShape Shape = FCollisionShape::MakeSphere(60.f);
		if (GetWorld()->SweepSingleByObjectType(Block, LastHead, Head, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Shape))
		{
			Catch(LastHead, Block.Location);
			Finish(Block.Location);
			return;
		}
		Catch(LastHead, Head);
		LastHead = Head;
		if (Age >= Lifetime)
		{
			Finish(Head);
			return;
		}
	}

	if (Parts.Num() > 0)
	{
		const FVector Right = FVector::CrossProduct(Heading, FVector::UpVector).GetSafeNormal();
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			// Each part sits where the head was a moment ago, swaying side to side in a travelling wave.
			const FVector Along = HeadAt(Age - Index * DragonPartSpacing * VisualScale / Speed);
			const FVector Sway = Right * FMath::Sin(Age * 9.f - Index * 0.7f) * 70.f * VisualScale
				+ FVector::UpVector * FMath::Cos(Age * 7.f - Index * 0.5f) * 35.f * VisualScale;
			Parts[Index]->SetWorldLocation(Along + Sway * FMath::Min(Index * 0.3f, 1.f));
		}
	}
}

void AShinobiWaterDragon::Catch(const FVector& From, const FVector& To)
{
	AShinobiCharacterBase* Thrower = Caster.Get();
	if (!Thrower)
	{
		return;
	}
	const FVector Middle = (From + To) * 0.5f;
	const float Reach = FVector::Dist(From, To) * 0.5f + HitData.HitRadius;
	UAbilitySystemComponent* Source = Thrower->GetAbilitySystemComponent();
	for (AShinobiCharacterBase* Victim : ShinobiCombat::FindHostilesInRadius(Thrower, Middle, Reach + 100.f))
	{
		const FVector Closest = FMath::ClosestPointOnSegment(Victim->GetActorLocation(), From, To);
		if (FVector::Dist(Closest, Victim->GetActorLocation()) > HitData.HitRadius || Caught.Contains(Victim))
		{
			continue;
		}
		Caught.Add(Victim);
		if (ShinobiCombat::ApplyDamage(Source, Victim, HitData.Damage))
		{
			ShinobiCombat::ApplyStatus(Source, Victim, UGE_Stagger::StaticClass());
			Victim->LaunchCharacter(Heading * HitData.Knockback + FVector(0.f, 0.f, 250.f), true, true);
		}
	}
}

void AShinobiWaterDragon::Finish(const FVector& Where)
{
	AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::DragonSplash, Where, VisualScale);
	Destroy();
}
