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
#include "Engine/SkeletalMesh.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiMutableLook.h"
#include "Shinobi/Hero/ShinobiMutableNames.h"
#include "Shinobi/Hero/ShinobiMutableTypes.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiCreatorPage.h"
#include "Shinobi/UI/ShinobiMainMenuWidget.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiStudioSet.h"
#include "Engine/DataTable.h"

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
		&& (FParse::Param(Cmd, TEXT("ShinobiMenuTour")) || FParse::Param(Cmd, TEXT("ShinobiMenuPortraits")) || FParse::Param(Cmd, TEXT("ShinobiMapShot"))
			|| FParse::Param(Cmd, TEXT("ShinobiThumbnailShot")));
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
	if (FParse::Param(Cmd, TEXT("ShinobiThumbnailShot")))
	{
		StartThumbnails(World);
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
	View(EShinobiMenuTab::Create, TEXT("ShinobiMenuTour_5_Create"), nullptr);
	Steps.Add([] {}); // (the made character's grooms and face finish loading)
	View(EShinobiMenuTab::Create, TEXT("ShinobiMenuTour_6_CreateFace"), [](UShinobiMainMenuWidget& M)
	{
		if (UShinobiCreatorPage* Creator = Cast<UShinobiCreatorPage>(M.GetTabPage(EShinobiMenuTab::Create)))
		{
			Creator->ShowCategory(UShinobiCreatorPage::ECategory::Face);
		}
	});
	View(EShinobiMenuTab::Jutsu, TEXT("ShinobiMenuTour_7_Jutsu"), nullptr);
	View(EShinobiMenuTab::Settings, TEXT("ShinobiMenuTour_8_Settings"), nullptr);
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

void UShinobiMenuShotSubsystem::StartThumbnails(UWorld& World)
{
	TActorIterator<AShinobiMenuStage> It(&World);
	TWeakObjectPtr<AShinobiMenuStage> Stage = It ? *It : nullptr;
	TArray<TFunction<void()>> Steps;
	Steps.Add([Stage]()
	{
		// No menu; the stage in the studio, in its Studio light.
		TArray<UUserWidget*> Widgets;
		if (Stage.IsValid())
		{
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Stage->GetWorld(), Widgets, UUserWidget::StaticClass(), true);
			Stage->SetStudio(true);
			if (AShinobiStudioSet* Studio = Stage->GetStudio())
			{
				Studio->ApplyPreset(EShinobiStudioLight::Studio);
			}
		}
		for (UUserWidget* Widget : Widgets)
		{
			Widget->RemoveFromParent();
		}
	});
	for (int32 Slot = 0; Slot < int32(EShinobiWardrobeSlot::Cloak); ++Slot)
	{
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(ShinobiMutable::SlotNames[Slot]), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const TArray<FName> Names = Table ? Table->GetRowNames() : TArray<FName>();
		for (int32 Row = 0; Row < Names.Num(); ++Row)
		{
			const FShinobiWardrobeRow* Piece = UShinobiMutableLook::FindRow(EShinobiWardrobeSlot(Slot), uint8(Row));
			if (!Piece)
			{
				continue;
			}
			// The piece alone on its sex's base (men first), average build, a MetaHuman face without hair.
			FShinobiAppearance Look;
			Look.Sex = Piece->MaleMesh.IsNull() ? EShinobiSex::Female : EShinobiSex::Male;
			Look.Head = FShinobiAppearance::None;
			Look.Hair = Look.Beard = Look.Mustache = FShinobiAppearance::None;
			for (int32 Other = 0; Other < ShinobiWardrobeSlotCount; ++Other)
			{
				Look.Slot(EShinobiWardrobeSlot(Other)) = Other == Slot ? uint8(Row) : FShinobiAppearance::None;
			}
			Steps.Add([Stage, Look]() { if (Stage.IsValid()) { Stage->ShowAppearance(Look, 0); } });
			Steps.Add(ShootThumbnail(Stage, Slot, uint8(Row), FString::Printf(TEXT("ShinobiThumbnail_%s_%s"), ShinobiMutable::SlotNames[Slot], *Names[Row].ToString())));
		}
	}
	RunSteps(MoveTemp(Steps), 0.75f);
}

TFunction<void()> UShinobiMenuShotSubsystem::ShootThumbnail(TWeakObjectPtr<AShinobiMenuStage> Stage, int32 WardrobeSlot, uint8 Row, FString Name)
{
	return [this, Stage, WardrobeSlot, Row, Name]()
	{
		UShinobiBodyComponent* Body = Stage.IsValid() ? Stage->GetBody() : nullptr;
		if (Body && Body->IsMutableBuildPending())
		{
			Pending.Insert(ShootThumbnail(Stage, WardrobeSlot, Row, Name), 0); // not built yet: again next step
			return;
		}
		const FShinobiWardrobeRow* Piece = UShinobiMutableLook::FindRow(EShinobiWardrobeSlot(WardrobeSlot), Row);
		const USkeletalMesh* Mesh = Piece ? (Piece->MaleMesh.IsNull() ? Piece->FemaleMesh : Piece->MaleMesh).LoadSynchronous() : nullptr;
		if (!Body || !Mesh)
		{
			return;
		}
		// The piece's bounds on the body, seen from 20 degrees off the front and a little above, with a 10 % margin.
		Stage->FramePortrait(); // (holds the camera where it is put)
		FBox Box = Mesh->GetBounds().GetBox().TransformBy(Body->GetComponentTransform());
		if (WardrobeSlot == int32(EShinobiWardrobeSlot::Shoes))
		{
			// (shoe meshes' bounds reach far past the feet): the feet, from ankle to toe
			Box = FBox(TArray<FVector>{ Body->GetBoneLocation(TEXT("foot_l")), Body->GetBoneLocation(TEXT("foot_r")), Body->GetBoneLocation(TEXT("ball_l")),
				Body->GetBoneLocation(TEXT("ball_r")) }).ExpandBy(FVector(12.f, 12.f, 10.f));
		}
		const float Fov = 30.f;
		const float Radius = FMath::Max(Box.GetExtent().Z, FMath::Max(Box.GetExtent().X, Box.GetExtent().Y)) * 1.1f;
		const float Above = WardrobeSlot == int32(EShinobiWardrobeSlot::Shoes) ? 25.f : 5.f; // (shoes from above: less floor)
		const FVector Toward = Stage->GetActorTransform().TransformVectorNoScale(FRotator(Above, 20.f, 0.f).Vector());
		UCameraComponent* Camera = Stage->GetCamera();
		Camera->SetFieldOfView(Fov);
		Camera->SetWorldLocationAndRotation(Box.GetCenter() + Toward * (Radius / FMath::Tan(FMath::DegreesToRadians(Fov / 2.f))), (-Toward).Rotation());
		Pending.Insert([Name]() { MenuShotsTake(Name, false); }, 0); // next step: the new view rendered
	};
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
