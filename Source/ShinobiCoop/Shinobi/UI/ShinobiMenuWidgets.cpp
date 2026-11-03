#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiDropdown.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"

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

void UShinobiIndexSlider::Arm()
{
	OnValueChanged.AddUniqueDynamic(this, &UShinobiIndexSlider::HandleValue);
}

void UShinobiIndexSlider::HandleValue(float NewValue)
{
	OnIndexValue.ExecuteIfBound(Index, NewValue);
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

namespace
{
	constexpr float SettingsPanelWidth = 640.f;
	constexpr float SettingsValueWidth = 190.f;
	/** A stepper arrow's hit area: the glyph and its padding. */
	constexpr float SettingsArrowWidth = 32.f;
	const TCHAR* const SettingsGeneralNote = TEXT("Changes apply and save immediately. Lower graphics quality or render scale for more FPS.");

	/** A row's < or >: an arrow glyph in a padded, transparent hit area. */
	UBorder* SettingsMakeArrow(UWidgetTree& Tree, const TCHAR* Glyph)
	{
		UBorder* Hit = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
		Hit->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor::Transparent));
		Hit->SetPadding(FMargin(10.f, 0.f));
		Hit->SetHorizontalAlignment(HAlign_Center);
		Hit->SetVerticalAlignment(VAlign_Center);
		Hit->SetContent(ShinobiMenuStyle::Text(Tree, FText::FromString(Glyph), 20, ShinobiMenuStyle::Accent));
		return Hit;
	}
}

TSharedRef<SWidget> UShinobiSettingsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		UBorder* Panel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(32.f, 22.f, 32.f, 20.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(GetPanelAnchor().X, GetPanelAnchor().Y));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Width->SetWidthOverride(SettingsPanelWidth);
		Panel->SetContent(Width);
		Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Width->AddChild(Column);

		Rows = ShinobiSettings::RowsFor(bMainMenu);
		FString LastSection;
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const ShinobiSettings::ERow Row = Rows[Index];
			if (const FString Section = ShinobiSettings::RowSection(Row); Section != LastSection)
			{
				LastSection = Section;
				UTextBlock* Heading = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Section), 13, ShinobiMenuStyle::Accent, true);
				Column->AddChildToVerticalBox(Heading)->SetPadding(FMargin(4.f, Index == 0 ? 0.f : 16.f, 0.f, 4.f));
			}
			UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
			Button->Index = Index;
			Button->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiSettingsWidget::OnEntry);
			Button->Arm();
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Button->AddChild(Line);
			UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(ShinobiSettings::RowName(Row)), 16, FLinearColor::White));
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
			UTextBlock* Value = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 16, FLinearColor::White);
			FSlateFontInfo Font = Value->GetFont();
			Font.TypefaceFontName = TEXT("Bold");
			Value->SetFont(Font);
			UBorder* Back = nullptr;
			UBorder* Field = nullptr;
			if (ShinobiSettings::UsesDropdown(Row))
			{
				// Many values: a field that opens the list of them ("Value  v"), as wide as a stepper.
				USizeBox* FieldBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				FieldBox->SetWidthOverride(SettingsValueWidth + 2.f * SettingsArrowWidth);
				Field = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				Field->SetBrush(ShinobiMenuStyle::FieldBrush(false));
				Field->SetPadding(FMargin(12.f, 3.f, 10.f, 3.f));
				FieldBox->AddChild(Field);
				UHorizontalBox* FieldLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				Field->SetContent(FieldLine);
				UHorizontalBoxSlot* ValueSlot = FieldLine->AddChildToHorizontalBox(Value);
				ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				ValueSlot->SetVerticalAlignment(VAlign_Center);
				FieldLine->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(TEXT("\u25BE")), 16, ShinobiMenuStyle::Accent))->SetVerticalAlignment(VAlign_Center);
				Line->AddChildToHorizontalBox(FieldBox)->SetVerticalAlignment(VAlign_Center);
			}
			else
			{
				// Two values (On / Off): "< Value >", a click or Left / Right flips it.
				Back = SettingsMakeArrow(*WidgetTree, TEXT("\u2039"));
				Line->AddChildToHorizontalBox(Back)->SetVerticalAlignment(VAlign_Fill);
				USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				ValueBox->SetWidthOverride(SettingsValueWidth);
				Value->SetJustification(ETextJustify::Center);
				ValueBox->AddChild(Value);
				Line->AddChildToHorizontalBox(ValueBox)->SetVerticalAlignment(VAlign_Center);
				Line->AddChildToHorizontalBox(SettingsMakeArrow(*WidgetTree, TEXT("\u203A")))->SetVerticalAlignment(VAlign_Fill);
			}
			ShinobiMenuStyle::StyleButton(*Button, false, FMargin(20.f, 7.f, 10.f, 7.f));
			Cast<UButtonSlot>(Line->Slot)->SetHorizontalAlignment(HAlign_Fill);
			Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 3.f));
			EntryButtons.Add(Button);
			ValueTexts.Add(Value);
			BackArrows.Add(Back);
			Fields.Add(Field);
		}

		// The help line: what the hovered or focused row does.
		UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Rule->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f)));
		Rule->SetPadding(FMargin(0.f, 0.5f));
		Column->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.f, 16.f, 0.f, 10.f));
		USizeBox* DescriptionBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DescriptionBox->SetMinDesiredHeight(40.f);
		DescriptionText = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(SettingsGeneralNote), 14, ShinobiMenuStyle::TextDim);
		DescriptionText->SetAutoWrapText(true);
		DescriptionBox->AddChild(DescriptionText);
		Column->AddChildToVerticalBox(DescriptionBox)->SetPadding(FMargin(4.f, 0.f));
		RefreshValues();
	}
	return UUserWidget::RebuildWidget(); // not UShinobiMenuWidget's: this panel has its own layout
}

void UShinobiSettingsWidget::OnEntry(int32 Index)
{
	if (Rows.IsValidIndex(Index) && ShinobiSettings::UsesDropdown(Rows[Index]))
	{
		PendingStep = 0;
		OpenRowList(Index);
		return;
	}
	const int32 Delta = PendingStep != 0 ? PendingStep : 1; // a click on the < steps back, anywhere else on
	PendingStep = 0;
	StepRow(Index, Delta);
}

void UShinobiSettingsWidget::OpenRowList(int32 Index)
{
	if (!Rows.IsValidIndex(Index) || !EntryButtons.IsValidIndex(Index))
	{
		return;
	}
	TWeakObjectPtr<UShinobiSettingsWidget> Self = this;
	// Under the row's field; focus comes back to the row.
	UShinobiDropdownList::Open(*EntryButtons[Index], ShinobiSettings::RowOptions(Rows[Index]), ShinobiSettings::RowIndex(Rows[Index]),
		[Self, Index](int32 Picked)
		{
			if (UShinobiSettingsWidget* Panel = Self.Get())
			{
				Panel->PickRow(Index, Picked);
			}
		}, Fields.IsValidIndex(Index) ? Fields[Index].Get() : nullptr);
}

void UShinobiSettingsWidget::PickRow(int32 Index, int32 Option)
{
	if (Rows.IsValidIndex(Index))
	{
		ShinobiSettings::SetRowIndex(Rows[Index], GetWorld(), Option);
		RefreshValues();
	}
}

void UShinobiSettingsWidget::StepRow(int32 Index, int32 Delta)
{
	if (Rows.IsValidIndex(Index))
	{
		ShinobiSettings::Step(Rows[Index], GetWorld(), Delta);
		RefreshValues(); // (one row can change another: the character's faces, the quality preset's render scale)
	}
}

FText UShinobiSettingsWidget::GetShownValue(int32 Index) const
{
	return ValueTexts.IsValidIndex(Index) ? ValueTexts[Index]->GetText() : FText::GetEmpty();
}

FText UShinobiSettingsWidget::GetDescriptionText() const
{
	return DescriptionText ? DescriptionText->GetText() : FText::GetEmpty();
}

void UShinobiSettingsWidget::RefreshValues()
{
	for (int32 Index = 0; Index < ValueTexts.Num() && Index < Rows.Num(); ++Index)
	{
		const FText Value = FText::FromString(ShinobiSettings::RowValue(Rows[Index]));
		if (!ValueTexts[Index]->GetText().EqualTo(Value))
		{
			ValueTexts[Index]->SetText(Value);
		}
	}
}

int32 UShinobiSettingsWidget::GetFocusedRow() const
{
	for (int32 Index = 0; Index < EntryButtons.Num(); ++Index)
	{
		if (EntryButtons[Index]->HasAnyUserFocus())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UShinobiSettingsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime); // focus highlight
	// The mouse's row first, else the focused one.
	int32 Row = INDEX_NONE;
	for (int32 Index = 0; Index < EntryButtons.Num() && Row == INDEX_NONE; ++Index)
	{
		Row = EntryButtons[Index]->IsHovered() ? Index : INDEX_NONE;
	}
	Row = Row != INDEX_NONE ? Row : GetFocusedRow();
	if (Row != DescribedRow && DescriptionText)
	{
		for (int32 Index = 0; Index < Fields.Num(); ++Index)
		{
			if (Fields[Index] && (Index == Row || Index == DescribedRow))
			{
				Fields[Index]->SetBrush(ShinobiMenuStyle::FieldBrush(Index == Row)); // the field of the row under the mouse or focus
			}
		}
		DescribedRow = Row;
		DescriptionText->SetText(FText::FromString(Rows.IsValidIndex(Row) ? ShinobiSettings::RowDescription(Rows[Row]) : FString(SettingsGeneralNote)));
	}
}

FNavigationReply UShinobiSettingsWidget::NativeOnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event, const FNavigationReply& DefaultReply)
{
	// Left / Right change the focused row's value instead of moving focus (arrows, D-pad and stick alike).
	const EUINavigation Direction = Event.GetNavigationType();
	if (Direction == EUINavigation::Left || Direction == EUINavigation::Right)
	{
		if (const int32 Row = GetFocusedRow(); Row != INDEX_NONE)
		{
			StepRow(Row, Direction == EUINavigation::Left ? -1 : 1);
			return FNavigationReply::Stop();
		}
	}
	return Super::NativeOnNavigation(Geometry, Event, DefaultReply);
}

FReply UShinobiSettingsWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	PendingStep = 0;
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		for (const UWidget* Arrow : BackArrows)
		{
			PendingStep = Arrow && Arrow->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()) ? -1 : PendingStep;
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event); // (the row's button still takes the click)
}

void UShinobiSettingsWidget::Refresh()
{
	// Not tied to the session state: rows stay usable while matchmaking. (A host's snow pick can change what's shown.)
	RefreshValues();
}
