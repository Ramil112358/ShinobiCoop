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
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"

using ShinobiKeyBindings::EBindable;

namespace
{
	const FLinearColor ShortcutsGoodColor(1.f, 0.75f, 0.3f);
	const FLinearColor ShortcutsBadColor(1.f, 0.35f, 0.3f);
	const FMargin ShortcutsChipPadding(10.f, 5.f);

	UShinobiIndexButton* ShortcutsMakeButton(UWidgetTree& Tree, const FString& Label, int32 Size, int32 Index, FShinobiIndexClicked Handler, const FMargin& Padding)
	{
		UShinobiIndexButton* Button = Tree.ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
		Button->Index = Index;
		Button->OnIndexClicked = MoveTemp(Handler);
		Button->Arm();
		Button->AddChild(ShinobiMenuStyle::Text(Tree, FText::FromString(Label), Size, FLinearColor::White));
		ShinobiMenuStyle::StyleButton(*Button, false, Padding);
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
		UBorder* Panel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(34.f, 24.f, 34.f, 22.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(PanelAnchor.X, PanelAnchor.Y));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		UTextBlock* Hint = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(TEXT("Select a key, then press the new one  ·  Alt + key works  ·  Esc cancels  ·  gamepad buttons are fixed")), 13, ShinobiMenuStyle::TextDim);
		Column->AddChildToVerticalBox(Hint)->SetPadding(FMargin(4.f, 0.f, 4.f, 6.f));

		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Columns)->SetPadding(FMargin(0.f, 6.f));
		Chips.SetNum(static_cast<int32>(EBindable::Num));
		const auto MakeColumn = [this, Columns](const TCHAR* Heading, int32 First, int32 Last, float Gap)
		{
			UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Columns->AddChildToHorizontalBox(Rows)->SetPadding(FMargin(Gap, 0.f, 0.f, 0.f));
			Rows->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Heading), 13, ShinobiMenuStyle::Accent, true))->SetPadding(FMargin(4.f, 8.f, 0.f, 6.f));
			for (int32 Bindable = First; Bindable <= Last; ++Bindable)
			{
				AddRow(Rows, Bindable);
			}
		};
		MakeColumn(TEXT("JUTSU SLOTS"), static_cast<int32>(EBindable::Slot1), static_cast<int32>(EBindable::SkillsScreen), 0.f);
		MakeColumn(TEXT("ACTIONS"), static_cast<int32>(EBindable::Melee), static_cast<int32>(EBindable::Map), 40.f);

		UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Rule->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f)));
		Rule->SetPadding(FMargin(0.f, 0.5f));
		Column->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.f, 12.f, 0.f, 10.f));
		UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Footer);
		MessageText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 14, ShortcutsGoodColor);
		MessageText->SetAutoWrapText(true);
		UHorizontalBoxSlot* MessageSlot = Footer->AddChildToHorizontalBox(MessageText);
		MessageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MessageSlot->SetVerticalAlignment(VAlign_Center);
		MessageSlot->SetPadding(FMargin(4.f, 0.f, 16.f, 0.f));
		Footer->AddChildToHorizontalBox(ShortcutsMakeButton(*WidgetTree, TEXT("RESET TO DEFAULTS"), 14, 0,
			FShinobiIndexClicked::CreateUObject(this, &UShinobiShortcutsWidget::OnFooter), FMargin(16.f, 7.f)))->SetVerticalAlignment(VAlign_Center);
		RefreshChips();
	}
	return Super::RebuildWidget();
}

void UShinobiShortcutsWidget::AddRow(UVerticalBox* Column, int32 Bindable)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(230.f);
	UHorizontalBox* Label = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LabelBox->AddChild(Label);
	UHorizontalBoxSlot* NameSlot = Label->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, ShinobiKeyBindings::Label(static_cast<EBindable>(Bindable)), 15, FLinearColor::White));
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
	if (Bindable <= static_cast<int32>(EBindable::Slot8))
	{
		const ShinobiLoadout::FSlots Slots = ShortcutsCurrentSlots(*this);
		const UClass* Jutsu = Slots.IsValidIndex(Bindable) ? Slots[Bindable].Get() : nullptr;
		const FString Name = Jutsu ? GetDefault<UShinobiJutsuAbility>(const_cast<UClass*>(Jutsu))->DisplayName.ToString() : TEXT("empty");
		UHorizontalBoxSlot* JutsuSlot = Label->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Name), 12, ShinobiMenuStyle::TextDim)); // for reference: the key stays with the slot
		JutsuSlot->SetVerticalAlignment(VAlign_Center);
		JutsuSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}
	Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
	USizeBox* ChipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ChipBox->SetWidthOverride(140.f);
	UShinobiIndexButton* Chip = ShortcutsMakeButton(*WidgetTree, FString(), 14, Bindable, FShinobiIndexClicked::CreateUObject(this, &UShinobiShortcutsWidget::OnKeyChip), ShortcutsChipPadding);
	ChipBox->AddChild(Chip);
	Row->AddChildToHorizontalBox(ChipBox);
	Chips[Bindable] = Chip;
}

UWidget* UShinobiShortcutsWidget::GetFocusTarget() const
{
	return Chips.Num() > 0 && Chips[0] ? static_cast<UWidget*>(Chips[0]) : const_cast<UShinobiShortcutsWidget*>(this);
}

void UShinobiShortcutsWidget::StopListening()
{
	if (Listening != INDEX_NONE)
	{
		Listening = INDEX_NONE;
		SetMessage(FString(), ShortcutsGoodColor);
		RefreshChips();
	}
}

void UShinobiShortcutsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	ShinobiMenuStyle::TickFocus(*WidgetTree); // focus highlight
}

void UShinobiShortcutsWidget::RefreshChips()
{
	for (int32 Bindable = 0; Bindable < Chips.Num(); ++Bindable)
	{
		if (UTextBlock* Text = Chips[Bindable] ? Cast<UTextBlock>(Chips[Bindable]->GetChildAt(0)) : nullptr)
		{
			Text->SetText(Bindable == Listening ? INVTEXT("Press a key...") : ShinobiKeyBindings::KeyLabel(ShinobiKeyBindings::Get(static_cast<EBindable>(Bindable))));
			ShinobiMenuStyle::StyleButton(*Chips[Bindable], Bindable == Listening, ShortcutsChipPadding); // the listening key is lit
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
	ShinobiKeyBindings::ResetToDefaults();
	RefreshChips();
	SetMessage(TEXT("All shortcuts are back to their defaults."), ShortcutsGoodColor);
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
