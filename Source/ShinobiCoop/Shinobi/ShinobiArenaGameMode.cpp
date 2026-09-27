#include "Shinobi/ShinobiArenaGameMode.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"

namespace
{
	FAutoConsoleCommandWithWorld CmdKillEnemies(
		TEXT("Shinobi.KillEnemies"),
		TEXT("Dev aid (host only): kills every enemy in the arena, to skip ahead through the waves."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (AShinobiArenaGameMode* Arena = World ? World->GetAuthGameMode<AShinobiArenaGameMode>() : nullptr)
			{
				Arena->KillAllEnemies();
			}
		}));

	// ponytail: fixed table; a data asset when there's more than one arena
	const FShinobiWave Waves[] = {
		{ 4, 0, false },
		{ 4, 3, false },
		{ 6, 4, false },
		{ 0, 0, true },
	};
}

const FName AShinobiArenaGameMode::SpawnPointTag(TEXT("ShinobiEnemySpawn"));

AShinobiArenaGameMode::AShinobiArenaGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AShinobiPlayerCharacter::StaticClass();
	PlayerControllerClass = AShinobiPlayerController::StaticClass();
	GameStateClass = AShinobiGameState::StaticClass();
}

int32 AShinobiArenaGameMode::GetNumWaves()
{
	return UE_ARRAY_COUNT(Waves);
}

FShinobiWave AShinobiArenaGameMode::GetWave(int32 Wave)
{
	return Waves[FMath::Clamp(Wave, 1, GetNumWaves()) - 1];
}

float AShinobiArenaGameMode::GetHealthScale(int32 NumPlayers)
{
	return FShinobiEncounter::GetHealthScale(NumPlayers);
}

AShinobiGameState* AShinobiArenaGameMode::GetArenaState() const
{
	return GetGameState<AShinobiGameState>();
}

void AShinobiArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (TActorIterator<APlayerStart> Start(GetWorld()); Start)
	{
		ArenaCenter = Start->GetActorLocation();
	}
	else
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("AShinobiArenaGameMode: no PlayerStart, enemies spawn around the origin"));
	}
	for (TActorIterator<ATargetPoint> Point(GetWorld()); Point; ++Point)
	{
		if (Point->ActorHasTag(SpawnPointTag))
		{
			SpawnPoints.Add(Point->GetActorLocation());
		}
	}
	GetArenaState()->NumWaves = GetNumWaves();
	Encounter.OnEnemyDied = [this](AShinobiEnemyCharacter* Enemy) { OnEnemyDied(Enemy); };
	StartBreak();
}

void AShinobiArenaGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AShinobiGameState* State = GetArenaState();
	const bool bTimeUp = State->PhaseEndTime > 0.f && GetWorld()->GetTimeSeconds() >= State->PhaseEndTime;
	switch (State->Phase)
	{
	case EShinobiArenaPhase::Break:
		if (bTimeUp)
		{
			StartWave();
		}
		break;
	case EShinobiArenaPhase::Wiped:
		if (bTimeUp)
		{
			RefillPlayers();
			StartWave(); // same wave again
		}
		break;
	case EShinobiArenaPhase::Fighting:
	{
		bool bAnyPlayer = false;
		if (!AnyPlayerAlive(bAnyPlayer) && bAnyPlayer)
		{
			Wipe();
			break;
		}
		// An enemy destroyed without dying (fell out of the world) never reports in: count it out here.
		if (Encounter.Prune() > 0)
		{
			State->EnemiesRemaining = Encounter.Remaining();
			if (GetWave(State->Wave).bBoss ? !Boss.IsValid() : State->EnemiesRemaining == 0)
			{
				GetWave(State->Wave).bBoss ? Win() : StartBreak();
			}
		}
		break;
	}
	default:
		break;
	}
}

void AShinobiArenaGameMode::SetPhase(EShinobiArenaPhase NewPhase, float Seconds)
{
	AShinobiGameState* State = GetArenaState();
	State->Phase = NewPhase;
	State->PhaseEndTime = Seconds > 0.f ? GetWorld()->GetTimeSeconds() + Seconds : 0.f;
}

void AShinobiArenaGameMode::StartBreak()
{
	RefillPlayers();
	++GetArenaState()->Wave;
	SetPhase(EShinobiArenaPhase::Break, BreakSeconds);
}

void AShinobiArenaGameMode::StartWave()
{
	RespawnDeadPlayers();
	AShinobiGameState* State = GetArenaState();
	State->EnemiesRemaining = 0;
	Boss = nullptr;
	bBossSummoned = false;
	SetPhase(EShinobiArenaPhase::Fighting, 0.f);

	const FShinobiWave Wave = GetWave(State->Wave);
	const int32 NumSlots = Wave.RogueNinjas + Wave.KunaiThrowers + (Wave.bBoss ? 1 : 0);
	int32 Slot = 0;
	for (int32 i = 0; i < Wave.RogueNinjas; ++i)
	{
		SpawnEnemy(AShinobiRogueNinja::StaticClass(), GetRingPoint(Slot++, NumSlots));
	}
	for (int32 i = 0; i < Wave.KunaiThrowers; ++i)
	{
		SpawnEnemy(AShinobiKunaiThrower::StaticClass(), GetRingPoint(Slot++, NumSlots));
	}
	if (Wave.bBoss)
	{
		Boss = Cast<AShinobiRogueJonin>(SpawnEnemy(AShinobiRogueJonin::StaticClass(), GetRingPoint(Slot++, NumSlots)));
		if (Boss.IsValid())
		{
			Boss->GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(UShinobiAttributeSet::GetHealthAttribute())
				.AddUObject(this, &AShinobiArenaGameMode::OnBossHealthChanged);
		}
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("Arena: wave %d/%d, %d enemies"), State->Wave, State->NumWaves, State->EnemiesRemaining);
}

void AShinobiArenaGameMode::Wipe()
{
	Encounter.DespawnAll();
	GetArenaState()->EnemiesRemaining = 0;
	SetPhase(EShinobiArenaPhase::Wiped, WipeSeconds);
	UE_LOG(LogShinobiCoop, Log, TEXT("Arena: wiped on wave %d"), GetArenaState()->Wave);
}

void AShinobiArenaGameMode::Win()
{
	SetPhase(EShinobiArenaPhase::Victory, 0.f);
	KillAllEnemies(); // the boss's summons fall with it
	UE_LOG(LogShinobiCoop, Log, TEXT("Arena: victory"));
}

void AShinobiArenaGameMode::KillAllEnemies()
{
	Encounter.KillAll();
}

void AShinobiArenaGameMode::RefillPlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(It->Get() ? It->Get()->GetPawn() : nullptr);
		if (!Player || Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Dead))
		{
			continue;
		}
		Player->Revive();
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		const UShinobiAttributeSet* Stats = Player->GetAttributes();
		ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetHealthAttribute(), Stats->GetMaxHealth());
		ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetChakraAttribute(), Stats->GetMaxChakra());
		ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetStaminaAttribute(), Stats->GetMaxStamina());
	}
}

void AShinobiArenaGameMode::RespawnDeadPlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Body = PC ? PC->GetPawn() : nullptr;
		const AShinobiCharacterBase* Shinobi = Cast<AShinobiCharacterBase>(Body);
		if (Shinobi && Shinobi->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Dead))
		{
			PC->UnPossess();
			Body->Destroy();
			RestartPlayer(PC);
		}
	}
}

bool AShinobiArenaGameMode::AnyPlayerAlive(bool& bOutAnyPlayer) const
{
	bOutAnyPlayer = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const AShinobiCharacterBase* Player = Cast<AShinobiCharacterBase>(It->Get() ? It->Get()->GetPawn() : nullptr))
		{
			bOutAnyPlayer = true;
			if (Player->IsAlive())
			{
				return true;
			}
		}
	}
	return false;
}

FVector AShinobiArenaGameMode::GetRingPoint(int32 Slot, int32 NumSlots) const
{
	FVector Point;
	if (SpawnPoints.Num() > 0)
	{
		// Round-robin over the level's spawn points, scattered so a group doesn't stack up.
		Point = SpawnPoints[Slot % SpawnPoints.Num()] + FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 0.f);
	}
	else
	{
		const float Angle = UE_TWO_PI * Slot / FMath::Max(1, NumSlots) + FMath::FRandRange(-0.15f, 0.15f);
		Point = ArenaCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * SpawnRadius;
	}
	return FShinobiEncounter::DropToFloor(GetWorld(), Point); // without a floor, keep the PlayerStart's height
}

AShinobiEnemyCharacter* AShinobiArenaGameMode::SpawnEnemy(TSubclassOf<AShinobiEnemyCharacter> EnemyClass, const FVector& Location)
{
	AShinobiEnemyCharacter* Enemy = Encounter.Spawn(this, EnemyClass, Location, ArenaCenter, GetNumPlayers());
	if (Enemy)
	{
		++GetArenaState()->EnemiesRemaining;
	}
	return Enemy;
}

void AShinobiArenaGameMode::OnEnemyDied(AShinobiEnemyCharacter* Enemy)
{
	AShinobiGameState* State = GetArenaState();
	State->EnemiesRemaining = FMath::Max(0, State->EnemiesRemaining - 1);
	if (State->Phase != EShinobiArenaPhase::Fighting)
	{
		return;
	}
	if (Enemy == Boss.Get())
	{
		Win();
	}
	else if (State->EnemiesRemaining == 0 && !GetWave(State->Wave).bBoss)
	{
		StartBreak();
	}
}

void AShinobiArenaGameMode::OnBossHealthChanged(const FOnAttributeChangeData& Data)
{
	const AShinobiRogueJonin* Jonin = Boss.Get();
	if (bBossSummoned || !Jonin || Data.NewValue <= 0.f || Data.NewValue >= Jonin->GetAttributes()->GetMaxHealth() * 0.5f
		|| GetArenaState()->Phase != EShinobiArenaPhase::Fighting)
	{
		return;
	}
	bBossSummoned = true;
	const FVector Side = Jonin->GetActorRightVector() * 250.f;
	for (int32 i = 0; i < BossSummonCount; ++i)
	{
		SpawnEnemy(AShinobiRogueNinja::StaticClass(), Jonin->GetActorLocation() + (i % 2 == 0 ? Side : -Side));
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("Arena: the boss calls for help"));
}
