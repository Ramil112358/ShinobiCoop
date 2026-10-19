#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiSettings.h"

namespace
{
	const FLinearColor PanelColor(0.f, 0.f, 0.f, 0.7f);
	const FLinearColor MessageColor(1.f, 0.75f, 0.3f);
	const FLinearColor DimColor(0.7f, 0.7f, 0.7f);
}

void UShinobiIndexButton::Arm()
{
	OnClicked.AddUniqueDynamic(this, &UShinobiIndexButton::HandleClicked);
}

void UShinobiIndexButton::HandleClicked()
{
	OnIndexClicked.ExecuteIfBound(Index);
}

TSharedRef<SWidget> UShinobiMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(40.f, 30.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);

		Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Panel->SetContent(Column);
		AddText(Column, GetTitle(), 36, FLinearColor::White);
		const TArray<FString> Entries = GetEntries();
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			EntryButtons.Add(AddButton(Column, Entries[Index], Index, FShinobiIndexClicked::CreateUObject(this, &UShinobiMenuWidget::OnEntry)));
		}
		MessageText = AddText(Column, FString(), 14, MessageColor);
	}
	return Super::RebuildWidget();
}

void UShinobiMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UShinobiSessionSubsystem* Sessions = GetSessions())
	{
		ChangedHandle = Sessions->OnChanged.AddUObject(this, &UShinobiMenuWidget::Refresh);
	}
	Refresh();
	if (EntryButtons.Num() > 0)
	{
		EntryButtons[0]->SetKeyboardFocus(); // gamepad / keyboard navigation starts on the first entry
	}
}

void UShinobiMenuWidget::NativeDestruct()
{
	if (UShinobiSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->OnChanged.Remove(ChangedHandle);
	}
	Super::NativeDestruct();
}

void UShinobiMenuWidget::Refresh()
{
	const UShinobiSessionSubsystem* Sessions = GetSessions();
	MessageText->SetText(FText::FromString(Sessions ? Sessions->GetMessage() : FString()));
	for (UShinobiIndexButton* Button : EntryButtons)
	{
		Button->SetIsEnabled(!Sessions || !Sessions->IsBusy());
	}
}

UShinobiSessionSubsystem* UShinobiMenuWidget::GetSessions() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UShinobiSessionSubsystem>() : nullptr;
}

UShinobiIndexButton* UShinobiMenuWidget::AddButton(UVerticalBox* Box, const FString& Label, int32 Index, FShinobiIndexClicked Handler)
{
	UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
	Button->Index = Index;
	Button->OnIndexClicked = MoveTemp(Handler);
	Button->Arm();
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 20;
	Text->SetFont(Font);
	Text->SetText(FText::FromString(Label));
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	Button->AddChild(Text);
	UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(0.f, 6.f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	return Button;
}

UTextBlock* UShinobiMenuWidget::AddText(UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	Block->SetText(FText::FromString(Text));
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(Block)->SetPadding(FMargin(0.f, 8.f));
	return Block;
}

void UShinobiMenuWidget::SetEntryLabel(int32 Index, const FString& Label)
{
	if (EntryButtons.IsValidIndex(Index))
	{
		if (UTextBlock* Text = Cast<UTextBlock>(EntryButtons[Index]->GetChildAt(0)))
		{
			Text->SetText(FText::FromString(Label));
		}
	}
}

void UShinobiMenuWidget::OpenSettings()
{
	UShinobiSettingsWidget* Panel = CreateWidget<UShinobiSettingsWidget>(GetOwningPlayer(), UShinobiSettingsWidget::StaticClass());
	Panel->Opener = this;
	SetVisibility(ESlateVisibility::Collapsed);
	Panel->AddToViewport(50);
}

TSharedRef<SWidget> UShinobiMainMenuWidget::RebuildWidget()
{
	const bool bBuild = WidgetTree && !WidgetTree->RootWidget;
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	if (bBuild)
	{
		const UShinobiSessionSubsystem* Sessions = GetSessions();
		AddText(Column, Sessions && Sessions->IsLanMode() ? TEXT("Steam not detected - LAN mode") : TEXT("Online via Steam"), 12, DimColor);
		GameList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GameList"));
		Column->AddChildToVerticalBox(GameList);
	}
	return Result;
}

void UShinobiMainMenuWidget::OnEntry(int32 Index)
{
	UShinobiSessionSubsystem* Sessions = GetSessions();
	switch (Index)
	{
	case 0:
		bPickingMap = !bPickingMap;
		Refresh();
		break;
	case 1:
		bPickingMap = false;
		if (Sessions)
		{
			Sessions->FindGames();
		}
		break;
	case 2:
		OpenSettings();
		break;
	default:
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	}
}

void UShinobiMainMenuWidget::Refresh()
{
	Super::Refresh();
	const UShinobiSessionSubsystem* Sessions = GetSessions();
	GameList->ClearChildren();
	if (!Sessions)
	{
		return;
	}
	if (bPickingMap)
	{
		// Co-op maps keep their rows; each arena gets one row per format (ponytail: a format toggle when more modes exist).
		HostRows.Reset();
		const TArray<FShinobiMap>& Maps = UShinobiSessionSubsystem::GetMaps();
		for (int32 Index = 0; Index < Maps.Num(); ++Index)
		{
			if (!Maps[Index].bArena)
			{
				HostRows.Add({ Index, EShinobiPvpFormat::None });
			}
		}
		for (int32 Index = 0; Index < Maps.Num(); ++Index)
		{
			if (Maps[Index].bArena)
			{
				HostRows.Add({ Index, EShinobiPvpFormat::OneVsOne });
				HostRows.Add({ Index, EShinobiPvpFormat::TwoVsTwo });
			}
		}
		for (int32 Row = 0; Row < HostRows.Num(); ++Row)
		{
			const FShinobiMap& Map = Maps[HostRows[Row].MapIndex];
			const FString Label = !Map.bArena ? FString::Printf(TEXT("Host on %s"), Map.DisplayName)
				: FString::Printf(TEXT("Host %s %s"), Map.DisplayName, HostRows[Row].Format == EShinobiPvpFormat::TwoVsTwo ? TEXT("2v2") : TEXT("1v1"));
			AddButton(GameList, Label, Row,
				FShinobiIndexClicked::CreateUObject(this, &UShinobiMainMenuWidget::OnMapRow))->SetIsEnabled(!Sessions->IsBusy());
		}
		return;
	}
	const TArray<FShinobiFoundGame>& Games = Sessions->GetFoundGames();
	for (int32 Index = 0; Index < Games.Num(); ++Index)
	{
		const FShinobiFoundGame& Game = Games[Index];
		const FString Label = FString::Printf(TEXT("Join %s - %s%s  (%d/%d, %d ms)"), *Game.HostName,
			Game.MapName.IsEmpty() ? TEXT("?") : *Game.MapName, Game.Format.IsEmpty() ? TEXT("") : *(TEXT(" ") + Game.Format),
			Game.Players, Game.MaxPlayers, Game.PingMs);
		AddButton(GameList, Label, Index, FShinobiIndexClicked::CreateUObject(this, &UShinobiMainMenuWidget::OnJoinRow))->SetIsEnabled(!Sessions->IsBusy());
	}
}

void UShinobiMainMenuWidget::OnMapRow(int32 Index)
{
	if (UShinobiSessionSubsystem* Sessions = GetSessions(); Sessions && HostRows.IsValidIndex(Index))
	{
		Sessions->HostGame(HostRows[Index].MapIndex, HostRows[Index].Format);
	}
}

void UShinobiMainMenuWidget::OnJoinRow(int32 Index)
{
	if (UShinobiSessionSubsystem* Sessions = GetSessions())
	{
		Sessions->JoinGame(Index);
	}
}

void UShinobiPauseMenuWidget::OnEntry(int32 Index)
{
	switch (Index)
	{
	case 0:
		if (AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(GetOwningPlayer()))
		{
			PC->TogglePauseMenu();
		}
		break;
	case 1:
		if (AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(GetOwningPlayer()))
		{
			PC->ToggleSkillsScreen(); // closes this menu first; the gamepad's way to the Skills screen
		}
		break;
	case 2:
		if (UShinobiSessionSubsystem* Sessions = GetSessions())
		{
			Sessions->ShowInviteUI();
		}
		break;
	case 3:
		OpenSettings();
		break;
	default:
		if (UShinobiSessionSubsystem* Sessions = GetSessions())
		{
			Sessions->LeaveGame();
		}
		break;
	}
}

void UShinobiPauseMenuWidget::Refresh()
{
	Super::Refresh();
	const UShinobiSessionSubsystem* Sessions = GetSessions();
	EntryButtons[2]->SetIsEnabled(Sessions && !Sessions->IsLanMode()); // the Steam overlay does the inviting
}

TArray<FString> UShinobiSettingsWidget::GetEntries() const
{
	TArray<FString> Entries;
	for (uint8 Row = 0; Row < uint8(ShinobiSettings::ERow::Num); ++Row)
	{
		Entries.Add(ShinobiSettings::Label(ShinobiSettings::ERow(Row)));
	}
	Entries.Add(TEXT("Back"));
	return Entries;
}

void UShinobiSettingsWidget::OnEntry(int32 Index)
{
	if (Index < int32(ShinobiSettings::ERow::Num))
	{
		const ShinobiSettings::ERow Row = ShinobiSettings::ERow(Index);
		ShinobiSettings::Cycle(Row, GetWorld());
		SetEntryLabel(Index, ShinobiSettings::Label(Row));
		return;
	}
	if (UUserWidget* Menu = Opener.Get())
	{
		Menu->SetVisibility(ESlateVisibility::Visible);
	}
	RemoveFromParent();
}

void UShinobiSettingsWidget::Refresh()
{
	// Not tied to the session state: rows stay usable while matchmaking.
	MessageText->SetText(FText::FromString(TEXT("Changes apply and save immediately. Lower quality or render scale for more FPS.")));
}
