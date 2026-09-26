#include "Shinobi/UI/ShinobiMenuGameMode.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
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
		Sessions->HostGame();
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
