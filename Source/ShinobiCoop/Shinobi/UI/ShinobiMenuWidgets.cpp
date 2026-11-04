#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiDropdown.h"
#include "Shinobi/UI/ShinobiGraphics.h"
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
	/** A locked row's opacity (greyed out, still readable). */
	constexpr float SettingsLockedOpacity = 0.4f;
	/** The rows' height (slate units), the same on every category tab so the panel doesn't change size when switching; a
	 * category with more rows scrolls. The panel, its tabs, help line and the menu's bars fit a 1080p screen. */
	constexpr float SettingsRowsHeight = 470.f;
	const FMargin SettingsCategoryPadding(16.f, 6.f);
	/** The category tab last shown: the panel reopens on it (the pause menu builds a new one each time it opens). */
	ShinobiSettings::ECategory SettingsLastCategory = ShinobiSettings::ECategory::Num;
	constexpr float SettingsValueWidth = 190.f;
	/** A stepper arrow's hit area: the glyph and its padding. */
	constexpr float SettingsArrowWidth = 32.f;
	const TCHAR* const SettingsGeneralNote = TEXT("Changes apply and save immediately. Lower graphics quality or upscaling quality for more FPS.");

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
		UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Width->AddChild(Outer);

		ShinobiGraphics::RefreshCaps(); // (what the GPU and NVIDIA's plugins allow decides which rows and options are locked)
		Rows = ShinobiSettings::RowsFor(bMainMenu);
		Categories = ShinobiSettings::CategoriesFor(bMainMenu);

		// The category tabs, along the top (LT / RT switch them, as on CONTROLS).
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Outer->AddChildToVerticalBox(TabRow);
		for (int32 Tab = 0; Tab < Categories.Num(); ++Tab)
		{
			UShinobiIndexButton* TabButton = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
			TabButton->Index = Tab;
			TabButton->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiSettingsWidget::OnCategoryButton);
			TabButton->Arm();
			TabButton->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(ShinobiSettings::CategoryName(Categories[Tab])), 14, FLinearColor::White));
			ShinobiMenuStyle::StyleButton(*TabButton, false, SettingsCategoryPadding);
			TabButton->SetCursor(EMouseCursor::Hand); // a click shows the category
			TabRow->AddChildToHorizontalBox(TabButton)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			CategoryButtons.Add(TabButton);
		}
		TabRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UHorizontalBoxSlot* TabKeys = TabRow->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("LT / RT"), 11, ShinobiMenuStyle::TextDim));
		TabKeys->SetVerticalAlignment(VAlign_Center);
		TabKeys->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
		UBorder* TabRule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		TabRule->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f)));
		TabRule->SetPadding(FMargin(0.f, 0.5f));
		Outer->AddChildToVerticalBox(TabRule)->SetPadding(FMargin(0.f, 12.f, 0.f, 12.f));

		// A page of rows per category, one shown at a time; each scrolls if its rows don't fit.
		USizeBox* RowsHeight = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RowsHeight->SetHeightOverride(SettingsRowsHeight);
		Outer->AddChildToVerticalBox(RowsHeight);
		CategoryPages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("CategoryPages"));
		RowsHeight->AddChild(CategoryPages);
		TArray<UVerticalBox*> TabColumns;
		TArray<TSet<FString>> TabSections;
		TabSections.SetNum(Categories.Num());
		for (int32 Tab = 0; Tab < Categories.Num(); ++Tab)
		{
			UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
			Scroll->SetScrollbarThickness(FVector2D(4.f, 4.f));
			CategoryPages->AddChild(Scroll);
			RowsScrolls.Add(Scroll);
			TabColumns.Add(WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()));
			Scroll->AddChild(TabColumns.Last());
		}
		for (const ShinobiSettings::ERow Row : Rows)
		{
			if (const int32 Tab = Categories.IndexOfByKey(ShinobiSettings::RowCategory(Row)); TabSections.IsValidIndex(Tab))
			{
				TabSections[Tab].Add(ShinobiSettings::RowSection(Row));
			}
		}
		Column = TabColumns.Num() > 0 ? TabColumns[0] : nullptr;

		TArray<FString> LastSections;
		LastSections.SetNum(Categories.Num());
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const ShinobiSettings::ERow Row = Rows[Index];
			const int32 Tab = FMath::Max(0, Categories.IndexOfByKey(ShinobiSettings::RowCategory(Row)));
			RowTabs.Add(Tab);
			UVerticalBox* TabColumn = TabColumns[Tab];
			// Headings split a category with several sections; a single one would only repeat the tab's name.
			if (const FString Section = ShinobiSettings::RowSection(Row); TabSections[Tab].Num() > 1 && Section != LastSections[Tab])
			{
				LastSections[Tab] = Section;
				UTextBlock* Heading = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Section), 13, ShinobiMenuStyle::Accent, true);
				TabColumn->AddChildToVerticalBox(Heading)->SetPadding(FMargin(4.f, TabColumn->GetChildrenCount() == 0 ? 0.f : 16.f, 0.f, 4.f));
			}
			UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
			Button->Index = Index;
			Button->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiSettingsWidget::OnEntry);
			Button->Arm();
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Button->AddChild(Line);
			UTextBlock* Name = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(ShinobiSettings::RowName(Row)), 16, FLinearColor::White);
			UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Name);
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
			TabColumn->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 3.f));
			EntryButtons.Add(Button);
			ValueTexts.Add(Value);
			BackArrows.Add(Back);
			Fields.Add(Field);
		}

		// The help line: what the hovered or focused row does.
		UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Rule->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f)));
		Rule->SetPadding(FMargin(0.f, 0.5f));
		Outer->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.f, 16.f, 0.f, 10.f));
		USizeBox* DescriptionBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DescriptionBox->SetMinDesiredHeight(60.f); // (a description and a "Locked: ..." line)
		DescriptionText = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(SettingsGeneralNote), 14, ShinobiMenuStyle::TextDim);
		DescriptionText->SetAutoWrapText(true);
		DescriptionBox->AddChild(DescriptionText);
		Outer->AddChildToVerticalBox(DescriptionBox)->SetPadding(FMargin(4.f, 0.f));
		const int32 Remembered = Categories.IndexOfByKey(SettingsLastCategory);
		ShowCategoryTab(Remembered != INDEX_NONE ? Remembered : 0, false);
		RefreshValues();
	}
	return UUserWidget::RebuildWidget(); // not UShinobiMenuWidget's: this panel has its own layout
}

void UShinobiSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct(); // (focuses the very first row, which is on another category when the panel reopens on a later one)
	if (EntryButtons.Num() > 0 && EntryButtons[0]->HasAnyUserFocus() && !IsRowShown(0))
	{
		if (UWidget* Target = GetFocusTarget())
		{
			Target->SetKeyboardFocus();
		}
	}
}

void UShinobiSettingsWidget::OnCategoryButton(int32 Index)
{
	ShowCategoryTab(Index, false); // (focus stays on the clicked tab)
}

void UShinobiSettingsWidget::ShowCategoryTab(int32 Index, bool bFocusRow)
{
	if (!Categories.IsValidIndex(Index))
	{
		return;
	}
	ShownTab = Index;
	SettingsLastCategory = Categories[Index];
	if (CategoryPages)
	{
		CategoryPages->SetActiveWidgetIndex(Index);
	}
	for (int32 Tab = 0; Tab < CategoryButtons.Num(); ++Tab)
	{
		ShinobiMenuStyle::StyleButton(*CategoryButtons[Tab], Tab == Index, SettingsCategoryPadding); // the shown one is filled
	}
	bDescriptionStale = true; // (the row the help line explained may be on the category just hidden)
	if (bFocusRow)
	{
		if (UWidget* Target = GetFocusTarget())
		{
			Target->SetKeyboardFocus();
		}
	}
}

void UShinobiSettingsWidget::SelectCategory(ShinobiSettings::ECategory Category, bool bFocusRow)
{
	ShowCategoryTab(Categories.IndexOfByKey(Category), bFocusRow);
}

ShinobiSettings::ECategory UShinobiSettingsWidget::GetCategory() const
{
	return Categories.IsValidIndex(ShownTab) ? Categories[ShownTab] : ShinobiSettings::ECategory::Num;
}

UShinobiIndexButton* UShinobiSettingsWidget::GetCategoryButton(ShinobiSettings::ECategory Category) const
{
	const int32 Tab = Categories.IndexOfByKey(Category);
	return CategoryButtons.IsValidIndex(Tab) ? CategoryButtons[Tab].Get() : nullptr;
}

UWidget* UShinobiSettingsWidget::GetFocusTarget() const
{
	for (int32 Index = 0; Index < EntryButtons.Num(); ++Index)
	{
		if (IsRowShown(Index))
		{
			return EntryButtons[Index].Get();
		}
	}
	return CategoryButtons.IsValidIndex(ShownTab) ? CategoryButtons[ShownTab].Get() : nullptr;
}

bool UShinobiSettingsWidget::IsRowShown(int32 Index) const
{
	return RowTabs.IsValidIndex(Index) && RowTabs[Index] == ShownTab;
}

int32 UShinobiSettingsWidget::GetFocusedCategoryTab() const
{
	for (int32 Tab = 0; Tab < CategoryButtons.Num(); ++Tab)
	{
		if (CategoryButtons[Tab]->HasAnyUserFocus())
		{
			return Tab;
		}
	}
	return INDEX_NONE;
}

FReply UShinobiSettingsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	// LT / RT: the previous / next category (wrapping), focus to its first row. (Q / E and the bumpers stay the menu's tabs.)
	const FKey Key = Event.GetKey();
	if ((Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_RightTrigger) && Categories.Num() > 0)
	{
		if (!Event.IsRepeat())
		{
			const int32 Count = Categories.Num();
			ShowCategoryTab((ShownTab + (Key == EKeys::Gamepad_RightTrigger ? 1 : Count - 1)) % Count, true);
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
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
	if (!ShinobiSettings::RowLock(Rows[Index]).IsEmpty())
	{
		return; // a locked row: the help line says why
	}
	// Under the row's field; focus comes back to the row. Locked options are listed greyed out.
	UShinobiDropdownList::Open(*EntryButtons[Index], ShinobiSettings::RowOptions(Rows[Index]), ShinobiSettings::RowIndex(Rows[Index]),
		[Self, Index](int32 Picked)
		{
			if (UShinobiSettingsWidget* Panel = Self.Get())
			{
				Panel->PickRow(Index, Picked);
			}
		}, Fields.IsValidIndex(Index) ? Fields[Index].Get() : nullptr, ShinobiSettings::OptionLocks(Rows[Index]));
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
		// A locked row (no RTX card, VSync under frame generation, quality with upscaling off...) is greyed out as a whole (name,
		// value, arrows, field) and keeps showing its own value; it can still be focused, and the help line says why.
		if (EntryButtons.IsValidIndex(Index))
		{
			EntryButtons[Index]->SetRenderOpacity(ShinobiSettings::RowLock(Rows[Index]).IsEmpty() ? 1.f : SettingsLockedOpacity);
		}
	}
	bDescriptionStale = true; // (one change can lock or unlock others: the help line is written again)
}

FString UShinobiSettingsWidget::DescribeRow(int32 Row) const
{
	if (!Rows.IsValidIndex(Row))
	{
		return SettingsGeneralNote;
	}
	const FString Note = ShinobiSettings::RowNote(Rows[Row]);
	return Note.IsEmpty() ? ShinobiSettings::RowDescription(Rows[Row]) : ShinobiSettings::RowDescription(Rows[Row]) + TEXT("\n") + Note;
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
	const int32 Focused = GetFocusedRow();
	if (Focused != ScrolledToRow && EntryButtons.IsValidIndex(Focused) && RowTabs.IsValidIndex(Focused) && RowsScrolls.IsValidIndex(RowTabs[Focused]))
	{
		RowsScrolls[RowTabs[Focused]]->ScrollWidgetIntoView(EntryButtons[Focused], true); // keys / gamepad move focus off the visible rows
	}
	ScrolledToRow = Focused;
	Row = Row != INDEX_NONE ? Row : Focused;
	if ((Row != DescribedRow || bDescriptionStale) && DescriptionText)
	{
		for (int32 Index = 0; Index < Fields.Num(); ++Index)
		{
			if (Fields[Index] && (Index == Row || Index == DescribedRow))
			{
				Fields[Index]->SetBrush(ShinobiMenuStyle::FieldBrush(Index == Row)); // the field of the row under the mouse or focus
			}
		}
		DescribedRow = Row;
		bDescriptionStale = false;
		DescriptionText->SetText(FText::FromString(DescribeRow(Row)));
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
		// On the category tabs, the shown category follows focus along them.
		if (const int32 Tab = GetFocusedCategoryTab(); Tab != INDEX_NONE)
		{
			const int32 Next = Tab + (Direction == EUINavigation::Left ? -1 : 1);
			if (CategoryButtons.IsValidIndex(Next))
			{
				ShowCategoryTab(Next, false);
				CategoryButtons[Next]->SetKeyboardFocus();
			}
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
