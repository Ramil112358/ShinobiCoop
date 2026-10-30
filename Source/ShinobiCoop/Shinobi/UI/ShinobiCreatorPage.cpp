#include "Shinobi/UI/ShinobiCreatorPage.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CircularThrobber.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Shinobi/Hero/ShinobiFaceShapeAnimInstance.h"
#include "Engine/DataTable.h"
#include "Shinobi/Hero/ShinobiMutableLook.h"
#include "Shinobi/Hero/ShinobiMutableNames.h"
#include "Shinobi/Hero/ShinobiMutableTypes.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Shinobi/UI/ShinobiSettings.h"

namespace
{
	const TCHAR* const CreatorCategoryNames[] = { TEXT("CHARACTER"), TEXT("CLOTHES"), TEXT("BODY & SKIN"), TEXT("FACE") };
	const TCHAR* const CreatorBuildNames[] = { TEXT("Slim"), TEXT("Average"), TEXT("Heavy") };
	constexpr float CreatorPanelWidth = 470.f;
	constexpr float CreatorRowsHeight = 470.f;

	FString CreatorSlotLabel(const UShinobiCharacterSave* Save, int32 SlotIndex)
	{
		return Save && Save->Slots.IsValidIndex(SlotIndex) && !Save->Slots[SlotIndex].Name.IsEmpty()
			? FString::Printf(TEXT("%d %s"), SlotIndex + 1, *Save->Slots[SlotIndex].Name.Left(10)) : FString::Printf(TEXT("%d EMPTY"), SlotIndex + 1);
	}

	int8 CreatorShape(float Value) { return int8(FMath::Clamp(FMath::RoundToInt(Value * 127.f), -127, 127)); }

	/** The wardrobe table's row name of Row in WardrobeSlot (the piece's display name), or None. */
	FName ShinobiCreatorRowName(EShinobiWardrobeSlot WardrobeSlot, uint8 Row)
	{
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(ShinobiMutable::SlotNames[int32(WardrobeSlot)]), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const TArray<FName> Names = Table ? Table->GetRowNames() : TArray<FName>();
		return Names.IsValidIndex(Row) ? Names[Row] : NAME_None;
	}

	/** AShinobiPlayerCharacter::Roster's index of the character named Name, or INDEX_NONE. */
	int32 CreatorRosterIndex(FName Name)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Index)
		{
			if (Name == FName(AShinobiPlayerCharacter::Roster[Index].Name))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

TArray<int32> UShinobiCreatorPage::HeadsOfSex(const UShinobiCreatorCatalog& InCatalog, EShinobiSex Sex)
{
	TArray<int32> Heads;
	for (int32 Index = 0; Index < InCatalog.Heads.Num(); ++Index)
	{
		if (InCatalog.Heads[Index].Sex == Sex)
		{
			Heads.Add(Index);
		}
	}
	return Heads;
}

void UShinobiCreatorPage::ValidateForSex(FShinobiAppearance& Look, const UShinobiCreatorCatalog& InCatalog)
{
	const EShinobiSex Sex = Look.Sex;
	const FShinobiCreatorHead* Head = InCatalog.FindHead(Look.Head);
	if (!Head || Head->Sex != Sex)
	{
		Look.Head = FShinobiAppearance::None;
	}
	for (int32 Slot = 0; Slot < ShinobiWardrobeSlotCount; ++Slot)
	{
		uint8& Row = Look.Slot(EShinobiWardrobeSlot(Slot));
		const FShinobiWardrobeRow* Piece = Row != FShinobiAppearance::None ? UShinobiMutableLook::FindRow(EShinobiWardrobeSlot(Slot), Row) : nullptr;
		if (Piece && (Sex == EShinobiSex::Female ? Piece->FemaleMesh : Piece->MaleMesh).IsNull())
		{
			Row = FShinobiAppearance::None;
		}
	}
	const int32 Faces = InCatalog.CountFaces(Sex);
	Look.Face = Look.Face < Faces ? Look.Face : 0;
	for (int32 Blend = 0; Blend < 2; ++Blend)
	{
		if (Look.BlendFace[Blend] != FShinobiAppearance::None && Look.BlendFace[Blend] >= Faces)
		{
			Look.BlendFace[Blend] = FShinobiAppearance::None;
			Look.BlendWeight[Blend] = 0;
		}
	}
	for (const TPair<uint8*, EShinobiGroomSlot> Groom : { TPair<uint8*, EShinobiGroomSlot>(&Look.Hair, EShinobiGroomSlot::Hair),
		TPair<uint8*, EShinobiGroomSlot>(&Look.Beard, EShinobiGroomSlot::Beard), TPair<uint8*, EShinobiGroomSlot>(&Look.Mustache, EShinobiGroomSlot::Mustache) })
	{
		if (*Groom.Key != FShinobiAppearance::None && *Groom.Key >= InCatalog.CountGrooms(Sex, Groom.Value))
		{
			*Groom.Key = FShinobiAppearance::None;
		}
	}
}

TArray<uint8> UShinobiCreatorPage::RowsForSex(EShinobiWardrobeSlot WardrobeSlot, EShinobiSex Sex)
{
	TArray<uint8> Found;
	for (int32 Row = 0; Row < FShinobiAppearance::None; ++Row)
	{
		const FShinobiWardrobeRow* Piece = UShinobiMutableLook::FindRow(WardrobeSlot, uint8(Row));
		if (!Piece)
		{
			break;
		}
		if (!(Sex == EShinobiSex::Female ? Piece->FemaleMesh : Piece->MaleMesh).IsNull())
		{
			Found.Add(uint8(Row));
		}
	}
	return Found;
}

bool UShinobiCreatorPage::IsTinted(const FShinobiWardrobeRow& Row, EShinobiSex Sex)
{
	return (Sex == EShinobiSex::Female ? Row.FemaleTinted : Row.MaleTinted).ContainsByPredicate([](const TSoftObjectPtr<UMaterialInterface>& Copy) { return !Copy.IsNull(); });
}

UPanelWidget* UShinobiCreatorPage::GetClothesStrip(EShinobiWardrobeSlot WardrobeSlot) const
{
	return ClothesStrips.IsValidIndex(int32(WardrobeSlot)) ? ClothesStrips[int32(WardrobeSlot)].Get() : nullptr;
}

UPanelWidget* UShinobiCreatorPage::GetSwatches(EShinobiWardrobeSlot WardrobeSlot) const
{
	return Swatches.IsValidIndex(int32(WardrobeSlot)) ? Swatches[int32(WardrobeSlot)].Get() : nullptr;
}

UShinobiIndexButton* UShinobiCreatorPage::MakeButton(const FString& Label, int32 Index, TFunction<void(int32)> Handler, int32 Size)
{
	UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
	Button->Index = Index;
	Button->OnIndexClicked = FShinobiIndexClicked::CreateWeakLambda(this, [Handler = MoveTemp(Handler)](int32 Picked) { Handler(Picked); });
	Button->Arm();
	if (!Label.IsEmpty())
	{
		Button->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), Size, FLinearColor::White));
	}
	ShinobiMenuStyle::StyleButton(*Button, false, FMargin(10.f, 6.f));
	return Button;
}

TSharedRef<SWidget> UShinobiCreatorPage::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Save = UShinobiCharacterSave::Load();
		Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		Cloak = uint8(FMath::Max(0, AShinobiPlayerCharacter::FindOutfitIndex(ShinobiSettings::GetSavedOutfit())));
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBorder* Panel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(24.f, 18.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.f, 0.5f));
		PanelSlot->SetPosition(FVector2D(48.f, 0.f));
		PanelSlot->SetAutoSize(true);
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Width->SetWidthOverride(CreatorPanelWidth);
		Panel->SetContent(Width);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Width->AddChild(Column);
		Column->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("CREATE"), 12, ShinobiMenuStyle::Accent));

		UHorizontalBox* CategoryRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(CategoryRow)->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));
		for (int32 Index = 0; Index < int32(ECategory::Num); ++Index)
		{
			CategoryButtons.Add(MakeButton(CreatorCategoryNames[Index], Index, [this](int32 Picked) { ShowCategory(ECategory(Picked)); }, 13));
			CategoryRow->AddChildToHorizontalBox(CategoryButtons.Last())->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		// The rows of the category, scrolling (the face has 30-odd sliders).
		USizeBox* RowsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RowsBox->SetHeightOverride(CreatorRowsHeight);
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		RowsBox->AddChild(Scroll);
		Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(Rows);
		Column->AddChildToVerticalBox(RowsBox);
		// Name, code and the buttons.
		const auto AddBox = [this, Column](const TCHAR* Label, const FText& Hint)
		{
			Column->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), 11, ShinobiMenuStyle::TextDim))->SetPadding(FMargin(0.f, 10.f, 0.f, 2.f));
			UEditableTextBox* Box = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			Box->SetHintText(Hint);
			FEditableTextBoxStyle Style = Box->GetWidgetStyle(); // dark, like the panels
			Style.SetBackgroundColor(FSlateColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.9f)));
			Style.SetForegroundColor(FSlateColor(FLinearColor::White));
			Style.SetFocusedForegroundColor(FSlateColor(FLinearColor::White));
			Box->SetWidgetStyle(Style);
			Column->AddChildToVerticalBox(Box);
			return Box;
		};
		NameBox = AddBox(TEXT("NAME"), INVTEXT("Shinobi"));
		CodeBox = AddBox(TEXT("PRESET CODE (copy to share, paste and LOAD to wear)"), INVTEXT("paste a code"));
		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		Buttons->AddChildToHorizontalBox(MakeButton(TEXT("LOAD CODE"), 0, [this](int32) { LoadCode(); }))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		UShinobiIndexButton* Play = MakeButton(TEXT("SAVE & PLAY"), 0, [this](int32) { SaveAndPlay(); }, 16);
		ShinobiMenuStyle::StyleButton(*Play, true, FMargin(14.f, 6.f));
		Buttons->AddChildToHorizontalBox(Play);
		MessageText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 12, FLinearColor(1.f, 0.75f, 0.3f));
		Column->AddChildToVerticalBox(MessageText)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		if (!Catalog)
		{
			MessageText->SetText(INVTEXT("The creator catalog isn't built (Scripts/setup_creator.py)."));
		}

		// Under the character (CREATE puts it right of centre), while its next look builds.
		Spinner = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass());
		Spinner->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* SpinnerSlot = Root->AddChildToCanvas(Spinner);
		SpinnerSlot->SetAnchors(FAnchors(0.72f, 0.9f));
		SpinnerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SpinnerSlot->SetAutoSize(true);

		for (int32 SlotIndex = 0; SlotIndex < UShinobiCharacterSave::MaxSlots; ++SlotIndex)
		{
			SlotButtons.Add(MakeButton(CreatorSlotLabel(Save, SlotIndex), SlotIndex, [this](int32 Picked) { EditSlot(Picked); }, 12));
		}
		EditSlot(Save->Active != INDEX_NONE ? Save->Active : 0);
	}
	return Super::RebuildWidget();
}

void UShinobiCreatorPage::EditSlot(int32 SlotIndex)
{
	EditedSlot = FMath::Clamp(SlotIndex, 0, UShinobiCharacterSave::MaxSlots - 1);
	const bool bMade = Save && Save->Slots.IsValidIndex(EditedSlot);
	Look = bMade ? Save->Slots[EditedSlot].Look : FShinobiAppearance();
	if (NameBox)
	{
		NameBox->SetText(FText::FromString(bMade ? Save->Slots[EditedSlot].Name : FString()));
	}
	Refresh(true);
}

void UShinobiCreatorPage::ApplyPreset(int32 PresetIndex)
{
	if (!Catalog || !Catalog->Presets.IsValidIndex(PresetIndex))
	{
		return;
	}
	Look = Catalog->Presets[PresetIndex].Look;
	if (NameBox)
	{
		NameBox->SetText(FText::FromName(Catalog->Presets[PresetIndex].Roster));
	}
	Refresh(true);
}

void UShinobiCreatorPage::ShowCategory(ECategory NewCategory)
{
	Category = NewCategory;
	FrameCategory();
	Refresh(false);
}

void UShinobiCreatorPage::FrameCategory()
{
	if (AShinobiMenuStage* Shown = Stage.Get())
	{
		Shown->SetAutoZoom(Category == ECategory::Face ? 1.f : 0.f);
	}
}

void UShinobiCreatorPage::SaveAndPlay()
{
	if (!Save)
	{
		return;
	}
	Save->Slots.SetNum(FMath::Max(Save->Slots.Num(), EditedSlot + 1));
	FString Name = NameBox ? NameBox->GetText().ToString().TrimStartAndEnd().Left(24) : FString();
	Save->Slots[EditedSlot].Name = Name.IsEmpty() ? FString::Printf(TEXT("Shinobi %d"), EditedSlot + 1) : Name;
	Save->Slots[EditedSlot].Look = Look;
	Save->Active = EditedSlot;
	const bool bSaved = Save->Save();
	MessageText->SetText(bSaved ? FText::Format(INVTEXT("{0} saved: played from the next match."), FText::FromString(Save->Slots[EditedSlot].Name))
		: INVTEXT("Could not save."));
	Refresh(false);
}

bool UShinobiCreatorPage::LoadCode()
{
	FShinobiAppearance Coded;
	if (!CodeBox || !FShinobiAppearance::FromCode(CodeBox->GetText().ToString(), Coded))
	{
		MessageText->SetText(INVTEXT("That isn't a preset code."));
		return false;
	}
	Look = Coded;
	if (Catalog)
	{
		ValidateForSex(Look, *Catalog);
	}
	MessageText->SetText(INVTEXT("Code loaded: SAVE & PLAY keeps it."));
	Refresh(true);
	return true;
}

UWidget* UShinobiCreatorPage::GetFocusTarget() const
{
	return CategoryButtons.IsValidIndex(int32(Category)) ? CategoryButtons[int32(Category)].Get() : nullptr;
}

void UShinobiCreatorPage::EditLook(TFunctionRef<void(FShinobiAppearance&)> Change, EShinobiLookEdit Edit)
{
	const EShinobiSex Was = Look.Sex;
	Change(Look);
	if (Look.Sex != Was && Catalog)
	{
		ValidateForSex(Look, *Catalog);
	}
	UShinobiBodyComponent* Body = Stage.IsValid() ? Stage->GetBody() : nullptr;
	switch (Edit)
	{
	case EShinobiLookEdit::Rebuild:
		bRedressPending = true;
		break;
	case EShinobiLookEdit::Debounced:
		DebounceLeft = DebounceSeconds;
		break;
	case EShinobiLookEdit::Skin:
		if (Body) { Body->UpdateSkin(Look); }
		break;
	case EShinobiLookEdit::Shape:
		if (Body) { Body->UpdateBody(Look); Body->UpdateFace(Look); }
		break;
	}
	if (CodeBox)
	{
		CodeBox->SetText(FText::FromString(Look.ToCode()));
	}
}

void UShinobiCreatorPage::TickEdits(float DeltaSeconds)
{
	if (bRedressPending)
	{
		bRedressPending = bRowsPending = false;
		DebounceLeft = 0.f; // (this rebuild carries it)
		Refresh(true);
	}
	if (bRowsPending)
	{
		bRowsPending = false;
		Refresh(false);
	}
	if (DebounceLeft > 0.f)
	{
		DebounceLeft -= DeltaSeconds;
		if (DebounceLeft <= 0.f)
		{
			DebounceLeft = 0.f;
			Redress(); // the rows stay: a slider may still be held
		}
	}
}

void UShinobiCreatorPage::AddNote(const FString& Text)
{
	UTextBlock* Note = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Text), 13, ShinobiMenuStyle::TextDim);
	Note->SetAutoWrapText(true);
	Rows->AddChildToVerticalBox(Note)->SetPadding(FMargin(0.f, 6.f));
}

void UShinobiCreatorPage::AddChoice(const FString& Label, const TArray<FString>& Names, int32 Current, TFunction<void(int32)> Pick)
{
	if (Names.Num() == 0)
	{
		return; // nothing to pick
	}
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 3.f));
	UTextBlock* Caption = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), 14, ShinobiMenuStyle::TextDim);
	Caption->SetMinDesiredWidth(150.f);
	Row->AddChildToHorizontalBox(Caption)->SetVerticalAlignment(VAlign_Center);
	const int32 Shown = Names.IsValidIndex(Current) ? Current : 0;
	for (const int32 Direction : {-1, +1})
	{
		const int32 Next = ((Shown + Direction) % Names.Num() + Names.Num()) % Names.Num();
		UShinobiIndexButton* Arrow = MakeButton(Direction < 0 ? TEXT("<") : TEXT(">"), Next, Pick);
		ShinobiMenuStyle::StyleButton(*Arrow, false, FMargin(10.f, 4.f));
		Row->AddChildToHorizontalBox(Arrow);
		if (Direction < 0)
		{
			UTextBlock* Value = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Names[Shown]), 14, FLinearColor::White);
			Value->SetMinDesiredWidth(150.f);
			Value->SetJustification(ETextJustify::Center);
			UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value);
			ValueSlot->SetVerticalAlignment(VAlign_Center);
			ValueSlot->SetPadding(FMargin(6.f, 0.f));
		}
	}
}

void UShinobiCreatorPage::AddPicker(const FString& Label, uint8 FShinobiAppearance::* Field, int32 Count, bool bNone, TFunction<FString(int32)> Name)
{
	// None sits first when allowed.
	TArray<FString> Names;
	if (bNone)
	{
		Names.Add(TEXT("None"));
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Names.Add(Name ? Name(Index) : FString::Printf(TEXT("%d"), Index + 1));
	}
	const uint8 Value = Look.*Field;
	const int32 Current = (Value == FShinobiAppearance::None || Value >= Count) ? 0 : Value + (bNone ? 1 : 0);
	AddChoice(Label, Names, Current, [this, Field, bNone](int32 Picked)
	{
		EditLook([Field, bNone, Picked](FShinobiAppearance& L) { L.*Field = bNone ? (Picked == 0 ? FShinobiAppearance::None : uint8(Picked - 1)) : uint8(Picked); },
			EShinobiLookEdit::Rebuild);
	});
}

void UShinobiCreatorPage::AddSlider(const FString& Label, float Value, bool bUnit, TFunction<void(float)> Set)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
	UTextBlock* Caption = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), 13, ShinobiMenuStyle::TextDim);
	Caption->SetMinDesiredWidth(150.f);
	Row->AddChildToHorizontalBox(Caption)->SetVerticalAlignment(VAlign_Center);
	UShinobiIndexSlider* Slider = WidgetTree->ConstructWidget<UShinobiIndexSlider>(UShinobiIndexSlider::StaticClass());
	Slider->SetMinValue(bUnit ? 0.f : -1.f);
	Slider->SetMaxValue(1.f);
	Slider->SetStepSize(1.f / 127.f);
	Slider->SetValue(Value);
	Slider->SetSliderHandleColor(ShinobiMenuStyle::Accent);
	Slider->OnIndexValue = FShinobiIndexValue::CreateWeakLambda(this, [Set](int32, float NewValue) { Set(NewValue); });
	Slider->Arm();
	UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SliderSlot->SetVerticalAlignment(VAlign_Center);
}

void UShinobiCreatorPage::Redress()
{
	if (Cloak != AShinobiPlayerCharacter::FindOutfitIndex(ShinobiSettings::GetSavedOutfit()))
	{
		ShinobiSettings::SetSavedOutfit(AShinobiPlayerCharacter::Outfits[Cloak].Name);
	}
	if (AShinobiMenuStage* Shown = Stage.Get())
	{
		Shown->ShowAppearance(Look, Cloak);
	}
}

void UShinobiCreatorPage::Refresh(bool bRedress)
{
	if (!Rows)
	{
		return;
	}
	if (bRedress)
	{
		Redress();
	}
	for (int32 Index = 0; Index < CategoryButtons.Num(); ++Index)
	{
		ShinobiMenuStyle::StyleButton(*CategoryButtons[Index], Index == int32(Category), FMargin(10.f, 6.f));
	}
	CodeBox->SetText(FText::FromString(Look.ToCode()));

	Rows->ClearChildren();
	ClothesStrips.Init(nullptr, ShinobiWardrobeSlotCount);
	Swatches.Init(nullptr, ShinobiWardrobeSlotCount);
	switch (Category)
	{
	case ECategory::Character: BuildCharacterRows(); break;
	case ECategory::Clothes: BuildClothesRows(); break;
	case ECategory::BodySkin: BuildBodySkinRows(); break;
	case ECategory::Face:
	default: BuildFaceRows(); break;
	}
}

void UShinobiCreatorPage::BuildCharacterRows()
{
	// Roster presets: start from a roster character's look.
	AddNote(TEXT("START FROM"));
	UScrollBox* Strip = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Strip->SetOrientation(Orient_Horizontal);
	Rows->AddChildToVerticalBox(Strip);
	for (int32 Index = 0; Catalog && Index < Catalog->Presets.Num(); ++Index)
	{
		const FName Roster = Catalog->Presets[Index].Roster;
		UShinobiIndexButton* Card = MakeButton(FString(), Index, [this](int32 Picked) { ApplyPreset(Picked); });
		ShinobiMenuStyle::StyleButton(*Card, Look == Catalog->Presets[Index].Look, FMargin(4.f));
		UVerticalBox* Face = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		USizeBox* PictureBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PictureBox->SetWidthOverride(64.f);
		PictureBox->SetHeightOverride(80.f);
		UImage* Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		const int32 RosterIndex = CreatorRosterIndex(Roster);
		if (UTexture2D* Art = RosterIndex != INDEX_NONE ? ShinobiMenuArt::LoadArt(ShinobiMenuArt::PortraitPath(RosterIndex)) : nullptr)
		{
			Picture->SetBrushFromTexture(Art);
		}
		else
		{
			Picture->SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.12f));
		}
		PictureBox->AddChild(Picture);
		Face->AddChildToVerticalBox(PictureBox);
		UTextBlock* Name = ShinobiMenuStyle::Text(*WidgetTree, FText::FromName(Roster), 10, FLinearColor::White);
		Name->SetJustification(ETextJustify::Center);
		Face->AddChildToVerticalBox(Name);
		Card->AddChild(Face);
		Strip->AddChild(Card);
	}

	// Save slots.
	AddNote(TEXT("SAVE SLOT"));
	UHorizontalBox* SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Rows->AddChildToVerticalBox(SlotRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	for (int32 SlotIndex = 0; SlotIndex < SlotButtons.Num(); ++SlotIndex)
	{
		UShinobiIndexButton* Button = SlotButtons[SlotIndex];
		ShinobiMenuStyle::StyleButton(*Button, SlotIndex == EditedSlot, FMargin(10.f, 6.f));
		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0)))
		{
			Label->SetText(FText::FromString(CreatorSlotLabel(Save, SlotIndex) + (Save && Save->Active == SlotIndex ? TEXT(" *") : TEXT(""))));
		}
		SlotRow->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}

	AddChoice(TEXT("Sex"), { TEXT("Male"), TEXT("Female") }, int32(Look.Sex), [this](int32 Picked)
	{
		EditLook([Picked](FShinobiAppearance& L) { L.Sex = EShinobiSex(Picked); }, EShinobiLookEdit::Rebuild);
	});
	if (Catalog)
	{
		const TArray<int32> Heads = HeadsOfSex(*Catalog, Look.Sex);
		TArray<FString> Names = { TEXT("MetaHuman face") };
		for (const int32 Head : Heads)
		{
			Names.Add(Catalog->Heads[Head].Id.ToString());
		}
		AddChoice(TEXT("Head"), Names, Heads.IndexOfByKey(int32(Look.Head)) + 1, [this, Heads](int32 Picked)
		{
			EditLook([&Heads, Picked](FShinobiAppearance& L) { L.Head = Picked == 0 ? FShinobiAppearance::None : uint8(Heads[Picked - 1]); }, EShinobiLookEdit::Rebuild);
		});
	}
}

void UShinobiCreatorPage::BuildClothesRows()
{
	const EShinobiSex Sex = Look.Sex;
	const UDataTable* Palette = LoadObject<UDataTable>(nullptr, ShinobiMutable::PalettePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	// Slots a worn piece takes off (a whole outfit's Top, Bottom and Shoes), and by which.
	TMap<EShinobiWardrobeSlot, FName> ReplacedBy;
	for (int32 Index = 0; Index < ShinobiWardrobeSlotCount; ++Index)
	{
		if (const FShinobiWardrobeRow* Worn = UShinobiMutableLook::FindRow(EShinobiWardrobeSlot(Index), Look.Slot(EShinobiWardrobeSlot(Index))))
		{
			for (const EShinobiWardrobeSlot Replaced : Worn->Replaces)
			{
				ReplacedBy.Add(Replaced, ShinobiCreatorRowName(EShinobiWardrobeSlot(Index), Look.Slot(EShinobiWardrobeSlot(Index))));
			}
		}
	}
	for (int32 Index = 0; Index < int32(EShinobiWardrobeSlot::Cloak); ++Index) // (cloaks: the outfit picker below until P5)
	{
		const EShinobiWardrobeSlot WardrobeSlot = EShinobiWardrobeSlot(Index);
		const TArray<uint8> Pieces = RowsForSex(WardrobeSlot, Sex);
		if (Pieces.Num() == 0)
		{
			continue;
		}
		const uint8 Worn = Look.Slot(WardrobeSlot);
		AddNote(FString(ShinobiMutable::SlotNames[Index]).ToUpper() + (Worn != FShinobiAppearance::None ? TEXT("  ") + ShinobiCreatorRowName(WardrobeSlot, Worn).ToString() : FString()));
		if (const FName* By = ReplacedBy.Find(WardrobeSlot))
		{
			AddNote(FString::Printf(TEXT("(worn under %s)"), *By->ToString()));
			continue;
		}
		UScrollBox* Strip = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Strip->SetOrientation(Orient_Horizontal);
		Rows->AddChildToVerticalBox(Strip);
		ClothesStrips[Index] = Strip;
		for (int32 Position = 0; Position <= Pieces.Num(); ++Position)
		{
			const uint8 Row = Position == 0 ? FShinobiAppearance::None : Pieces[Position - 1];
			UShinobiIndexButton* Card = MakeButton(FString(), Row, [this, WardrobeSlot](int32 Picked)
			{
				EditLook([WardrobeSlot, Picked](FShinobiAppearance& L) { L.Slot(WardrobeSlot) = uint8(Picked); }, EShinobiLookEdit::Rebuild);
			});
			ShinobiMenuStyle::StyleButton(*Card, Row == Worn, FMargin(3.f));
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(72.f);
			Box->SetHeightOverride(72.f);
			const FShinobiWardrobeRow* Piece = UShinobiMutableLook::FindRow(WardrobeSlot, Row);
			if (UTexture2D* Thumbnail = Piece ? Piece->Thumbnail.LoadSynchronous() : nullptr)
			{
				UImage* Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Picture->SetBrushFromTexture(Thumbnail);
				Box->AddChild(Picture);
			}
			else
			{
				UTextBlock* Name = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Piece ? ShinobiCreatorRowName(WardrobeSlot, Row).ToString() : TEXT("None")), 10, FLinearColor::White);
				Name->SetAutoWrapText(true);
				Name->SetJustification(ETextJustify::Center);
				Box->AddChild(Name);
			}
			Card->AddChild(Box);
			Strip->AddChild(Card);
		}
		// The worn piece's colours, when it takes them.
		const FShinobiWardrobeRow* WornPiece = UShinobiMutableLook::FindRow(WardrobeSlot, Worn);
		if (Palette && WornPiece && IsTinted(*WornPiece, Sex))
		{
			UHorizontalBox* Colours = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Rows->AddChildToVerticalBox(Colours)->SetPadding(FMargin(0.f, 4.f, 0.f, 6.f));
			Swatches[Index] = Colours;
			const TArray<FName> Names = Palette->GetRowNames();
			for (int32 Colour = 0; Colour < Names.Num(); ++Colour)
			{
				UShinobiIndexButton* Swatch = MakeButton(FString(), Colour, [this, Index](int32 Picked)
				{
					EditLook([Index, Picked](FShinobiAppearance& L) { L.SlotColour[Index] = uint8(Picked); }, EShinobiLookEdit::Skin);
					bRowsPending = true; // (the selection mark moves)
				});
				ShinobiMenuStyle::StyleButton(*Swatch, Look.SlotColour[Index] == Colour, FMargin(2.f));
				USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				Box->SetWidthOverride(20.f);
				Box->SetHeightOverride(20.f);
				UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				// Original (row 0) shows as the dark panel colour: the piece's own colours.
				Fill->SetBrush(ShinobiMenuStyle::ColorBrush(Colour == 0 ? FLinearColor(0.15f, 0.15f, 0.17f) : Palette->FindRow<FShinobiPaletteRow>(Names[Colour], TEXT("Swatch"))->Colour));
				Box->AddChild(Fill);
				Swatch->AddChild(Box);
				Swatch->SetToolTipText(FText::FromName(Names[Colour]));
				Colours->AddChildToHorizontalBox(Swatch)->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
			}
		}
	}
	TArray<FString> Cloaks;
	for (const auto& Outfit : AShinobiPlayerCharacter::Outfits)
	{
		Cloaks.Add(Outfit.Name);
	}
	AddChoice(TEXT("Cloak"), Cloaks, Cloak, [this](int32 Picked)
	{
		Cloak = uint8(Picked);
		EditLook([](FShinobiAppearance&) {}, EShinobiLookEdit::Rebuild);
	});
}

void UShinobiCreatorPage::BuildBodySkinRows()
{
	AddPicker(TEXT("Build"), reinterpret_cast<uint8 FShinobiAppearance::*>(&FShinobiAppearance::Build), 3, false, [](int32 V) { return FString(CreatorBuildNames[V]); });
	const TArray<FName> Sliders = UShinobiBodyComponent::GetBodySliderNames();
	for (int32 Index = 0; Index < Sliders.Num(); ++Index)
	{
		AddSlider(FName::NameToDisplayString(Sliders[Index].ToString(), false), Look.BodyShape.IsValidIndex(Index) ? Look.BodyShape[Index] / 127.f : 0.f, false,
			[this, Index](float V)
			{
				EditLook([Index, V](FShinobiAppearance& L)
				{
					L.BodyShape.SetNumZeroed(FMath::Max(L.BodyShape.Num(), Index + 1));
					L.BodyShape[Index] = CreatorShape(V);
				}, EShinobiLookEdit::Shape);
			});
	}
}

void UShinobiCreatorPage::BuildFaceRows()
{
	if (Look.Head != FShinobiAppearance::None)
	{
		AddNote(TEXT("Roster head: its face and hair are fixed. Pick \"MetaHuman face\" under CHARACTER to shape a face."));
		return;
	}
	const EShinobiSex Sex = Look.Sex;
	const int32 Faces = Catalog ? Catalog->CountFaces(Sex) : 0;
	AddPicker(TEXT("Face"), &FShinobiAppearance::Face, Faces, false);
	TArray<FString> BlendNames = { TEXT("None") };
	for (int32 Index = 0; Index < Faces; ++Index)
	{
		BlendNames.Add(FString::Printf(TEXT("%d"), Index + 1));
	}
	for (int32 Blend = 0; Blend < 2; ++Blend)
	{
		const uint8 Current = Look.BlendFace[Blend];
		AddChoice(FString::Printf(TEXT("Blend with %d"), Blend + 1), BlendNames, Current == FShinobiAppearance::None || Current >= Faces ? 0 : Current + 1,
			[this, Blend](int32 Picked)
			{
				EditLook([Blend, Picked](FShinobiAppearance& L) { L.BlendFace[Blend] = Picked == 0 ? FShinobiAppearance::None : uint8(Picked - 1); }, EShinobiLookEdit::Rebuild);
			});
		if (Current != FShinobiAppearance::None)
		{
			AddSlider(TEXT("  amount"), Look.BlendWeight[Blend] / 255.f, true, [this, Blend](float V)
			{
				EditLook([Blend, V](FShinobiAppearance& L) { L.BlendWeight[Blend] = uint8(FMath::RoundToInt(V * 255.f)); }, EShinobiLookEdit::Shape);
			});
		}
	}
	const auto Grooms = [this, Sex](EShinobiGroomSlot GroomSlot) { return Catalog ? Catalog->CountGrooms(Sex, GroomSlot) : 0; };
	AddPicker(TEXT("Hair"), &FShinobiAppearance::Hair, Grooms(EShinobiGroomSlot::Hair), true);
	AddPicker(TEXT("Beard"), &FShinobiAppearance::Beard, Grooms(EShinobiGroomSlot::Beard), true);
	AddPicker(TEXT("Mustache"), &FShinobiAppearance::Mustache, Grooms(EShinobiGroomSlot::Mustache), true);
	const TArray<FName> Sliders = UShinobiFaceShapeAnimInstance::GetSliderNames();
	for (int32 Index = 0; Index < Sliders.Num(); ++Index)
	{
		AddSlider(FName::NameToDisplayString(Sliders[Index].ToString(), false), Look.FaceShape.IsValidIndex(Index) ? Look.FaceShape[Index] / 127.f : 0.f, false,
			[this, Index](float V)
			{
				EditLook([Index, V](FShinobiAppearance& L)
				{
					L.FaceShape.SetNumZeroed(FMath::Max(L.FaceShape.Num(), Index + 1));
					L.FaceShape[Index] = CreatorShape(V);
				}, EShinobiLookEdit::Shape);
			});
	}
}

void UShinobiCreatorPage::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	TickEdits(DeltaTime);
	const UShinobiBodyComponent* Body = Stage.IsValid() ? Stage->GetBody() : nullptr;
	Spinner->SetVisibility(Body && Body->IsMutableBuildPending() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ShinobiMenuStyle::TickFocus(*WidgetTree);
}
