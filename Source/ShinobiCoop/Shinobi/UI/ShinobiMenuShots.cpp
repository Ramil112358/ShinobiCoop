#include "Shinobi/UI/ShinobiMenuShots.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiMainMenuWidget.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiMenuStage.h"

namespace
{
	UShinobiMainMenuWidget* MenuShotsFindMenu(UWorld& World)
	{
		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(&World, Found, UShinobiMainMenuWidget::StaticClass(), false);
		return Found.Num() > 0 ? Cast<UShinobiMainMenuWidget>(Found[0]) : nullptr;
	}

	void MenuShotsTake(const FString& Name, bool bShowUI = true)
	{
		FScreenshotRequest::RequestScreenshot(Name, bShowUI, false);
	}

	/** Seconds a level gets to stream in and settle before its picture. */
	constexpr float MenuShotsMapSettle = 10.f;
}

bool UShinobiMenuShotSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	const TCHAR* Cmd = FCommandLine::Get();
	return World && World->IsGameWorld()
		&& (FParse::Param(Cmd, TEXT("ShinobiMenuTour")) || FParse::Param(Cmd, TEXT("ShinobiMenuPortraits")) || FParse::Param(Cmd, TEXT("ShinobiMapShot")));
}

void UShinobiMenuShotSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("ShinobiMapShot")))
	{
		StartMapShot(World);
		return;
	}
	if (!World.GetMapName().EndsWith(TEXT("Lvl_MainMenu")))
	{
		return;
	}
	if (FParse::Param(Cmd, TEXT("ShinobiMenuPortraits")))
	{
		StartPortraits(World);
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

void UShinobiMenuShotSubsystem::StartPortraits(UWorld& World)
{
	TActorIterator<AShinobiMenuStage> It(&World);
	TWeakObjectPtr<AShinobiMenuStage> Stage = It ? *It : nullptr;
	TArray<TFunction<void()>> Steps;
	Steps.Add([Stage]()
	{
		// The menu goes; the stage's camera frames each character's head and shoulders.
		TArray<UUserWidget*> Widgets;
		if (Stage.IsValid())
		{
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Stage->GetWorld(), Widgets, UUserWidget::StaticClass(), true);
		}
		for (UUserWidget* Widget : Widgets)
		{
			Widget->RemoveFromParent();
		}
	});
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Index)
	{
		Steps.Add([Stage, Index]()
		{
			if (Stage.IsValid())
			{
				Stage->Show(Index, 0);
				Stage->FramePortrait();
			}
		});
		Steps.Add([Index]() { MenuShotsTake(FString(TEXT("ShinobiMenuPortrait_")) + AShinobiPlayerCharacter::Roster[Index].Name, false); });
	}
	RunSteps(MoveTemp(Steps), 1.5f);
}

void UShinobiMenuShotSubsystem::StartMapShot(UWorld& World)
{
	TWeakObjectPtr<UWorld> WeakWorld(&World);
	FString Id = World.GetMapName();
	Id.RemoveFromStart(World.StreamingLevelsPrefix);
	Id.RemoveFromStart(TEXT("Lvl_"));
	TArray<TFunction<void()>> Steps;
	Steps.Add([WeakWorld]()
	{
		UWorld* W = WeakWorld.Get();
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC)
		{
			return;
		}
		// The view: a camera tagged ShinobiMenuShot placed in the level, else above and behind the first
		// PlayerStart, looking along and down it.
		FTransform View;
		bool bFound = false;
		for (TActorIterator<ACameraActor> It(W); It && !bFound; ++It)
		{
			if (It->ActorHasTag(TEXT("ShinobiMenuShot")))
			{
				View = It->GetActorTransform();
				bFound = true;
			}
		}
		if (!bFound)
		{
			TActorIterator<APlayerStart> Start(W);
			const FTransform From = Start ? Start->GetActorTransform() : FTransform::Identity;
			const FVector Forward = From.GetRotation().GetForwardVector().GetSafeNormal2D();
			// An aerial: 25 m behind and 18 m above, looking down over the start.
			View = FTransform(FRotator(-25.f, Forward.Rotation().Yaw, 0.f), From.GetLocation() - Forward * 2500.f + FVector(0.f, 0.f, 1800.f));
		}
		ACameraActor* Camera = W->SpawnActor<ACameraActor>(View.GetLocation(), View.Rotator());
		Camera->GetCameraComponent()->SetFieldOfView(80.f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorHiddenInGame(true);
		}
		PC->SetViewTarget(Camera);
	});
	Steps.Add([] {}); // the new view's streaming and exposure settle
	Steps.Add([Id]() { MenuShotsTake(TEXT("ShinobiMapShot_") + Id, false); });
	RunSteps(MoveTemp(Steps), MenuShotsMapSettle / 2.f);
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
