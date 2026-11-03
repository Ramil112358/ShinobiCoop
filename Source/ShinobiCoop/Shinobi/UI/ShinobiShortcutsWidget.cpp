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
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"

using ShinobiKeyBindings::EBindable;

namespace
{
	const FLinearColor ShortcutsGoodColor(1.f, 0.75f, 0.3f);
	const FLinearColor ShortcutsBadColor(1.f, 0.35f, 0.3f);
	const FMargin ShortcutsChipPadding(10.f, 5.f);
	constexpr float ShortcutsLabelWidth = 230.f;
	constexpr float ShortcutsChipWidth = 170.f;

	UShinobiIndexButton* ShortcutsMakeButton(UWidgetTree& Tree, const FString& Label, int32 Size, int32 Index, FShinobiIndexClicked Handler, const FMargin& Padding, bool bPrimary = false)
	{
		UShinobiIndexButton* Button = Tree.ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
		Button->Index = Index;
		Button->OnIndexClicked = MoveTemp(Handler);
		Button->Arm();
		Button->AddChild(ShinobiMenuStyle::Text(Tree, FText::FromString(Label), Size, FLinearColor::White));
		ShinobiMenuStyle::StyleButton(*Button, bPrimary, Padding);
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

	UTextBlock* ShortcutsChipText(const UShinobiIndexButton* Chip)
	{
		return Chip ? Cast<UTextBlock>(Chip->GetChildAt(0)) : nullptr;
	}
}

TSharedRef<SWidget> UShinobiShortcutsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true); // takes the key presses while a row listens
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		UBorder* Panel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(34.f, 22.f, 34.f, 22.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(PanelAnchor.X, PanelAnchor.Y));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);

		// The device tabs.
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(TabRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		for (const TCHAR* Name : { TEXT("KEYBOARD & MOUSE"), TEXT("CONTROLLER") })
		{
			DeviceTabs.Add(ShortcutsMakeButton(*WidgetTree, Name, 14, DeviceTabs.Num(),
				FShinobiIndexClicked::CreateUObject(this, &UShinobiShortcutsWidget::OnDeviceTab), FMargin(16.f, 6.f)));
			TabRow->AddChildToHorizontalBox(DeviceTabs.Last())->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		UHorizontalBoxSlot* TabKeys = TabRow->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("LT / RT"), 11, ShinobiMenuStyle::TextDim));
		TabKeys->SetVerticalAlignment(VAlign_Center);
		TabKeys->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

		HintText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 13, ShinobiMenuStyle::TextDim);
		Column->AddChildToVerticalBox(HintText)->SetPadding(FMargin(4.f, 0.f, 4.f, 6.f));

		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Columns)->SetPadding(FMargin(0.f, 6.f));
		Chips.SetNum(static_cast<int32>(EBindable::Num));
		const auto MakeColumn = [this, Columns](const TCHAR* Heading, int32 First, int32 Last, float Gap)
		{
			UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Columns->AddChildToHorizontalBox(Rows)->SetPadding(FMargin(Gap, 0.f, 0.f, 0.f));
			Rows->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Heading), 13, ShinobiMenuStyle::Accent, true))->SetPadding(FMargin(4.f, 4.f, 0.f, 6.f));
			for (int32 Bindable = First; Bindable <= Last; ++Bindable)
			{
				AddRow(Rows, Bindable);
			}
		};
		MakeColumn(TEXT("JUTSU SLOTS"), static_cast<int32>(EBindable::Slot1), static_cast<int32>(EBindable::SkillsScreen), 0.f);
		MakeColumn(TEXT("ACTIONS"), static_cast<int32>(EBindable::Melee), static_cast<int32>(EBindable::MapZoomOut), 40.f);

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
		ShowDevice(false);
	}
	return Super::RebuildWidget();
}

void UShinobiShortcutsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Opens on the device in use (in game; the main menu starts on the keyboard).
	if (const AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(GetOwningPlayer()))
	{
		ShowDevice(PC->IsUsingGamepad());
	}
}

void UShinobiShortcutsWidget::AddRow(UVerticalBox* Column, int32 Bindable)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(ShortcutsLabelWidth);
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
	ChipBox->SetWidthOverride(ShortcutsChipWidth);
	UShinobiIndexButton* Chip = ShortcutsMakeButton(*WidgetTree, FString(), 14, Bindable, FShinobiIndexClicked::CreateUObject(this, &UShinobiShortcutsWidget::OnChip), ShortcutsChipPadding);
	ChipBox->AddChild(Chip);
	Row->AddChildToHorizontalBox(ChipBox);
	Chips[Bindable] = Chip;
}

UWidget* UShinobiShortcutsWidget::GetFocusTarget() const
{
	return Chips.Num() > 0 && Chips[0] ? static_cast<UWidget*>(Chips[0]) : const_cast<UShinobiShortcutsWidget*>(this);
}

FText UShinobiShortcutsWidget::GetChipText(int32 Bindable) const
{
	const UTextBlock* Text = Chips.IsValidIndex(Bindable) ? ShortcutsChipText(Chips[Bindable]) : nullptr;
	return Text ? Text->GetText() : FText::GetEmpty();
}

void UShinobiShortcutsWidget::ShowDevice(bool bPad)
{
	if (Listening != INDEX_NONE)
	{
		Cancel();
	}
	bShowingPad = bPad;
	for (int32 Index = 0; Index < DeviceTabs.Num(); ++Index)
	{
		ShinobiMenuStyle::StyleButton(*DeviceTabs[Index], (Index == 1) == bPad, FMargin(16.f, 6.f)); // the shown one is filled
	}
	if (HintText)
	{
		HintText->SetText(bPad
			? INVTEXT("Select an action, then press its button  ·  hold RB or RT first for a two-button combo  ·  Menu cancels  ·  Menu always pauses")
			: INVTEXT("Select an action, then press its key  ·  Alt + key works  ·  Esc cancels  ·  Esc always pauses"));
	}
	SetMessage(FString(), ShortcutsGoodColor);
	RefreshChips();
}

void UShinobiShortcutsWidget::StopListening()
{
	if (Listening != INDEX_NONE)
	{
		Cancel();
	}
}

void UShinobiShortcutsWidget::Cancel()
{
	Listening = INDEX_NONE;
	PendingHold = FKey();
	SetMessage(FString(), ShortcutsGoodColor);
	RefreshChips();
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
		UTextBlock* Text = ShortcutsChipText(Chips[Bindable]);
		if (!Text)
		{
			continue;
		}
		const EBindable Which = static_cast<EBindable>(Bindable);
		const bool bLit = Bindable == Listening;
		FText Label;
		if (bLit)
		{
			Label = !bShowingPad ? INVTEXT("Press a key...")
				: PendingHold.IsValid() ? FText::FromString(ShinobiKeyBindings::PadButtonLabel(PendingHold).ToString() + TEXT(" + ...")) : INVTEXT("Press a button...");
		}
		else
		{
			Label = bShowingPad ? ShinobiKeyBindings::PadLabel(ShinobiKeyBindings::GetPad(Which)) : ShinobiKeyBindings::KeyLabel(ShinobiKeyBindings::Get(Which));
		}
		Text->SetText(Label);
		ShinobiMenuStyle::StyleButton(*Chips[Bindable], bLit, ShortcutsChipPadding); // the listening one is lit
	}
}

void UShinobiShortcutsWidget::SetMessage(const FString& Text, const FLinearColor& Color)
{
	if (!MessageText)
	{
		return; // (not built yet)
	}
	MessageText->SetText(FText::FromString(Text));
	MessageText->SetColorAndOpacity(FSlateColor(Color));
}

void UShinobiShortcutsWidget::OnChip(int32 Bindable)
{
	Listening = Bindable;
	PendingHold = FKey();
	SetMessage(bShowingPad ? TEXT("Press a button, or hold RB / RT and press another. Menu cancels.") : FString(), ShortcutsGoodColor);
	RefreshChips();
	SetKeyboardFocus(); // off the chip, so Space / Enter / A can be bound instead of clicking it again
}

void UShinobiShortcutsWidget::OnDeviceTab(int32 Index)
{
	ShowDevice(Index == 1);
}

void UShinobiShortcutsWidget::OnFooter(int32 Index)
{
	Listening = INDEX_NONE;
	PendingHold = FKey();
	if (bShowingPad)
	{
		ShinobiKeyBindings::ResetPad();
	}
	else
	{
		ShinobiKeyBindings::ResetKeyboard();
	}
	RefreshChips();
	SetMessage(bShowingPad ? TEXT("All controller buttons are back to their defaults.") : TEXT("All keyboard and mouse keys are back to their defaults."), ShortcutsGoodColor);
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
		const FString Other = ShinobiKeyBindings::Label(SwappedWith).ToString();
		SetMessage(FString::Printf(TEXT("%s was on %s: %s moved to %s."), *ShinobiKeyBindings::KeyLabel(New).ToString(), *Other, *Other,
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

void UShinobiShortcutsWidget::BindPad(const FKey& Hold, const FKey& Button)
{
	const EBindable Which = static_cast<EBindable>(Listening);
	const ShinobiKeyBindings::FPadBinding Old = ShinobiKeyBindings::GetPad(Which);
	const ShinobiKeyBindings::FPadBinding New{ Hold, Button };
	EBindable SwappedWith = EBindable::Num;
	const ShinobiKeyBindings::ERebind Result = ShinobiKeyBindings::RebindPad(Which, New, &SwappedWith);
	PendingHold = FKey();
	if (Result == ShinobiKeyBindings::ERebind::Blocked)
	{
		SetMessage(FString::Printf(TEXT("%s can't be used. Try A, B, X, Y, the D-pad, LB, LT, L3, R3 or View."),
			*ShinobiKeyBindings::PadButtonLabel(Button).ToString()), ShortcutsBadColor);
		RefreshChips();
		return; // still listening: try another button
	}
	Listening = INDEX_NONE;
	if (Result == ShinobiKeyBindings::ERebind::Swapped)
	{
		const FString Other = ShinobiKeyBindings::Label(SwappedWith).ToString();
		SetMessage(Old.IsBound()
			? FString::Printf(TEXT("%s was on %s: %s moved to %s."), *ShinobiKeyBindings::PadLabel(New).ToString(), *Other, *Other, *ShinobiKeyBindings::PadLabel(Old).ToString())
			: FString::Printf(TEXT("%s was on %s, which now has no button."), *ShinobiKeyBindings::PadLabel(New).ToString(), *Other), ShortcutsGoodColor);
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
	const FKey Key = Event.GetKey();
	if (Listening == INDEX_NONE)
	{
		if (Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_RightTrigger)
		{
			ShowDevice(Key == EKeys::Gamepad_RightTrigger); // LT / RT: the device tabs
			return FReply::Handled();
		}
		return Super::NativeOnKeyDown(Geometry, Event);
	}
	if (Event.IsRepeat())
	{
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)
	{
		Cancel(); // (Esc and Menu always pause, so they're never bound)
		return FReply::Handled();
	}
	if (bShowingPad)
	{
		if (!Key.IsGamepadKey())
		{
			return FReply::Handled(); // keys other than Esc wait for a button
		}
		if (ShinobiKeyBindings::IsPadHold(Key))
		{
			if (!PendingHold.IsValid())
			{
				PendingHold = Key; // the first of a combo: wait for the second
				RefreshChips();
			}
			return FReply::Handled();
		}
		BindPad(PendingHold, Key);
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Right)
	{
		Cancel(); // a pad on a keyboard row: B backs out
		return FReply::Handled();
	}
	if (Key.IsGamepadKey() || Key == EKeys::LeftAlt || Key == EKeys::RightAlt)
	{
		return FReply::Handled(); // Alt alone waits for the key it goes with; buttons are on the CONTROLLER tab
	}
	Bind(Key, Event.IsAltDown());
	return FReply::Handled();
}

FReply UShinobiShortcutsWidget::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Listening != INDEX_NONE && bShowingPad && PendingHold.IsValid() && Event.GetKey() == PendingHold)
	{
		// Let go with nothing pressed alongside: RB / RT never act alone.
		SetMessage(FString::Printf(TEXT("%s only works held with another button: hold it, then press the other."),
			*ShinobiKeyBindings::PadButtonLabel(PendingHold).ToString()), ShortcutsBadColor);
		PendingHold = FKey();
		RefreshChips();
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(Geometry, Event);
}

FReply UShinobiShortcutsWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Listening == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(Geometry, Event);
	}
	if (bShowingPad)
	{
		Cancel(); // a click while a button is awaited backs out
		return FReply::Handled();
	}
	Bind(Event.GetEffectingButton(), Event.IsAltDown());
	return FReply::Handled();
}
