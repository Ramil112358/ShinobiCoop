#include "Shinobi/UI/ShinobiMenuGameMode.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "ShinobiCoop.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"

AShinobiMenuGameMode::AShinobiMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AShinobiMenuPlayerController::StaticClass();
}

void AShinobiMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(this, UShinobiMainMenuWidget::StaticClass());
	Menu->AddToViewport();
	SetShowMouseCursor(true);
	SetInputMode(FInputModeUIOnly()); // the menu focuses its first button itself

	UShinobiSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UShinobiSessionSubsystem>();
	if (FParse::Param(FCommandLine::Get(), TEXT("ShinobiAutoHost")))
	{
		// -ShinobiAutoHostArena=<WestEnd|Camden|Westminster|Tower> hosts that PvP arena, in -ShinobiAutoHostFormat=1v1|2v2 (1v1 if absent).
		FString ArenaId, FormatId;
		int32 MapIndex = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("ShinobiAutoHostArena="), ArenaId))
		{
			const TArray<FShinobiMap>& Maps = UShinobiSessionSubsystem::GetMaps();
			const int32 Found = Maps.IndexOfByPredicate([&ArenaId](const FShinobiMap& Map)
			{
				return Map.bArena && FString(Map.Path).EndsWith(TEXT("Lvl_Arena_") + ArenaId, ESearchCase::IgnoreCase);
			});
			if (Found == INDEX_NONE)
			{
				UE_LOG(LogShinobiCoop, Warning, TEXT("-ShinobiAutoHostArena=%s matches no arena, hosting the default map"), *ArenaId);
			}
			else
			{
				MapIndex = Found;
			}
		}
		FParse::Value(FCommandLine::Get(), TEXT("ShinobiAutoHostFormat="), FormatId);
		Sessions->HostGame(MapIndex, FormatId == TEXT("2v2") ? EShinobiPvpFormat::TwoVsTwo : EShinobiPvpFormat::OneVsOne);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("ShinobiAutoJoin")))
	{
		GetWorldTimerManager().SetTimer(AutoTimer, this, &AShinobiMenuPlayerController::AutoJoinStep, 2.f, true, 1.f);
	}
}

void AShinobiMenuPlayerController::AutoJoinStep()
{
	UShinobiSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UShinobiSessionSubsystem>();
	if (Sessions->IsBusy())
	{
		return;
	}
	if (Sessions->GetFoundGames().Num() > 0)
	{
		GetWorldTimerManager().ClearTimer(AutoTimer);
		Sessions->JoinGame(0);
		return;
	}
	Sessions->FindGames();
}
