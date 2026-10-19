#include "Shinobi/UI/ShinobiShortcutsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"

using ShinobiKeyBindings::EBindable;

namespace
{
	const FLinearColor ShortcutsPanelColor(0.f, 0.f, 0.f, 0.8f);
	const FLinearColor ShortcutsHeadingColor(0.91f, 0.79f, 0.48f);
	const FLinearColor ShortcutsRowColor(0.88f, 0.88f, 0.9f);
	const FLinearColor ShortcutsGreyColor(0.5f, 0.5f, 0.55f);
	const FLinearColor ShortcutsGoodColor(0.91f, 0.79f, 0.48f);
	const FLinearColor ShortcutsBadColor(1.f, 0.35f, 0.3f);

	UTextBlock* ShortcutsMakeText(UWidgetTree& Tree, const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetText(FText::FromString(Text));
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}

	UShinobiIndexButton* ShortcutsMakeButton(UWidgetTree& Tree, const FString& Label, int32 Index, FShinobiIndexClicked Handler)
	{
		UShinobiIndexButton* Button = Tree.ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
		Button->Index = Index;
		Button->OnIndexClicked = MoveTemp(Handler);
		Button->Arm();
		Button->AddChild(ShortcutsMakeText(Tree, Label, 13, FLinearColor::Black));
		return Button;
	}

	/** The jutsu in each slot, for the grey hint: the live loadout in game, the saved one in the main menu. */
	ShinobiLoadout::FSlots ShortcutsCurrentSlots(const UUserWidget& Widget)
	{
		if (const AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(Widget.GetOwningPlayerPawn()))
		{
			return Player->GetLoadout();
		}
		return ShinobiLoadout::LoadSaved(GetDefault<AShinobiPlayerCharacter>()->GetKnownJutsu());
	}
}

TSharedRef<SWidget> UShinobiShortcutsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true); // takes the key presses while a row listens
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrushColor(ShortcutsPanelColor);
		Panel->SetPadding(FMargin(36.f, 24.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		UTextBlock* Title = ShortcutsMakeText(*WidgetTree, TEXT("SHORTCUTS"), 30, FLinearColor::White);
		Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Hint = ShortcutsMakeText(*WidgetTree, TEXT("Click a key, then press the new one  ·  Alt + key allowed  ·  Esc cancels  ·  keyboard & mouse only"), 11, ShortcutsGreyColor);
		Column->AddChildToVerticalBox(Hint)->SetHorizontalAlignment(HAlign_Center);

		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Columns)->SetPadding(FMargin(0.f, 14.f));
		Chips.SetNum(static_cast<int32>(EBindable::Num));
		const auto MakeColumn = [this, Columns](const TCHAR* Heading, int32 First, int32 Last)
		{
			UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Columns->AddChildToHorizontalBox(Rows)->SetPadding(FMargin(14.f, 0.f));
			Rows->AddChildToVerticalBox(ShortcutsMakeText(*WidgetTree, Heading, 12, ShortcutsHeadingColor))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			for (int32 Bindable = First; Bindable <= Last; ++Bindable)
			{
				AddRow(Rows, Bindable);
			}
		};
		MakeColumn(TEXT("JUTSU SLOTS"), static_cast<int32>(EBindable::Slot1), static_cast<int32>(EBindable::SkillsScreen));
		MakeColumn(TEXT("ACTIONS"), static_cast<int32>(EBindable::Melee), static_cast<int32>(EBindable::Map));

		MessageText = ShortcutsMakeText(*WidgetTree, FString(), 12, ShortcutsGoodColor);
		Column->AddChildToVerticalBox(MessageText)->SetHorizontalAlignment(HAlign_Center);
		UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Footer);
		FooterSlot->SetHorizontalAlignment(HAlign_Center);
		FooterSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		Footer->AddChildToHorizontalBox(ShortcutsMakeButton(*WidgetTree, TEXT("  Reset to defaults  "), 0, FShinobiIndexClicked::CreateUObject(this, &UShinobiShortcutsWidget::OnFooter)))->SetPadding(FMargin(6.f, 0.f));
		Footer->AddChildToHorizontalBox(ShortcutsMakeButton(*WidgetTree, TEXT("  Back  "), 1, FShinobiIndexClicked::CreateUObject(this, &UShinobiShortcutsWidget::OnFooter)))->SetPadding(FMargin(6.f, 0.f));
		RefreshChips();
	}
	return Super::RebuildWidget();
}

void UShinobiShortcutsWidget::AddRow(UVerticalBox* Column, int32 Bindable)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(210.f);
	UHorizontalBox* Label = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LabelBox->AddChild(Label);
	Label->AddChildToHorizontalBox(ShortcutsMakeText(*WidgetTree, ShinobiKeyBindings::Label(static_cast<EBindable>(Bindable)).ToString(), 13, ShortcutsRowColor))->SetVerticalAlignment(VAlign_Center);
	if (Bindable <= static_cast<int32>(EBindable::Slot8))
	{
		const ShinobiLoadout::FSlots Slots = ShortcutsCurrentSlots(*this);
		const UClass* Jutsu = Slots.IsValidIndex(Bindable) ? Slots[Bindable].Get() : nullptr;
		const FString Name = Jutsu ? GetDefault<UShinobiJutsuAbility>(const_cast<UClass*>(Jutsu))->DisplayName.ToString() : TEXT("empty");
		UHorizontalBoxSlot* NameSlot = Label->AddChildToHorizontalBox(ShortcutsMakeText(*WidgetTree, Name, 11, ShortcutsGreyColor)); // for reference: the key stays with the slot
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}
	Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
	USizeBox* ChipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ChipBox->SetWidthOverride(120.f);
	UShinobiIndexButton* Chip = ShortcutsMakeButton(*WidgetTree, FString(), Bindable, FShinobiIndexClicked::CreateUObject(this, &UShinobiShortcutsWidget::OnKeyChip));
	ChipBox->AddChild(Chip);
	Row->AddChildToHorizontalBox(ChipBox);
	Chips[Bindable] = Chip;
}

void UShinobiShortcutsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Chips.Num() > 0 && Chips[0])
	{
		Chips[0]->SetKeyboardFocus(); // gamepad / keyboard navigation starts on slot 1
	}
}

void UShinobiShortcutsWidget::RefreshChips()
{
	for (int32 Bindable = 0; Bindable < Chips.Num(); ++Bindable)
	{
		if (UTextBlock* Text = Chips[Bindable] ? Cast<UTextBlock>(Chips[Bindable]->GetChildAt(0)) : nullptr)
		{
			Text->SetText(Bindable == Listening ? INVTEXT("Press a key...") : ShinobiKeyBindings::KeyLabel(ShinobiKeyBindings::Get(static_cast<EBindable>(Bindable))));
		}
	}
}

void UShinobiShortcutsWidget::SetMessage(const FString& Text, const FLinearColor& Color)
{
	MessageText->SetText(FText::FromString(Text));
	MessageText->SetColorAndOpacity(FSlateColor(Color));
}

void UShinobiShortcutsWidget::OnKeyChip(int32 Bindable)
{
	Listening = Bindable;
	SetMessage(FString(), ShortcutsGoodColor);
	RefreshChips();
	SetKeyboardFocus(); // off the button, so Space / Enter can be bound instead of clicking it again
}

void UShinobiShortcutsWidget::OnFooter(int32 Index)
{
	Listening = INDEX_NONE;
	if (Index == 0)
	{
		ShinobiKeyBindings::ResetToDefaults();
		RefreshChips();
		SetMessage(TEXT("All shortcuts are back to their defaults."), ShortcutsGoodColor);
		return;
	}
	if (UUserWidget* Menu = Opener.Get())
	{
		Menu->SetVisibility(ESlateVisibility::Visible);
	}
	RemoveFromParent();
}

void UShinobiShortcutsWidget::Bind(const FKey& Key, bool bAlt)
{
	const EBindable Which = static_cast<EBindable>(Listening);
	const ShinobiKeyBindings::FBinding Old = ShinobiKeyBindings::Get(Which);
	const ShinobiKeyBindings::FBinding New{ Key, bAlt };
	EBindable SwappedWith = EBindable::Num;
	const ShinobiKeyBindings::ERebind Result = ShinobiKeyBindings::Rebind(Which, New, &SwappedWith);
	if (Result == ShinobiKeyBindings::ERebind::Blocked)
	{
		SetMessage(FString::Printf(TEXT("%s can't be used."), *ShinobiKeyBindings::KeyLabel(New).ToString()), ShortcutsBadColor);
		return; // still listening: try another key
	}
	Listening = INDEX_NONE;
	if (Result == ShinobiKeyBindings::ERebind::Swapped)
	{
		SetMessage(FString::Printf(TEXT("%s was on %s: %s moved to %s."), *ShinobiKeyBindings::KeyLabel(New).ToString(),
			*ShinobiKeyBindings::Label(SwappedWith).ToString(), *ShinobiKeyBindings::Label(SwappedWith).ToString(),
			*ShinobiKeyBindings::KeyLabel(Old).ToString()), ShortcutsGoodColor);
	}
	else
	{
		SetMessage(FString(), ShortcutsGoodColor);
	}
	RefreshChips();
	if (Chips.IsValidIndex(static_cast<int32>(Which)) && Chips[static_cast<int32>(Which)])
	{
		Chips[static_cast<int32>(Which)]->SetKeyboardFocus();
	}
}

FReply UShinobiShortcutsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Listening == INDEX_NONE)
	{
		return Super::NativeOnKeyDown(Geometry, Event);
	}
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape)
	{
		Listening = INDEX_NONE;
		SetMessage(FString(), ShortcutsGoodColor);
		RefreshChips();
		return FReply::Handled();
	}
	if (Key.IsGamepadKey() || Key == EKeys::LeftAlt || Key == EKeys::RightAlt)
	{
		return FReply::Handled(); // Alt alone waits for the key it goes with; pads stay fixed
	}
	Bind(Key, Event.IsAltDown());
	return FReply::Handled();
}

FReply UShinobiShortcutsWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Listening == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(Geometry, Event);
	}
	Bind(Event.GetEffectingButton(), Event.IsAltDown());
	return FReply::Handled();
}
