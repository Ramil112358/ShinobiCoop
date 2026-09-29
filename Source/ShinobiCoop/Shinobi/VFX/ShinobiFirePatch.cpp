#include "Shinobi/VFX/ShinobiFirePatch.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr int32 NumTongues = 12;
	constexpr float FadeTime = 0.5f;
	const FVector TongueScale(0.25f, 0.25f, 0.8f); // the engine sphere is 100 units across: a 25 x 80 tongue
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
	UMaterialInstanceDynamic* Flame = ShinobiVfx::MakeFlame(this, Color, bBlack ? 3.f : 4.f, bBlack);
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	FRandomStream Random(GetUniqueID()); // cosmetic: each machine may scatter the tongues differently
	for (int32 Index = 0; Index < NumTongues; ++Index)
	{
		UStaticMeshComponent* Tongue = NewObject<UStaticMeshComponent>(this);
		Tongue->SetStaticMesh(Sphere);
		Tongue->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tongue->SetCastShadow(false);
		Tongue->SetMaterial(0, Flame);
		Tongue->SetupAttachment(RootComponent);
		// Spread over most of the radius, taller in the middle.
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		const FVector2D Spot = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Random.FRandRange(0.f, 0.7f) * GetRadius();
		Tongue->SetRelativeLocation(FVector(Spot, 0.f));
		Tongue->RegisterComponent();
		Tongues.Add(Tongue);
	}
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(GetRadius() * 3.f);
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
	// Rise in quickly, flicker, shrink away over the last FadeTime.
	const float Life = FMath::Min(FMath::Clamp(Age / 0.2f, 0.f, 1.f), FMath::Clamp((Duration - Age) / FadeTime, 0.f, 1.f));
	for (int32 Index = 0; Index < Tongues.Num(); ++Index)
	{
		const float Centre = 1.f - Tongues[Index]->GetRelativeLocation().Size2D() / FMath::Max(GetRadius(), 1.f);
		const FVector Size = TongueScale * Scale * Life * FVector(1.f, 1.f, FMath::FRandRange(0.75f, 1.25f) * (0.6f + 0.6f * Centre));
		Tongues[Index]->SetRelativeScale3D(Size);
		// The engine sphere sits on its centre: lift each tongue by half its height so it stands on the ground.
		Tongues[Index]->SetRelativeLocation(FVector(Tongues[Index]->GetRelativeLocation().X, Tongues[Index]->GetRelativeLocation().Y, Size.Z * 50.f));
	}
	Light->SetIntensity((Kind == EShinobiFirePatch::BlackFlame ? 800.f : 4000.f) * Life * FMath::FRandRange(0.8f, 1.1f));
}
