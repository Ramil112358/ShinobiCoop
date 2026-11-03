#include "Shinobi/UI/ShinobiPauseMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Shinobi/UI/ShinobiShortcutsWidget.h"
#include "Shinobi/UI/ShinobiSkillsWidget.h"

namespace
{
	const TCHAR* const PauseTabNames[] = { TEXT("GAME"), TEXT("JUTSU"), TEXT("SETTINGS"), TEXT("CONTROLS") };
	static_assert(UE_ARRAY_COUNT(PauseTabNames) == int32(EShinobiPauseTab::Num), "a name per tab");
	constexpr float PauseTopBar = 64.f;
	constexpr float PauseFooter = 52.f;
	constexpr float PauseFadeSeconds = 0.15f;
	constexpr double PauseLeaveArmSeconds = 4.0;
	const FLinearColor PauseBackdropColor(0.01f, 0.01f, 0.03f, 0.7f);
	const FLinearColor PauseBarColor(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor PauseMessageColor(1.f, 0.75f, 0.3f);

	UCanvasPanelSlot* PausePlace(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		return Slot;
	}

	UShinobiIndexButton* PauseMakeButton(UWidgetTree& Tree, const FString& Label, int32 Size, bool bPrimary, int32 Index, FShinobiIndexClicked Handler, const FMargin& Padding = FMargin(18.f, 8.f))
	{
		UShinobiIndexButton* Button = Tree.ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
		Button->Index = Index;
		Button->OnIndexClicked = MoveTemp(Handler);
		Button->Arm();
		Button->AddChild(ShinobiMenuStyle::Text(Tree, FText::FromString(Label), Size, FLinearColor::White));
		ShinobiMenuStyle::StyleButton(*Button, bPrimary, Padding);
		return Button;
	}

	/** The map this world plays, if it's one Host Game offers. */
	const FShinobiMap* PauseCurrentMap(const UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		const FString Current = UWorld::RemovePIEPrefix(World->GetMapName());
		for (const FShinobiMap& Map : UShinobiSessionSubsystem::GetMaps())
		{
			if (FPackageName::GetShortName(Map.Path) == Current)
			{
				return &Map;
			}
		}
		return nullptr;
	}

	/** A key cap for the footer hints. */
	UBorder* PauseMakeKeyCap(UWidgetTree& Tree, const TCHAR* Key)
	{
		UBorder* Cap = ShinobiMenuStyle::PanelBox(Tree, FMargin(7.f, 1.f));
		Cap->SetContent(ShinobiMenuStyle::Text(Tree, FText::FromString(Key), 12, FLinearColor::White));
		return Cap;
	}
}

TSharedRef<SWidget> UShinobiPauseMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true); // Esc / Start / tab keys arrive here when nothing inside has focus
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Dims the (still running) game and takes the clicks that would otherwise reach the viewport.
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrush(ShinobiMenuStyle::ColorBrush(PauseBackdropColor));
		PausePlace(Root, Backdrop, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f));

		// Top bar: the tabs, then a reminder that the game goes on.
		UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Bar->SetBrush(ShinobiMenuStyle::ColorBrush(PauseBarColor));
		Bar->SetPadding(FMargin(28.f, 10.f));
		PausePlace(Root, Bar, FAnchors(0.f, 0.f, 1.f, 0.f), FMargin(0.f, 0.f, 0.f, PauseTopBar));
		UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Bar->SetContent(Tabs);
		for (int32 Index = 0; Index < int32(EShinobiPauseTab::Num); ++Index)
		{
			UShinobiIndexButton* Button = PauseMakeButton(*WidgetTree, PauseTabNames[Index], 16, false, Index, FShinobiIndexClicked::CreateUObject(this, &UShinobiPauseMenuWidget::OnTabButton));
			Tabs->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			TabButtons.Add(Button);
		}
		UHorizontalBoxSlot* Gap = Tabs->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
		Gap->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UHorizontalBoxSlot* DotSlot = Tabs->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(TEXT("●")), 12, ShinobiMenuStyle::Accent));
		DotSlot->SetVerticalAlignment(VAlign_Center);
		DotSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		Tabs->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("LIVE  ·  the game keeps running"), 13, ShinobiMenuStyle::TextDim))->SetVerticalAlignment(VAlign_Center);

		// The pages, between the bars.
		Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Pages"));
		PausePlace(Root, Pages, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f, PauseTopBar, 0.f, PauseFooter));
		FocusTargets.SetNum(int32(EShinobiPauseTab::Num));
		Pages->AddChild(BuildGamePage());
		FocusTargets[int32(EShinobiPauseTab::Game)] = ResumeButton;

		JutsuPage = CreateWidget<UShinobiSkillsWidget>(this, UShinobiSkillsWidget::StaticClass());
		JutsuPage->bEmbedded = true;
		Pages->AddChild(JutsuPage);
		JutsuPage->TakeWidget(); // builds its cells now, so it has one to focus
		FocusTargets[int32(EShinobiPauseTab::Jutsu)] = JutsuPage->GetFocusTarget();

		SettingsPage = CreateWidget<UShinobiSettingsWidget>(this, UShinobiSettingsWidget::StaticClass());
		Pages->AddChild(SettingsPage);
		SettingsPage->TakeWidget();
		FocusTargets[int32(EShinobiPauseTab::Settings)] = SettingsPage->GetFirstEntry();

		ControlsPage = CreateWidget<UShinobiShortcutsWidget>(this, UShinobiShortcutsWidget::StaticClass());
		Pages->AddChild(ControlsPage);
		ControlsPage->TakeWidget();
		FocusTargets[int32(EShinobiPauseTab::Controls)] = ControlsPage->GetFocusTarget();

		// Footer: session news on the left, the keys on the right.
		UBorder* Footer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Footer->SetBrush(ShinobiMenuStyle::ColorBrush(PauseBarColor));
		Footer->SetPadding(FMargin(28.f, 0.f));
		Footer->SetVerticalAlignment(VAlign_Center);
		PausePlace(Root, Footer, FAnchors(0.f, 1.f, 1.f, 1.f), FMargin(0.f, -PauseFooter, 0.f, PauseFooter));
		UHorizontalBox* FooterLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Footer->SetContent(FooterLine);
		MessageText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 14, PauseMessageColor);
		UHorizontalBoxSlot* MessageSlot = FooterLine->AddChildToHorizontalBox(MessageText);
		MessageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MessageSlot->SetVerticalAlignment(VAlign_Center);
		Hints = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		FooterLine->AddChildToHorizontalBox(Hints)->SetVerticalAlignment(VAlign_Center);
		const AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(GetOwningPlayer());
		UseGamepadHints(PC && PC->IsUsingGamepad());

		Tab = InitialTab;
		Pages->SetActiveWidgetIndex(int32(Tab));
		for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
		{
			ShinobiMenuStyle::StyleButton(*TabButtons[Index], Index == int32(Tab));
		}
	}
	return UUserWidget::RebuildWidget(); // not UShinobiMenuWidget's: this menu has no centred column
}

UWidget* UShinobiPauseMenuWidget::BuildGamePage()
{
	UCanvasPanel* Page = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GamePage"));
	const FShinobiMap* Map = PauseCurrentMap(GetWorld());

	// Left: where we are, then Resume / Invite / Leave.
	USizeBox* LeftWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LeftWidth->SetWidthOverride(440.f);
	PausePlace(Page, LeftWidth, FAnchors(0.f, 0.5f), FMargin(96.f, 0.f, 0.f, 0.f), FVector2D(0.f, 0.5f))->SetAutoSize(true);
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LeftWidth->AddChild(Left);
	const TCHAR* Kind = !Map ? TEXT("IN GAME") : Map->bTest ? TEXT("TEST LEVEL") : Map->bArena ? TEXT("PVP ARENA") : TEXT("CO-OP");
	Left->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Kind), 13, ShinobiMenuStyle::Accent, true));
	UTextBlock* Name = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Map ? Map->DisplayName : TEXT("Paused")), 46, FLinearColor::White, true);
	Name->SetAutoWrapText(true);
	Left->AddChildToVerticalBox(Name)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	UTextBlock* Note = ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("You're still in the world while this menu is open: keep an eye out."), 14, ShinobiMenuStyle::TextDim);
	Note->SetAutoWrapText(true);
	Left->AddChildToVerticalBox(Note)->SetPadding(FMargin(0.f, 6.f, 0.f, 26.f));

	const FShinobiIndexClicked Handler = FShinobiIndexClicked::CreateUObject(this, &UShinobiPauseMenuWidget::OnGameButton);
	ResumeButton = PauseMakeButton(*WidgetTree, TEXT("RESUME"), 20, true, 0, Handler, FMargin(18.f, 12.f));
	Left->AddChildToVerticalBox(ResumeButton)->SetPadding(FMargin(0.f, 4.f));
	InviteButton = PauseMakeButton(*WidgetTree, TEXT("INVITE FRIEND"), 18, false, 1, Handler, FMargin(18.f, 10.f));
	Left->AddChildToVerticalBox(InviteButton)->SetPadding(FMargin(0.f, 4.f));
	InviteNote = ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("Invites go through the Steam overlay: not available in LAN mode."), 12, ShinobiMenuStyle::TextDim);
	InviteNote->SetAutoWrapText(true);
	InviteNote->SetVisibility(ESlateVisibility::Collapsed);
	Left->AddChildToVerticalBox(InviteNote)->SetPadding(FMargin(6.f, 0.f, 0.f, 4.f));
	LeaveButton = PauseMakeButton(*WidgetTree, TEXT("LEAVE GAME"), 18, false, 2, Handler, FMargin(18.f, 10.f));
	Left->AddChildToVerticalBox(LeaveButton)->SetPadding(FMargin(0.f, 4.f));

	// Right: who's in the game.
	UBorder* Squad = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(28.f, 22.f));
	PausePlace(Page, Squad, FAnchors(1.f, 0.5f), FMargin(-96.f, 0.f, 0.f, 0.f), FVector2D(1.f, 0.5f))->SetAutoSize(true);
	USizeBox* SquadWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SquadWidth->SetWidthOverride(360.f);
	Squad->SetContent(SquadWidth);
	UVerticalBox* SquadColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SquadWidth->AddChild(SquadColumn);
	UHorizontalBox* Heading = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SquadColumn->AddChildToVerticalBox(Heading);
	Heading->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("PLAYERS"), 13, ShinobiMenuStyle::Accent, true))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SquadCount = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 13, ShinobiMenuStyle::TextDim);
	Heading->AddChildToHorizontalBox(SquadCount)->SetVerticalAlignment(VAlign_Center);
	UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Rule->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f)));
	Rule->SetPadding(FMargin(0.f, 0.5f));
	SquadColumn->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.f, 8.f, 0.f, 6.f));
	SquadList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SquadList"));
	SquadColumn->AddChildToVerticalBox(SquadList);
	RefreshSquad();
	return Page;
}

void UShinobiPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct(); // binds session changes, Refresh()
	FocusTab();
}

void UShinobiPauseMenuWidget::SelectTab(EShinobiPauseTab NewTab)
{
	if (NewTab == EShinobiPauseTab::Num)
	{
		return;
	}
	if (ControlsPage && NewTab != EShinobiPauseTab::Controls)
	{
		ControlsPage->StopListening(); // (a key press meant for another tab mustn't rebind anything)
	}
	if (Tab != NewTab && Pages)
	{
		Pages->SetActiveWidgetIndex(int32(NewTab));
		Pages->SetRenderOpacity(0.f);
		FadeIn = 0.f;
	}
	Tab = NewTab;
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		ShinobiMenuStyle::StyleButton(*TabButtons[Index], Index == int32(NewTab));
	}
	SetLeaveArmed(false);
	FocusTab();
}

UWidget* UShinobiPauseMenuWidget::GetTabPage(EShinobiPauseTab Which) const
{
	return Pages ? Pages->GetWidgetAtIndex(int32(Which)) : nullptr;
}

UWidget* UShinobiPauseMenuWidget::GetFocusTarget(EShinobiPauseTab Which) const
{
	return FocusTargets.IsValidIndex(int32(Which)) ? FocusTargets[int32(Which)].Get() : nullptr;
}

void UShinobiPauseMenuWidget::FocusTab()
{
	if (UWidget* Target = GetFocusTarget(Tab))
	{
		Target->SetKeyboardFocus();
	}
	else
	{
		SetKeyboardFocus();
	}
}

void UShinobiPauseMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime); // focus highlight
	if (FadeIn < 1.f && Pages)
	{
		FadeIn = FMath::Min(1.f, FadeIn + DeltaTime / PauseFadeSeconds);
		Pages->SetRenderOpacity(FadeIn);
	}
	if (LeaveArmedUntil > 0.0 && FPlatformTime::Seconds() > LeaveArmedUntil)
	{
		SetLeaveArmed(false);
	}
	SquadTimer -= DeltaTime;
	if (SquadTimer <= 0.f)
	{
		SquadTimer = 0.5f;
		RefreshSquad();
	}
	// Input is the menu's alone while it's open: if focus went astray (a click on nothing, the Steam overlay), take it
	// back, or Esc would have nowhere to go.
	if (!HasAnyUserFocus() && !HasFocusedDescendants())
	{
		FocusTab();
	}
}

FReply UShinobiPauseMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey().IsGamepadKey() != bGamepadHints)
	{
		UseGamepadHints(Event.GetKey().IsGamepadKey());
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event); // (pages and buttons still get the key)
}

FReply UShinobiPauseMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	// Only keys nothing on the page used arrive here: a key being bound on CONTROLS, or B dropping a JUTSU pick, never do.
	const FKey Key = Event.GetKey();
	const int32 Count = int32(EShinobiPauseTab::Num);
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)
	{
		Close();
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Right)
	{
		Tab == EShinobiPauseTab::Game ? Close() : SelectTab(EShinobiPauseTab::Game);
		return FReply::Handled();
	}
	const ShinobiKeyBindings::FBinding Skills = ShinobiKeyBindings::Get(ShinobiKeyBindings::EBindable::SkillsScreen);
	if (Key == Skills.Key && Event.IsAltDown() == Skills.bAlt)
	{
		Tab == EShinobiPauseTab::Jutsu ? Close() : SelectTab(EShinobiPauseTab::Jutsu); // K opens and closes JUTSU, as in the game
		return FReply::Handled();
	}
	if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder)
	{
		SelectTab(EShinobiPauseTab((int32(Tab) + Count - 1) % Count));
		return FReply::Handled();
	}
	if (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
	{
		SelectTab(EShinobiPauseTab((int32(Tab) + 1) % Count));
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UShinobiPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return FReply::Handled(); // a click on the backdrop stays in the menu (the viewport would take focus and the keys)
}

FReply UShinobiPauseMenuWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bGamepadHints && !Event.GetCursorDelta().IsNearlyZero())
	{
		UseGamepadHints(false);
	}
	return Super::NativeOnMouseMove(Geometry, Event);
}

void UShinobiPauseMenuWidget::OnTabButton(int32 Index)
{
	SelectTab(EShinobiPauseTab(Index));
}

void UShinobiPauseMenuWidget::OnGameButton(int32 Index)
{
	UShinobiSessionSubsystem* Sessions = GetSessions();
	switch (Index)
	{
	case 0:
		Close();
		break;
	case 1:
		if (!Sessions || !Sessions->ShowInviteUI())
		{
			MessageText->SetText(INVTEXT("The Steam overlay isn't available, so invites can't be sent from here."));
		}
		break;
	default:
		if (!IsLeaveArmed())
		{
			SetLeaveArmed(true); // asks first: leaving can't be undone
		}
		else if (Sessions)
		{
			SetLeaveArmed(false);
			Sessions->LeaveGame();
		}
		break;
	}
}

bool UShinobiPauseMenuWidget::IsLeaveArmed() const
{
	return LeaveArmedUntil > 0.0;
}

void UShinobiPauseMenuWidget::SetLeaveArmed(bool bArmed)
{
	if (!LeaveButton || bArmed == IsLeaveArmed())
	{
		return;
	}
	LeaveArmedUntil = bArmed ? FPlatformTime::Seconds() + PauseLeaveArmSeconds : 0.0;
	if (UTextBlock* Label = Cast<UTextBlock>(LeaveButton->GetChildAt(0)))
	{
		Label->SetText(bArmed ? INVTEXT("CONFIRM LEAVE?") : INVTEXT("LEAVE GAME"));
	}
	ShinobiMenuStyle::StyleButton(*LeaveButton, bArmed, LeaveButton->GetStyle().NormalPadding);
}

void UShinobiPauseMenuWidget::Close()
{
	if (AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(GetOwningPlayer()))
	{
		PC->ClosePauseMenu();
	}
	else
	{
		RemoveFromParent();
	}
}

void UShinobiPauseMenuWidget::Refresh()
{
	const UShinobiSessionSubsystem* Sessions = GetSessions();
	const bool bIdle = !Sessions || !Sessions->IsBusy();
	const bool bLan = !Sessions || Sessions->IsLanMode();
	if (MessageText)
	{
		MessageText->SetText(FText::FromString(Sessions ? Sessions->GetMessage() : FString()));
	}
	if (InviteButton)
	{
		InviteButton->SetIsEnabled(bIdle && !bLan); // the Steam overlay does the inviting
		InviteNote->SetVisibility(bLan ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LeaveButton)
	{
		LeaveButton->SetIsEnabled(bIdle);
	}
}

void UShinobiPauseMenuWidget::RefreshSquad()
{
	if (!SquadList)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World ? World->GetGameState() : nullptr;
	const APlayerState* Mine = GetOwningPlayer() ? GetOwningPlayer()->PlayerState.Get() : nullptr;
	struct FRow { FString Name; FString Ping; bool bMine; };
	TArray<FRow> Rows;
	TArray<FString> Signature;
	if (State)
	{
		for (const APlayerState* Player : State->PlayerArray)
		{
			if (!Player || Player->IsInactive())
			{
				continue;
			}
			const int32 Ping = FMath::RoundToInt(Player->GetPingInMilliseconds());
			Rows.Add({ Player->GetPlayerName(), Ping > 0 ? FString::Printf(TEXT("%d ms"), Ping) : FString(), Player == Mine });
			Signature.Add(Rows.Last().Name + TEXT("|") + Rows.Last().Ping + (Rows.Last().bMine ? TEXT("|me") : TEXT("")));
		}
	}
	if (Signature == ShownSquad && SquadList->GetChildrenCount() > 0)
	{
		return;
	}
	ShownSquad = Signature;
	SquadList->ClearChildren();
	SquadCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Rows.Num(), UShinobiSessionSubsystem::MaxPlayers)));
	if (Rows.Num() == 0)
	{
		SquadList->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("Just you for now."), 15, ShinobiMenuStyle::TextDim));
		return;
	}
	for (const FRow& Row : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		SquadList->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 5.f));
		UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Row.Name), 16, FLinearColor::White));
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
		if (Row.bMine)
		{
			UHorizontalBoxSlot* You = Line->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("YOU"), 11, ShinobiMenuStyle::Accent, true));
			You->SetVerticalAlignment(VAlign_Center);
			You->SetPadding(FMargin(8.f, 0.f));
		}
		Line->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Row.Ping), 13, ShinobiMenuStyle::TextDim))->SetVerticalAlignment(VAlign_Center);
	}
}

void UShinobiPauseMenuWidget::UseGamepadHints(bool bGamepad)
{
	if (!Hints || (bHintsBuilt && bGamepad == bGamepadHints))
	{
		return;
	}
	bGamepadHints = bGamepad;
	bHintsBuilt = true;
	Hints->ClearChildren();
	struct FHint { TArray<const TCHAR*> Keys; const TCHAR* Label; };
	const TArray<FHint> Shown = bGamepad
		? TArray<FHint>{ { { TEXT("LB"), TEXT("RB") }, TEXT("Switch tab") }, { { TEXT("A") }, TEXT("Select") }, { { TEXT("B") }, TEXT("Back") }, { { TEXT("Start") }, TEXT("Resume") } }
		: TArray<FHint>{ { { TEXT("Q"), TEXT("E") }, TEXT("Switch tab") }, { { TEXT("Esc") }, TEXT("Resume") } };
	for (const FHint& Hint : Shown)
	{
		for (const TCHAR* Key : Hint.Keys)
		{
			Hints->AddChildToHorizontalBox(PauseMakeKeyCap(*WidgetTree, Key))->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
		}
		UHorizontalBoxSlot* Label = Hints->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Hint.Label), 13, FLinearColor::White));
		Label->SetVerticalAlignment(VAlign_Center);
		Label->SetPadding(FMargin(4.f, 0.f, 22.f, 0.f));
	}
}
