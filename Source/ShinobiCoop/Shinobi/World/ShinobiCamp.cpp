#include "Shinobi/World/ShinobiCamp.h"
#include "AbilitySystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	const FLinearColor Fire(1.f, 0.35f, 0.08f);
	const FLinearColor ClearedFlame(0.2f, 0.55f, 1.f);
}

AShinobiCamp::AShinobiCamp()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true; // the compass shows every camp's state

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Flame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flame"));
	Flame->SetupAttachment(RootComponent);
	Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flame->SetCastShadow(false);
	Flame->SetRelativeLocation(FVector(0.f, 0.f, -10.f));
	Flame->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.9f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (Cone.Succeeded())
	{
		Flame->SetStaticMesh(Cone.Object);
	}
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Light->SetIntensity(6000.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetCastShadows(false);
}

void AShinobiCamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShinobiCamp, State);
}

void AShinobiCamp::BeginPlay()
{
	Super::BeginPlay();
	UpdateFlame();
}

void AShinobiCamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	ThinkClock += DeltaSeconds;
	if (ThinkClock < ThinkInterval)
	{
		return;
	}
	const float Step = ThinkClock;
	ThinkClock = 0.f;
	switch (State)
	{
	case EShinobiCampState::Idle:
		if (PlayersWithin(Radius) > 0)
		{
			Activate();
		}
		break;
	case EShinobiCampState::Active:
		Encounter.Prune();
		if (PlayersWithin(Radius * LeashFactor) == 0)
		{
			EmptyTime += Step;
			if (EmptyTime >= ResetSeconds)
			{
				UE_LOG(LogShinobiCoop, Log, TEXT("Camp %s: abandoned, resetting"), *CampId.ToString());
				ResetEncounter();
			}
			break;
		}
		EmptyTime = 0.f;
		if (Encounter.Remaining() == 0)
		{
			if (!bBoss && WaveIndex + 1 < Waves.Num())
			{
				SpawnWave(++WaveIndex);
			}
			else
			{
				Clear();
			}
		}
		break;
	default:
		break;
	}
}

int32 AShinobiCamp::PlayersWithin(float Distance) const
{
	int32 Count = 0;
	for (TActorIterator<AShinobiPlayerCharacter> It(GetWorld()); It; ++It)
	{
		Count += It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Distance ? 1 : 0;
	}
	return Count;
}

int32 AShinobiCamp::NumPlayers() const
{
	int32 Count = 0;
	for (TActorIterator<AShinobiPlayerCharacter> It(GetWorld()); It; ++It)
	{
		++Count;
	}
	return FMath::Max(1, Count);
}

void AShinobiCamp::Activate()
{
	State = EShinobiCampState::Active;
	WaveIndex = 0;
	EmptyTime = 0.f;
	bBossSummoned = false;
	UE_LOG(LogShinobiCoop, Log, TEXT("Camp %s: players arrived (difficulty %d)"), *CampId.ToString(), Difficulty);
	SpawnWave(0);
}

void AShinobiCamp::SpawnWave(int32 Index)
{
	if (bBoss)
	{
		if (AShinobiEnemyCharacter* Boss = Encounter.Spawn(this, AShinobiRogueJonin::StaticClass(), SpawnPoint(0, 1), GetActorLocation(), NumPlayers()))
		{
			Boss->GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(UShinobiAttributeSet::GetHealthAttribute())
				.AddUObject(this, &AShinobiCamp::OnBossHealthChanged);
		}
		return;
	}
	if (!Waves.IsValidIndex(Index))
	{
		return;
	}
	const FShinobiCampWave& Wave = Waves[Index];
	const int32 Total = Wave.Ninjas + Wave.Throwers;
	for (int32 Slot = 0; Slot < Total; ++Slot)
	{
		const TSubclassOf<AShinobiEnemyCharacter> Class = Slot < Wave.Ninjas ? TSubclassOf<AShinobiEnemyCharacter>(AShinobiRogueNinja::StaticClass())
			: TSubclassOf<AShinobiEnemyCharacter>(AShinobiKunaiThrower::StaticClass());
		Encounter.Spawn(this, Class, SpawnPoint(Slot, Total), GetActorLocation(), NumPlayers());
	}
}

FVector AShinobiCamp::SpawnPoint(int32 Slot, int32 NumSlots) const
{
	const float Angle = UE_TWO_PI * Slot / FMath::Max(1, NumSlots) + FMath::FRandRange(-0.3f, 0.3f);
	const float Distance = bBoss ? 0.f : Radius * 0.35f;
	const FVector Wanted = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
	// On the navmesh if there is one near; else on whatever floor is under the point; else the camp centre's height.
	if (const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation OnNav;
		if (Nav->ProjectPointToNavigation(Wanted, OnNav, FVector(500.f, 500.f, 1000.f)))
		{
			return OnNav.Location + FVector(0.f, 0.f, 100.f);
		}
	}
	return FShinobiEncounter::DropToFloor(GetWorld(), Wanted);
}

void AShinobiCamp::OnBossHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bBossSummoned || State != EShinobiCampState::Active || Data.NewValue <= 0.f)
	{
		return;
	}
	const TArray<TWeakObjectPtr<AShinobiEnemyCharacter>>& Enemies = Encounter.GetEnemies();
	const AShinobiEnemyCharacter* Boss = Enemies.Num() > 0 ? Enemies[0].Get() : nullptr;
	if (!Boss || Data.NewValue >= Boss->GetAttributes()->GetMaxHealth() * 0.5f)
	{
		return;
	}
	bBossSummoned = true;
	const FVector Side = Boss->GetActorRightVector() * 250.f;
	for (int32 i = 0; i < 2; ++i)
	{
		Encounter.Spawn(this, AShinobiRogueNinja::StaticClass(), FShinobiEncounter::DropToFloor(GetWorld(), Boss->GetActorLocation() + (i == 0 ? Side : -Side)),
			GetActorLocation(), NumPlayers());
	}
}

void AShinobiCamp::Clear()
{
	State = EShinobiCampState::Cleared;
	Encounter.KillAll(); // a boss's summons fall with it
	UE_LOG(LogShinobiCoop, Log, TEXT("Camp %s: cleared"), *CampId.ToString());
	UpdateFlame();
	OnCleared.Broadcast();
}

void AShinobiCamp::ResetEncounter()
{
	if (State != EShinobiCampState::Active)
	{
		return;
	}
	Encounter.DespawnAll();
	State = EShinobiCampState::Idle;
	EmptyTime = 0.f;
	UpdateFlame();
}

void AShinobiCamp::OnRep_State()
{
	UpdateFlame();
}

void AShinobiCamp::UpdateFlame()
{
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	const FLinearColor Colour = State == EShinobiCampState::Cleared ? ClearedFlame : Fire;
	if (UMaterialInstanceDynamic* Glow = ShinobiVfx::MakeGlow(this, Colour, 6.f, 1.f))
	{
		Flame->SetMaterial(0, Glow);
	}
	Light->SetLightColor(Colour);
}
