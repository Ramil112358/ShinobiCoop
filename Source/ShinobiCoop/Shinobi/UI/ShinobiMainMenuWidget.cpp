#include "Shinobi/UI/ShinobiMainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Shinobi/UI/ShinobiRosterPage.h"

namespace
{
	const TCHAR* const MainMenuTabNames[] = { TEXT("PLAY"), TEXT("SHINOBI"), TEXT("JUTSU"), TEXT("SETTINGS") };
	constexpr float MainMenuFadeSeconds = 0.2f;
	constexpr float MainMenuTopBar = 64.f;

	const TCHAR* MainMenuModeName(EShinobiPvpFormat Format)
	{
		return Format == EShinobiPvpFormat::OneVsOne ? TEXT("1v1") : Format == EShinobiPvpFormat::TwoVsTwo ? TEXT("2v2") : TEXT("CO-OP");
	}

	/** The map's image as a brush, or an accent gradient stand-in when it isn't captured yet. */
	FSlateBrush MainMenuMapBrush(const FShinobiMap& Map, const FVector2D& Size)
	{
		if (UTexture2D* Art = ShinobiMenuArt::LoadArt(ShinobiMenuArt::MapImagePath(Map)))
		{
			return ShinobiMenuStyle::TextureBrush(Art, Size);
		}
		FSlateBrush Brush = ShinobiMenuStyle::ColorBrush(ShinobiMenuStyle::Accent * FLinearColor(0.45f, 0.45f, 0.45f, 1.f));
		Brush.ImageSize = Size;
		return Brush;
	}

	UCanvasPanelSlot* MainMenuPlace(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		return Slot;
	}

	/** Walks up through parents and owning user widgets. */
	bool MainMenuIsInside(const UWidget* Widget, const UWidget* Container)
	{
		while (Widget)
		{
			if (Widget == Container)
			{
				return true;
			}
			const UWidget* Parent = Widget->GetParent();
			if (!Parent)
			{
				const UUserWidget* Owner = Widget->GetTypedOuter<UUserWidget>();
				Parent = Owner != Widget ? Owner : nullptr;
			}
			Widget = Parent;
		}
		return false;
	}
}

UShinobiIndexButton* UShinobiMainMenuWidget::MakeButton(const FString& Label, int32 Size, bool bPrimary, int32 Index, FShinobiIndexClicked Handler)
{
	UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
	Button->Index = Index;
	Button->OnIndexClicked = MoveTemp(Handler);
	Button->Arm();
	Button->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), Size, FLinearColor::White));
	ShinobiMenuStyle::StyleButton(*Button, bPrimary);
	return Button;
}

TSharedRef<SWidget> UShinobiMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true); // tab keys arrive here when nothing inside has focus
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetVisibility(ESlateVisibility::Collapsed);
		MainMenuPlace(Root, Backdrop, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f));
		// Darkens the left side, where the lists are.
		Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shade->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.45f)));
		Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
		MainMenuPlace(Root, Shade, FAnchors(0.f, 0.f, 0.31f, 1.f), FMargin(0.f));

		// Top bar: tabs, then who and how we're online.
		UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Bar->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.55f)));
		Bar->SetPadding(FMargin(28.f, 10.f));
		MainMenuPlace(Root, Bar, FAnchors(0.f, 0.f, 1.f, 0.f), FMargin(0.f, 0.f, 0.f, MainMenuTopBar));
		UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Bar->SetContent(Tabs);
		for (int32 Index = 0; Index < int32(EShinobiMenuTab::Num); ++Index)
		{
			UShinobiIndexButton* Button = MakeButton(MainMenuTabNames[Index], 16, false, Index, FShinobiIndexClicked::CreateUObject(this, &UShinobiMainMenuWidget::OnTabButton));
			Tabs->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			TabButtons.Add(Button);
		}
		UTextBlock* Keys = ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("Q / E"), 11, ShinobiMenuStyle::TextDim);
		UHorizontalBoxSlot* KeysSlot = Tabs->AddChildToHorizontalBox(Keys);
		KeysSlot->SetVerticalAlignment(VAlign_Center);
		KeysSlot->SetPadding(FMargin(6.f, 0.f));
		UHorizontalBoxSlot* Gap = Tabs->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
		Gap->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StatusText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 13, ShinobiMenuStyle::TextDim);
		Tabs->AddChildToHorizontalBox(StatusText)->SetVerticalAlignment(VAlign_Center);

		Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Pages"));
		MainMenuPlace(Root, Pages, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f, MainMenuTopBar, 0.f, 40.f));
		FocusTargets.SetNum(int32(EShinobiMenuTab::Num));
		Pages->AddChild(BuildPlayPage());
		UShinobiRosterPage* Roster = CreateWidget<UShinobiRosterPage>(this, UShinobiRosterPage::StaticClass());
		Roster->Stage = GetStage();
		Pages->AddChild(Roster);
		Roster->TakeWidget();
		FocusTargets[int32(EShinobiMenuTab::Shinobi)] = Roster->GetFocusTarget();
		Pages->AddChild(WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("JutsuPage")));
		UShinobiSettingsWidget* Settings = CreateWidget<UShinobiSettingsWidget>(this, UShinobiSettingsWidget::StaticClass());
		Settings->bMainMenu = true;
		Pages->AddChild(Settings);
		Settings->TakeWidget(); // builds its rows now, so it has a first one to focus
		FocusTargets[int32(EShinobiMenuTab::Settings)] = Settings->GetFirstEntry();

		MessageText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 15, FLinearColor(1.f, 0.75f, 0.3f));
		MessageText->SetJustification(ETextJustify::Center);
		MainMenuPlace(Root, MessageText, FAnchors(0.5f, 1.f), FMargin(0.f, -24.f, 0.f, 0.f), FVector2D(0.5f, 1.f))->SetAutoSize(true);
	}
	return UUserWidget::RebuildWidget(); // not UShinobiMenuWidget's: this menu has no centred column
}

UWidget* UShinobiMainMenuWidget::BuildPlayPage()
{
	UCanvasPanel* Page = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayPage"));

	// Left: HOST | FIND GAMES, the list, Quit.
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainMenuPlace(Page, Left, FAnchors(0.f, 0.f, 0.f, 1.f), FMargin(40.f, 24.f, 520.f, 24.f));
	UHorizontalBox* Sub = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Left->AddChildToVerticalBox(Sub)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	HostTab = MakeButton(TEXT("HOST"), 18, true, 0, FShinobiIndexClicked::CreateWeakLambda(this, [this](int32) { ShowFind(false); }));
	FindTab = MakeButton(TEXT("FIND GAMES"), 18, false, 1, FShinobiIndexClicked::CreateWeakLambda(this, [this](int32) { ShowFind(true); }));
	Sub->AddChildToHorizontalBox(HostTab)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	Sub->AddChildToHorizontalBox(FindTab);

	HostList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("HostList"));
	Left->AddChildToVerticalBox(HostList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FindList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FindList"));
	FindList->SetVisibility(ESlateVisibility::Collapsed);
	Left->AddChildToVerticalBox(FindList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	const TArray<FShinobiMap>& Maps = UShinobiSessionSubsystem::GetMaps();
	HostRows = UShinobiSessionSubsystem::GetHostOrder();
	for (int32 Row = 0; Row < HostRows.Num(); ++Row)
	{
		const FShinobiMap& Map = Maps[HostRows[Row]];
		UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
		Button->Index = Row;
		Button->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiMainMenuWidget::OnHostRow);
		Button->Arm();
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Button->AddChild(Line);
		UImage* Thumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Thumb->SetBrush(MainMenuMapBrush(Map, FVector2D(96.f, 54.f)));
		USizeBox* ThumbBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ThumbBox->SetWidthOverride(96.f);
		ThumbBox->SetHeightOverride(54.f);
		ThumbBox->AddChild(Thumb);
		Line->AddChildToHorizontalBox(ThumbBox)->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		UTextBlock* Name = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Map.DisplayName), 17, FLinearColor::White);
		UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Name);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Chip = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Map.bTest ? TEXT("TEST") : Map.bArena ? TEXT("PVP") : TEXT("CO-OP")), 11,
			Map.bArena ? ShinobiMenuStyle::Accent : ShinobiMenuStyle::TextDim);
		Line->AddChildToHorizontalBox(Chip)->SetVerticalAlignment(VAlign_Center);
		ShinobiMenuStyle::StyleButton(*Button, Row == SelectedRow); // the selected map is filled
		Cast<UButtonSlot>(Line->Slot)->SetHorizontalAlignment(HAlign_Fill);
		HostList->AddChild(Button);
		Cast<UScrollBoxSlot>(Button->Slot)->SetPadding(FMargin(0.f, 3.f));
		HostRowButtons.Add(Button);
	}

	QuitButton = MakeButton(TEXT("Quit"), 14, false, 0, FShinobiIndexClicked::CreateWeakLambda(this, [this](int32)
	{
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
	}));
	UVerticalBoxSlot* QuitSlot = Left->AddChildToVerticalBox(QuitButton);
	QuitSlot->SetHorizontalAlignment(HAlign_Left);
	QuitSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));

	// Right: the selected map, or the search controls.
	UBorder* Panel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(28.f, 24.f));
	MainMenuPlace(Page, Panel, FAnchors(1.f, 0.5f), FMargin(-48.f, 0.f, 440.f, 0.f), FVector2D(1.f, 0.5f))->SetAutoSize(true);
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(400.f);
	Panel->SetContent(Width);
	Detail = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Detail"));
	Width->AddChild(Detail);

	FocusTargets[int32(EShinobiMenuTab::Play)] = HostRowButtons.Num() > 0 ? static_cast<UWidget*>(HostRowButtons[0]) : HostTab.Get();
	BuildHostDetail();
	return Page;
}

void UShinobiMainMenuWidget::BuildHostDetail()
{
	Detail->ClearChildren();
	ModeButtons.Reset();
	HostButton = nullptr;
	RefreshButton = nullptr;
	const auto Add = [this](UWidget* Widget, float Top)
	{
		Detail->AddChildToVerticalBox(Widget)->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
	};
	if (bFinding)
	{
		Add(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("FIND GAMES"), 34, FLinearColor::White, true), 0.f);
		UTextBlock* Hint = ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("Games hosted by your Steam friends and on your network. Pick one to join."), 14, ShinobiMenuStyle::TextDim);
		Hint->SetAutoWrapText(true);
		Add(Hint, 8.f);
		RefreshButton = MakeButton(TEXT("REFRESH"), 18, true, 0, FShinobiIndexClicked::CreateWeakLambda(this, [this](int32)
		{
			if (UShinobiSessionSubsystem* Sessions = GetSessions())
			{
				Sessions->FindGames();
			}
		}));
		Add(RefreshButton, 22.f);
		return;
	}
	if (!HostRows.IsValidIndex(SelectedRow))
	{
		return;
	}
	const FShinobiMap& Map = UShinobiSessionSubsystem::GetMaps()[HostRows[SelectedRow]];
	const TArray<EShinobiPvpFormat> Modes = UShinobiSessionSubsystem::GetModes(Map);
	SelectedMode = FMath::Clamp(SelectedMode, 0, Modes.Num() - 1);
	Add(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Map.bTest ? TEXT("TEST LEVEL") : Map.bArena ? TEXT("PVP ARENA") : TEXT("CO-OP")), 12, ShinobiMenuStyle::Accent), 0.f);
	UTextBlock* Name = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Map.DisplayName), 36, FLinearColor::White, true);
	Name->SetAutoWrapText(true);
	Add(Name, 2.f);
	UTextBlock* Blurb = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Map.Blurb), 15, FLinearColor(0.9f, 0.9f, 0.92f));
	Blurb->SetAutoWrapText(true);
	Add(Blurb, 8.f);
	Add(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(FString::Printf(TEXT("Up to %d players"),
		UShinobiSessionSubsystem::GetMaxPlayers(Map, Modes[SelectedMode]))), 13, ShinobiMenuStyle::TextDim), 10.f);
	UHorizontalBox* ModeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Add(ModeRow, 16.f);
	for (int32 Index = 0; Index < Modes.Num(); ++Index)
	{
		UShinobiIndexButton* Button = MakeButton(MainMenuModeName(Modes[Index]), 15, Index == SelectedMode, Index,
			FShinobiIndexClicked::CreateUObject(this, &UShinobiMainMenuWidget::OnMode));
		ModeRow->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		ModeButtons.Add(Button);
	}
	HostButton = MakeButton(TEXT("HOST  >"), 22, true, 0, FShinobiIndexClicked::CreateWeakLambda(this, [this](int32)
	{
		UShinobiSessionSubsystem* Sessions = GetSessions();
		if (Sessions && HostRows.IsValidIndex(SelectedRow))
		{
			const TArray<EShinobiPvpFormat> Modes = UShinobiSessionSubsystem::GetModes(UShinobiSessionSubsystem::GetMaps()[HostRows[SelectedRow]]);
			Sessions->HostGame(HostRows[SelectedRow], Modes[FMath::Clamp(SelectedMode, 0, Modes.Num() - 1)]);
		}
	}));
	Add(HostButton, 24.f);
	const UShinobiSessionSubsystem* Sessions = GetSessions();
	const bool bIdle = !Sessions || !Sessions->IsBusy();
	HostButton->SetIsEnabled(bIdle);
}

void UShinobiMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct(); // binds session changes, Refresh()
	if (AShinobiMenuStage* Stage = GetStage(); Stage && GetOwningPlayer())
	{
		GetOwningPlayer()->SetViewTarget(Stage);
	}
	SelectTab(EShinobiMenuTab::Play);
	UpdateBackdrop();
}

void UShinobiMainMenuWidget::SelectTab(EShinobiMenuTab NewTab)
{
	Tab = NewTab;
	if (Pages)
	{
		Pages->SetActiveWidgetIndex(int32(NewTab));
		Pages->SetRenderOpacity(0.f);
		FadeIn = 0.f;
	}
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		ShinobiMenuStyle::StyleButton(*TabButtons[Index], Index == int32(NewTab));
	}
	if (AShinobiMenuStage* Stage = GetStage())
	{
		Stage->SetFocus(NewTab == EShinobiMenuTab::Shinobi || NewTab == EShinobiMenuTab::Settings ? EShinobiMenuStageFocus::Left : EShinobiMenuStageFocus::Center);
	}
	UpdateBackdrop();
	Shade->SetVisibility(NewTab == EShinobiMenuTab::Play ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UWidget* Target = GetFocusTarget(NewTab))
	{
		Target->SetKeyboardFocus();
	}
}

UWidget* UShinobiMainMenuWidget::GetTabPage(EShinobiMenuTab Which) const
{
	return Pages ? Pages->GetWidgetAtIndex(int32(Which)) : nullptr;
}

UWidget* UShinobiMainMenuWidget::GetFocusTarget(EShinobiMenuTab Which) const
{
	return FocusTargets.IsValidIndex(int32(Which)) ? FocusTargets[int32(Which)].Get() : nullptr;
}

void UShinobiMainMenuWidget::SetFocusTarget(EShinobiMenuTab Which, UWidget* Target)
{
	if (FocusTargets.IsValidIndex(int32(Which)))
	{
		FocusTargets[int32(Which)] = Target;
	}
}

AShinobiMenuStage* UShinobiMainMenuWidget::GetStage() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AShinobiMenuStage> It(World);
	return It ? *It : nullptr;
}

void UShinobiMainMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime); // focus highlight
	if (FadeIn < 1.f && Pages)
	{
		FadeIn = FMath::Min(1.f, FadeIn + DeltaTime / MainMenuFadeSeconds);
		Pages->SetRenderOpacity(FadeIn);
	}
	// A gamepad moving down the Host list previews each map it passes: only a focus move selects, so a row picked
	// another way (a click elsewhere, the tour) isn't undone by a row that merely still has focus.
	int32 FocusedRow = INDEX_NONE;
	for (int32 Row = 0; Row < HostRowButtons.Num() && FocusedRow == INDEX_NONE; ++Row)
	{
		FocusedRow = HostRowButtons[Row]->HasAnyUserFocus() ? Row : INDEX_NONE;
	}
	if (FocusedRow != LastFocusedRow)
	{
		LastFocusedRow = FocusedRow;
		if (FocusedRow != INDEX_NONE && Tab == EShinobiMenuTab::Play && !bFinding)
		{
			OnHostRow(FocusedRow);
		}
	}
}

FReply UShinobiMainMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	const int32 Count = int32(EShinobiMenuTab::Num);
	if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder)
	{
		SelectTab(EShinobiMenuTab((int32(Tab) + Count - 1) % Count));
		return FReply::Handled();
	}
	if (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
	{
		SelectTab(EShinobiMenuTab((int32(Tab) + 1) % Count));
		return FReply::Handled();
	}
	if ((Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) && Tab != EShinobiMenuTab::Play)
	{
		SelectTab(EShinobiMenuTab::Play);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UShinobiMainMenuWidget::OnTabButton(int32 Index)
{
	SelectTab(EShinobiMenuTab(Index));
}

void UShinobiMainMenuWidget::OnHostRow(int32 Row)
{
	if (Row == SelectedRow)
	{
		return;
	}
	if (HostRowButtons.IsValidIndex(SelectedRow))
	{
		ShinobiMenuStyle::StyleButton(*HostRowButtons[SelectedRow], false);
	}
	if (HostRowButtons.IsValidIndex(Row))
	{
		ShinobiMenuStyle::StyleButton(*HostRowButtons[Row], true);
	}
	SelectedRow = Row;
	SelectedMode = 0;
	BuildHostDetail();
	UpdateBackdrop();

}

void UShinobiMainMenuWidget::TourSelectHost(const FString& DisplayName, int32 Mode)
{
	ShowFind(false);
	const int32 Row = HostRows.IndexOfByKey(UShinobiSessionSubsystem::FindMapByName(DisplayName));
	if (Row != INDEX_NONE)
	{
		OnHostRow(Row);
		OnMode(Mode);
	}
}

void UShinobiMainMenuWidget::OnMode(int32 Index)
{
	SelectedMode = Index;
	BuildHostDetail();
	if (ModeButtons.IsValidIndex(Index))
	{
		ModeButtons[Index]->SetKeyboardFocus();
	}
}

void UShinobiMainMenuWidget::OnJoinRow(int32 Index)
{
	if (UShinobiSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->JoinGame(Index);
	}
}

void UShinobiMainMenuWidget::ShowFind(bool bFind)
{
	bFinding = bFind;
	HostList->SetVisibility(bFind ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	FindList->SetVisibility(bFind ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ShinobiMenuStyle::StyleButton(*HostTab, !bFind);
	ShinobiMenuStyle::StyleButton(*FindTab, bFind);
	BuildHostDetail();
	UpdateBackdrop();
	if (bFind)
	{
		if (UShinobiSessionSubsystem* Sessions = GetSessions(); Sessions && !Sessions->IsBusy())
		{
			Sessions->FindGames();
		}
	}
	Refresh();
}

void UShinobiMainMenuWidget::UpdateBackdrop()
{
	if (!Backdrop)
	{
		return;
	}
	UTexture2D* Art = nullptr;
	if (Tab == EShinobiMenuTab::Play && !bFinding && HostRows.IsValidIndex(SelectedRow))
	{
		Art = ShinobiMenuArt::LoadArt(ShinobiMenuArt::MapImagePath(UShinobiSessionSubsystem::GetMaps()[HostRows[SelectedRow]]));
	}
	Backdrop->SetVisibility(Art ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Art)
	{
		Backdrop->SetBrushFromTexture(Art);
		Backdrop->SetColorAndOpacity(FLinearColor(0.62f, 0.62f, 0.62f, 1.f));
	}
}

void UShinobiMainMenuWidget::Refresh()
{
	Super::Refresh(); // message line
	const UShinobiSessionSubsystem* Sessions = GetSessions();
	const bool bIdle = !Sessions || !Sessions->IsBusy();
	if (StatusText)
	{
		const APlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->PlayerState.Get() : nullptr;
		const FString Online = Sessions && Sessions->IsLanMode() ? TEXT("Steam not detected - LAN mode") : TEXT("Online via Steam");
		StatusText->SetText(FText::FromString(State ? State->GetPlayerName() + TEXT("  ·  ") + Online : Online));
	}
	if (HostButton)
	{
		HostButton->SetIsEnabled(bIdle);
	}
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(bIdle);
	}
	if (!FindList || !Sessions)
	{
		return;
	}
	FindList->ClearChildren();
	const TArray<FShinobiFoundGame>& Games = Sessions->GetFoundGames();
	if (Games.Num() == 0)
	{
		UTextBlock* Empty = ShinobiMenuStyle::Text(*WidgetTree, bIdle ? INVTEXT("No games found. Host one, or refresh.") : INVTEXT("Searching..."), 15, ShinobiMenuStyle::TextDim);
		FindList->AddChild(Empty);
		return;
	}
	const TArray<FShinobiMap>& Maps = UShinobiSessionSubsystem::GetMaps();
	for (int32 Index = 0; Index < Games.Num(); ++Index)
	{
		const FShinobiFoundGame& Game = Games[Index];
		UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
		Button->Index = Index;
		Button->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiMainMenuWidget::OnJoinRow);
		Button->Arm();
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Button->AddChild(Line);
		const int32 MapIndex = UShinobiSessionSubsystem::FindMapByName(Game.MapName);
		USizeBox* ThumbBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ThumbBox->SetWidthOverride(96.f);
		ThumbBox->SetHeightOverride(54.f);
		UImage* Thumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Thumb->SetBrush(Maps.IsValidIndex(MapIndex) ? MainMenuMapBrush(Maps[MapIndex], FVector2D(96.f, 54.f)) : ShinobiMenuStyle::ColorBrush(ShinobiMenuStyle::Panel));
		ThumbBox->AddChild(Thumb);
		Line->AddChildToHorizontalBox(ThumbBox)->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBoxSlot* TextSlot = Line->AddChildToHorizontalBox(Text);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextSlot->SetVerticalAlignment(VAlign_Center);
		Text->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Game.HostName), 16, FLinearColor::White));
		Text->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(FString::Printf(TEXT("%s  %s"),
			Game.MapName.IsEmpty() ? TEXT("?") : *Game.MapName, Game.Format.IsEmpty() ? TEXT("CO-OP") : *Game.Format)), 12, ShinobiMenuStyle::TextDim));
		UTextBlock* Numbers = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(FString::Printf(TEXT("%d/%d   %d ms"), Game.Players, Game.MaxPlayers, Game.PingMs)), 13, FLinearColor::White);
		Line->AddChildToHorizontalBox(Numbers)->SetVerticalAlignment(VAlign_Center);
		ShinobiMenuStyle::StyleButton(*Button, false);
		Cast<UButtonSlot>(Line->Slot)->SetHorizontalAlignment(HAlign_Fill);
		Button->SetIsEnabled(bIdle);
		FindList->AddChild(Button);
		Cast<UScrollBoxSlot>(Button->Slot)->SetPadding(FMargin(0.f, 3.f));
	}
}
