#include "Shinobi/UI/ShinobiMenuShots.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiMainMenuWidget.h"

namespace
{
	UShinobiMainMenuWidget* MenuShotsFindMenu(UWorld& World)
	{
		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(&World, Found, UShinobiMainMenuWidget::StaticClass(), false);
		return Found.Num() > 0 ? Cast<UShinobiMainMenuWidget>(Found[0]) : nullptr;
	}

	void MenuShotsTake(const FString& Name)
	{
		FScreenshotRequest::RequestScreenshot(Name, true, false);
	}
}

bool UShinobiMenuShotSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiMenuTour"));
}

void UShinobiMenuShotSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if (!World.GetMapName().EndsWith(TEXT("Lvl_MainMenu")))
	{
		return;
	}
	TWeakObjectPtr<UWorld> WeakWorld(&World);
	const auto Menu = [WeakWorld]() { return WeakWorld.IsValid() ? MenuShotsFindMenu(*WeakWorld) : nullptr; };
	TArray<TFunction<void()>> Steps;
	// Each view: set it up, then shoot on the next step (after the tab's fade-in and the stage's slide).
	const auto View = [&Steps, Menu](EShinobiMenuTab Tab, const TCHAR* Name, TFunction<void(UShinobiMainMenuWidget&)> Setup)
	{
		Steps.Add([Menu, Tab, Setup]()
		{
			if (UShinobiMainMenuWidget* M = Menu())
			{
				M->SelectTab(Tab);
				if (Setup)
				{
					Setup(*M);
				}
			}
		});
		Steps.Add([Name]() { MenuShotsTake(Name); });
	};
	Steps.Add([] {}); // the level settles, the stage loads
	View(EShinobiMenuTab::Play, TEXT("ShinobiMenuTour_1_Play"), nullptr);
	View(EShinobiMenuTab::Play, TEXT("ShinobiMenuTour_2_Arena2v2"), [](UShinobiMainMenuWidget& M) { M.TourSelectHost(TEXT("Arena: Camden"), 1); });
	View(EShinobiMenuTab::Play, TEXT("ShinobiMenuTour_3_Find"), [](UShinobiMainMenuWidget& M) { M.TourShowFind(); });
	View(EShinobiMenuTab::Shinobi, TEXT("ShinobiMenuTour_4_Shinobi"), nullptr);
	View(EShinobiMenuTab::Jutsu, TEXT("ShinobiMenuTour_5_Jutsu"), nullptr);
	View(EShinobiMenuTab::Settings, TEXT("ShinobiMenuTour_6_Settings"), nullptr);
	RunSteps(MoveTemp(Steps), 1.5f);
}

void UShinobiMenuShotSubsystem::RunSteps(TArray<TFunction<void()>> Steps, float Interval)
{
	Pending = MoveTemp(Steps);
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (Pending.Num() == 0)
		{
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
			return;
		}
		TFunction<void()> Step = MoveTemp(Pending[0]);
		Pending.RemoveAt(0);
		Step();
	}), Interval, true, 3.f);
}
