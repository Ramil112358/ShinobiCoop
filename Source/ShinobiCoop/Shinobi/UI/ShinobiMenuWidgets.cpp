#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Shinobi/UI/ShinobiSettings.h"
#include "Shinobi/UI/ShinobiShortcutsWidget.h"

namespace
{
	const FLinearColor MessageColor(1.f, 0.75f, 0.3f);
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
		UBorder* Panel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(44.f, 30.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(GetPanelAnchor().X, GetPanelAnchor().Y));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		BuildColumn(Panel);
	}
	return Super::RebuildWidget();
}

void UShinobiMenuWidget::BuildColumn(UContentWidget* Parent)
{
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Parent->SetContent(Column);
	if (!GetTitle().IsEmpty())
	{
		AddText(Column, GetTitle(), 34, FLinearColor::White, true);
		UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Rule->SetBrush(ShinobiMenuStyle::ColorBrush(ShinobiMenuStyle::Accent));
		Rule->SetPadding(FMargin(0.f, 1.5f));
		UVerticalBoxSlot* RuleSlot = Column->AddChildToVerticalBox(Rule);
		RuleSlot->SetHorizontalAlignment(HAlign_Center);
		RuleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		Rule->SetContent(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
		Cast<USpacer>(Rule->GetContent())->SetSize(FVector2D(120.f, 0.f));
	}
	const TArray<FString> Entries = GetEntries();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		EntryButtons.Add(AddButton(Column, Entries[Index], Index, FShinobiIndexClicked::CreateUObject(this, &UShinobiMenuWidget::OnEntry)));
	}
	MessageText = AddText(Column, FString(), 14, MessageColor);
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

void UShinobiMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	ShinobiMenuStyle::TickFocus(*WidgetTree);
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
	Button->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), 18, FLinearColor::White));
	ShinobiMenuStyle::StyleButton(*Button, false);
	UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(0.f, 4.f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	return Button;
}

UTextBlock* UShinobiMenuWidget::AddText(UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color, bool bHeading)
{
	UTextBlock* Block = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Text), Size, Color, bHeading);
	Block->SetJustification(ETextJustify::Center);
	Block->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(Block)->SetPadding(FMargin(0.f, 6.f));
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
	for (const ShinobiSettings::ERow Row : ShinobiSettings::RowsFor(bMainMenu))
	{
		Entries.Add(ShinobiSettings::Label(Row));
	}
	Entries.Add(TEXT("Shortcuts"));
	if (!bMainMenu)
	{
		Entries.Add(TEXT("Back"));
	}
	return Entries;
}

void UShinobiSettingsWidget::OnEntry(int32 Index)
{
	const TArray<ShinobiSettings::ERow> Rows = ShinobiSettings::RowsFor(bMainMenu);
	if (Rows.IsValidIndex(Index))
	{
		ShinobiSettings::Cycle(Rows[Index], GetWorld());
		SetEntryLabel(Index, ShinobiSettings::Label(Rows[Index]));
		return;
	}
	if (Index == Rows.Num()) // Shortcuts
	{
		UShinobiShortcutsWidget* Panel = CreateWidget<UShinobiShortcutsWidget>(GetOwningPlayer(), UShinobiShortcutsWidget::StaticClass());
		Panel->Opener = this;
		SetVisibility(ESlateVisibility::Collapsed);
		Panel->AddToViewport(60);
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
