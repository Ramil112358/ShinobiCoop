#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "Shinobi/VFX/ShinobiBurst.h"

namespace
{
	struct FSummonStats
	{
		float Health;
		float Damage;       // per hit of the shared melee (two hits per attack)
		float AttackRange;
		float AttackInterval;
		float Speed;
		float Lifetime;
		float Scale;
		FLinearColor Tint;  // the placeholder mannequin's colour, until the creature's own model is worn
		int32 Count;
	};

	const FSummonStats& SummonStatsFor(EShinobiSummonKind Kind)
	{
		static const FSummonStats Table[] = {
			{ 600.f, 30.f, 350.f, 2.5f, 350.f, 12.f, 2.5f, FLinearColor(0.1f, 0.4f, 0.1f), 1 },
			{ 350.f, 35.f, 300.f, 2.0f, 500.f, 10.f, 1.6f, FLinearColor(0.35f, 0.1f, 0.5f), 1 },
			{ 120.f, 18.f, 170.f, 0.8f, 800.f, 15.f, 0.8f, FLinearColor(0.15f, 0.15f, 0.17f), AShinobiSummon::DogPackSize },
		};
		return Table[static_cast<int32>(Kind)];
	}
}

AShinobiSummon::AShinobiSummon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	AttackAbility = UGA_EnemyMelee::StaticClass();
	StartupAbilities = { AttackAbility };
	UseMannequinMesh();
}

void AShinobiSummon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiSummon, Kind, COND_InitialOnly);
}

float AShinobiSummon::GetLifetime(EShinobiSummonKind Kind)
{
	return SummonStatsFor(Kind).Lifetime;
}

void AShinobiSummon::Configure()
{
	const FSummonStats& Stats = SummonStatsFor(Kind);
	BaseHealth = Stats.Health;
	AttackDamage = Stats.Damage;
	AttackRange = Stats.AttackRange;
	AttackInterval = Stats.AttackInterval;
	BodyTint = Stats.Tint;
	GetCharacterMovement()->MaxWalkSpeed = Stats.Speed;
	SetActorScale3D(FVector(Stats.Scale));
}

void AShinobiSummon::BeginPlay()
{
	Configure();
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(LifetimeTimer, this, &AShinobiSummon::Dismiss, SummonStatsFor(Kind).Lifetime);
	}
}

void AShinobiSummon::Dismiss()
{
	if (!IsAlive() || IsActorBeingDestroyed())
	{
		return;
	}
	AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::SummonPoof, GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), GetActorScale3D().X);
	Destroy();
}

TArray<AShinobiSummon*> AShinobiSummon::Summon(AShinobiCharacterBase* Caster, EShinobiSummonKind Kind, const FVector& Ground)
{
	TArray<AShinobiSummon*> Pack;
	UWorld* World = Caster ? Caster->GetWorld() : nullptr;
	if (!World || !Caster->HasAuthority())
	{
		return Pack;
	}
	TArray<AShinobiSummon*> Previous;
	for (TActorIterator<AShinobiSummon> It(World); It; ++It)
	{
		if (It->Summoner == Caster && It->Kind == Kind)
		{
			Previous.Add(*It);
		}
	}
	for (AShinobiSummon* Old : Previous)
	{
		Old->Dismiss();
	}

	const FSummonStats& Stats = SummonStatsFor(Kind);
	const float HalfHeight = GetDefault<AShinobiSummon>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * Stats.Scale;
	const float Yaw = Caster->GetActorRotation().Yaw;
	for (int32 Index = 0; Index < Stats.Count; ++Index)
	{
		const float Angle = 2.f * PI * Index / Stats.Count;
		const FVector Spot = Ground + (Stats.Count > 1 ? FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 160.f : FVector::ZeroVector);
		const FTransform Where(FRotator(0.f, Yaw, 0.f), Spot + FVector(0.f, 0.f, HalfHeight), FVector(Stats.Scale));
		AShinobiSummon* Summon = World->SpawnActorDeferred<AShinobiSummon>(StaticClass(), Where, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Summon)
		{
			continue;
		}
		Summon->Kind = Kind;
		Summon->Summoner = Caster;
		Summon->FinishSpawning(Where);
		Pack.Add(Summon);
		AShinobiBurst::Spawn(World, EShinobiBurst::SummonPoof, Spot, Stats.Scale);
	}
	return Pack;
}
