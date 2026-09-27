#include "Shinobi/World/ShinobiWorldGameMode.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/World/ShinobiCamp.h"

AShinobiWorldGameMode::AShinobiWorldGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AShinobiPlayerCharacter::StaticClass();
	PlayerControllerClass = AShinobiPlayerController::StaticClass();
	GameStateClass = AShinobiGameState::StaticClass();
}

AShinobiGameState* AShinobiWorldGameMode::GetWorldState() const
{
	return GetGameState<AShinobiGameState>();
}

bool AShinobiWorldGameMode::ReadMarker(const AActor& Marker, AShinobiCamp& Camp)
{
	if (!Marker.ActorHasTag(TEXT("ShinobiCampSite")))
	{
		return false;
	}
	for (const FName& Tag : Marker.Tags)
	{
		FString Key, Value;
		const FString Text = Tag.ToString();
		if (Text == TEXT("Boss"))
		{
			Camp.bBoss = true;
			Camp.CampId = TEXT("boss");
		}
		else if (Text.Split(TEXT(":"), &Key, &Value))
		{
			if (Key == TEXT("Camp"))
			{
				Camp.CampId = FName(Value);
			}
			else if (Key == TEXT("Radius"))
			{
				Camp.Radius = FCString::Atof(*Value);
			}
			else if (Key == TEXT("Difficulty"))
			{
				Camp.Difficulty = FCString::Atoi(*Value);
			}
			else if (Key == TEXT("Waves"))
			{
				TArray<FString> Waves;
				Value.ParseIntoArray(Waves, TEXT(";"));
				for (const FString& Wave : Waves)
				{
					FString Ninjas, Throwers;
					if (Wave.Split(TEXT(","), &Ninjas, &Throwers))
					{
						Camp.Waves.Add({ FCString::Atoi(*Ninjas), FCString::Atoi(*Throwers) });
					}
				}
			}
		}
	}
	return Camp.bBoss || Camp.Waves.Num() > 0;
}

void AShinobiWorldGameMode::BeginPlay()
{
	Super::BeginPlay();
	AShinobiGameState* State = GetWorldState();
	State->Phase = EShinobiArenaPhase::OpenWorld;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("ShinobiCampSite")))
		{
			continue;
		}
		const FTransform Where(FRotator::ZeroRotator, It->GetActorLocation());
		AShinobiCamp* Camp = GetWorld()->SpawnActorDeferred<AShinobiCamp>(AShinobiCamp::StaticClass(), Where);
		if (!ReadMarker(**It, *Camp))
		{
			Camp->Destroy();
			continue;
		}
		Camp->FinishSpawning(Where);
		Camp->OnCleared.AddWeakLambda(this, [this, Camp]() { OnCampCleared(Camp); });
		Camps.Add(Camp);
		State->NumCamps += Camp->bBoss ? 0 : 1;
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("World: %d camps (+ boss: %s)"), State->NumCamps, Camps.Num() > State->NumCamps ? TEXT("yes") : TEXT("no"));
}

void AShinobiWorldGameMode::OnCampCleared(AShinobiCamp* Camp)
{
	AShinobiGameState* State = GetWorldState();
	if (Camp->bBoss)
	{
		State->Phase = EShinobiArenaPhase::Victory;
		UE_LOG(LogShinobiCoop, Log, TEXT("World: valley cleared"));
		return;
	}
	++State->CampsCleared;
}

void AShinobiWorldGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	bool bAnyPlayer = false, bAnyAlive = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		const AShinobiCharacterBase* Body = Controller ? Cast<AShinobiCharacterBase>(Controller->GetPawn()) : nullptr;
		if (!Body)
		{
			continue;
		}
		bAnyPlayer = true;
		bAnyAlive |= Body->IsAlive();
		if (Body->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Dead))
		{
			const float& Since = DeadSince.FindOrAdd(Controller, Now);
			if (Now - Since >= RespawnSeconds)
			{
				Respawn(Controller);
			}
		}
	}
	if (bAnyPlayer && !bAnyAlive)
	{
		UE_LOG(LogShinobiCoop, Log, TEXT("World: wipe, everyone back to the village"));
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			Respawn(It->Get());
		}
		for (AShinobiCamp* Camp : Camps)
		{
			Camp->ResetEncounter();
		}
	}
}

void AShinobiWorldGameMode::Respawn(APlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}
	DeadSince.Remove(Controller);
	if (APawn* Body = Controller->GetPawn())
	{
		Controller->UnPossess();
		Body->Destroy();
	}
	RestartPlayer(Controller);
}
