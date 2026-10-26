#include "Shinobi/UI/ShinobiCreatorPage.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Shinobi/Hero/ShinobiFaceShapeAnimInstance.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Shinobi/UI/ShinobiSettings.h"

namespace
{
	const TCHAR* const CreatorCategoryNames[] = { TEXT("BODY"), TEXT("FACE"), TEXT("HAIR"), TEXT("CLOTHES") };
	const TCHAR* const CreatorBuildNames[] = { TEXT("Slim"), TEXT("Average"), TEXT("Heavy") };
	constexpr float CreatorPanelWidth = 470.f;
	constexpr float CreatorRowsHeight = 430.f;

	FString CreatorSlotLabel(const UShinobiCharacterSave* Save, int32 SlotIndex)
	{
		return Save && Save->Slots.IsValidIndex(SlotIndex) && !Save->Slots[SlotIndex].Name.IsEmpty()
			? FString::Printf(TEXT("%d %s"), SlotIndex + 1, *Save->Slots[SlotIndex].Name.Left(10)) : FString::Printf(TEXT("%d EMPTY"), SlotIndex + 1);
	}

	int8 CreatorShape(float Value) { return int8(FMath::Clamp(FMath::RoundToInt(Value * 127.f), -127, 127)); }
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

		const auto MakeButton = [this](const FString& Label, int32 Index, FShinobiIndexClicked Handler, int32 Size = 14)
		{
			UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
			Button->Index = Index;
			Button->OnIndexClicked = MoveTemp(Handler);
			Button->Arm();
			Button->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), Size, FLinearColor::White));
			ShinobiMenuStyle::StyleButton(*Button, false, FMargin(10.f, 6.f));
			return Button;
		};
		// Slots, then categories.
		UHorizontalBox* SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(SlotRow)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		for (int32 SlotIndex = 0; SlotIndex < UShinobiCharacterSave::MaxSlots; ++SlotIndex)
		{
			SlotButtons.Add(MakeButton(CreatorSlotLabel(Save, SlotIndex), SlotIndex, FShinobiIndexClicked::CreateUObject(this, &UShinobiCreatorPage::EditSlot), 12));
			SlotRow->AddChildToHorizontalBox(SlotButtons.Last())->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		UHorizontalBox* CategoryRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(CategoryRow)->SetPadding(FMargin(0.f, 12.f, 0.f, 8.f));
		for (int32 Index = 0; Index < int32(ECategory::Num); ++Index)
		{
			CategoryButtons.Add(MakeButton(CreatorCategoryNames[Index], Index,
				FShinobiIndexClicked::CreateWeakLambda(this, [this](int32 Picked) { ShowCategory(ECategory(Picked)); })));
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
		Buttons->AddChildToHorizontalBox(MakeButton(TEXT("LOAD CODE"), 0, FShinobiIndexClicked::CreateWeakLambda(this, [this](int32) { LoadCode(); })))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		UShinobiIndexButton* Play = MakeButton(TEXT("SAVE & PLAY"), 0, FShinobiIndexClicked::CreateWeakLambda(this, [this](int32) { SaveAndPlay(); }), 16);
		ShinobiMenuStyle::StyleButton(*Play, true, FMargin(14.f, 6.f));
		Buttons->AddChildToHorizontalBox(Play);
		MessageText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 12, FLinearColor(1.f, 0.75f, 0.3f));
		Column->AddChildToVerticalBox(MessageText)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		if (!Catalog)
		{
			MessageText->SetText(INVTEXT("The creator catalog isn't built (Scripts/setup_creator.py)."));
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

void UShinobiCreatorPage::ShowCategory(ECategory NewCategory)
{
	Category = NewCategory;
	Refresh(false);
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
	MessageText->SetText(INVTEXT("Code loaded: SAVE & PLAY keeps it."));
	Refresh(true);
	return true;
}

UWidget* UShinobiCreatorPage::GetFocusTarget() const
{
	return SlotButtons.IsValidIndex(EditedSlot) ? SlotButtons[EditedSlot].Get() : nullptr;
}

void UShinobiCreatorPage::ResetForSex()
{
	const FShinobiAppearance Fresh;
	Look.Face = Fresh.Face;
	Look.Hair = Fresh.Hair;
	Look.Beard = Fresh.Beard;
	Look.Mustache = Fresh.Mustache;
	Look.Top = Fresh.Top;
	Look.Bottom = Fresh.Bottom;
	Look.Shoes = Fresh.Shoes;
	Look.FaceShape.Reset();
	Look.BlendFace[0] = Look.BlendFace[1] = FShinobiAppearance::None;
	Look.BlendWeight[0] = Look.BlendWeight[1] = 0;
}

void UShinobiCreatorPage::AddPicker(const FString& Label, uint8& Field, int32 Count, bool bNone, TFunction<FString(int32)> Name)
{
	if (Count <= 0 && !bNone)
	{
		return; // nothing to pick
	}
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 3.f));
	UTextBlock* Caption = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), 14, ShinobiMenuStyle::TextDim);
	Caption->SetMinDesiredWidth(150.f);
	Row->AddChildToHorizontalBox(Caption)->SetVerticalAlignment(VAlign_Center);
	const bool bIsNone = Field == FShinobiAppearance::None || Field >= Count;
	const FString Value = bIsNone ? (bNone ? FString(TEXT("None")) : FString(TEXT("-"))) : Name ? Name(Field) : FString::Printf(TEXT("%d"), Field + 1);
	uint8* Target = &Field; // a member of Look (or Cloak): lives as long as the page
	for (const int32 Direction : {-1, +1})
	{
		UShinobiIndexButton* Arrow = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
		Arrow->Index = Direction;
		Arrow->OnIndexClicked = FShinobiIndexClicked::CreateWeakLambda(this, [this, Target, Count, bNone](int32 Step)
		{
			// None sits before 0 when allowed: cycle over Count (+1) positions.
			const int32 Positions = Count + (bNone ? 1 : 0);
			const int32 Now = (*Target == FShinobiAppearance::None || *Target >= Count) ? 0 : *Target + (bNone ? 1 : 0);
			const int32 Next = ((Now + Step) % Positions + Positions) % Positions;
			*Target = bNone ? (Next == 0 ? FShinobiAppearance::None : uint8(Next - 1)) : uint8(Next);
			if (Target == reinterpret_cast<uint8*>(&Look.Sex))
			{
				ResetForSex();
			}
			bRedressPending = true;
		});
		Arrow->Arm();
		Arrow->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Direction < 0 ? TEXT("<") : TEXT(">")), 14, FLinearColor::White));
		ShinobiMenuStyle::StyleButton(*Arrow, false, FMargin(10.f, 4.f));
		Row->AddChildToHorizontalBox(Arrow);
		if (Direction < 0)
		{
			UTextBlock* Shown = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Value), 14, FLinearColor::White);
			Shown->SetMinDesiredWidth(150.f);
			Shown->SetJustification(ETextJustify::Center);
			UHorizontalBoxSlot* ShownSlot = Row->AddChildToHorizontalBox(Shown);
			ShownSlot->SetVerticalAlignment(VAlign_Center);
			ShownSlot->SetPadding(FMargin(6.f, 0.f));
		}
	}
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
	Slider->OnIndexValue = FShinobiIndexValue::CreateWeakLambda(this, [this, Set](int32, float NewValue)
	{
		Set(NewValue);
		if (CodeBox)
		{
			CodeBox->SetText(FText::FromString(Look.ToCode()));
		}
	});
	Slider->Arm();
	UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SliderSlot->SetVerticalAlignment(VAlign_Center);
}

void UShinobiCreatorPage::Refresh(bool bRedress)
{
	if (!Rows)
	{
		return;
	}
	if (bRedress)
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
	for (int32 SlotIndex = 0; SlotIndex < SlotButtons.Num(); ++SlotIndex)
	{
		ShinobiMenuStyle::StyleButton(*SlotButtons[SlotIndex], SlotIndex == EditedSlot, FMargin(10.f, 6.f));
		if (UTextBlock* Label = Cast<UTextBlock>(SlotButtons[SlotIndex]->GetChildAt(0)))
		{
			const bool bPlayed = Save && Save->Active == SlotIndex;
			Label->SetText(FText::FromString(CreatorSlotLabel(Save, SlotIndex) + (bPlayed ? TEXT(" *") : TEXT(""))));
		}
	}
	for (int32 Index = 0; Index < CategoryButtons.Num(); ++Index)
	{
		ShinobiMenuStyle::StyleButton(*CategoryButtons[Index], Index == int32(Category), FMargin(10.f, 6.f));
	}
	CodeBox->SetText(FText::FromString(Look.ToCode()));

	Rows->ClearChildren();
	UShinobiBodyComponent* Body = Stage.IsValid() ? Stage->GetBody() : nullptr;
	if (Stage.IsValid())
	{
		Stage->SetCloseUp(Category == ECategory::Face || Category == ECategory::Hair);
	}
	const EShinobiSex Sex = Look.Sex;
	const auto Count = [this, Sex](auto Counter) { return Catalog ? Counter(*Catalog, Sex) : 0; };
	switch (Category)
	{
	case ECategory::Body:
	{
		AddPicker(TEXT("Sex"), reinterpret_cast<uint8&>(Look.Sex), 2, false, [](int32 V) { return FString(V ? TEXT("Female") : TEXT("Male")); });
		AddPicker(TEXT("Build"), reinterpret_cast<uint8&>(Look.Build), 3, false, [](int32 V) { return FString(CreatorBuildNames[V]); });
		const TArray<FName> Sliders = UShinobiBodyComponent::GetBodySliderNames();
		for (int32 Index = 0; Index < Sliders.Num(); ++Index)
		{
			AddSlider(FName::NameToDisplayString(Sliders[Index].ToString(), false), Look.BodyShape.IsValidIndex(Index) ? Look.BodyShape[Index] / 127.f : 0.f, false,
				[this, Index, Body](float V)
				{
					Look.BodyShape.SetNumZeroed(FMath::Max(Look.BodyShape.Num(), Index + 1));
					Look.BodyShape[Index] = CreatorShape(V);
					if (Body) { Body->UpdateBody(Look); }
				});
		}
		break;
	}
	case ECategory::Face:
	{
		const int32 Faces = Count([](const UShinobiCreatorCatalog& C, EShinobiSex S) { return C.CountFaces(S); });
		AddPicker(TEXT("Face"), Look.Face, Faces, false);
		for (int32 SlotIndex = 0; SlotIndex < 2; ++SlotIndex)
		{
			AddPicker(FString::Printf(TEXT("Blend with %d"), SlotIndex + 1), Look.BlendFace[SlotIndex], Faces, true);
			if (Look.BlendFace[SlotIndex] != FShinobiAppearance::None)
			{
				AddSlider(TEXT("  amount"), Look.BlendWeight[SlotIndex] / 255.f, true, [this, SlotIndex, Body](float V)
				{
					Look.BlendWeight[SlotIndex] = uint8(FMath::RoundToInt(V * 255.f));
					if (Body) { Body->UpdateFace(Look); }
				});
			}
		}
		const TArray<FName> Sliders = UShinobiFaceShapeAnimInstance::GetSliderNames();
		for (int32 Index = 0; Index < Sliders.Num(); ++Index)
		{
			AddSlider(FName::NameToDisplayString(Sliders[Index].ToString(), false), Look.FaceShape.IsValidIndex(Index) ? Look.FaceShape[Index] / 127.f : 0.f, false,
				[this, Index, Body](float V)
				{
					Look.FaceShape.SetNumZeroed(FMath::Max(Look.FaceShape.Num(), Index + 1));
					Look.FaceShape[Index] = CreatorShape(V);
					if (Body) { Body->UpdateFace(Look); }
				});
		}
		break;
	}
	case ECategory::Hair:
		AddPicker(TEXT("Hair"), Look.Hair, Count([](const UShinobiCreatorCatalog& C, EShinobiSex S) { return C.CountGrooms(S, EShinobiGroomSlot::Hair); }), true);
		AddPicker(TEXT("Beard"), Look.Beard, Count([](const UShinobiCreatorCatalog& C, EShinobiSex S) { return C.CountGrooms(S, EShinobiGroomSlot::Beard); }), true);
		AddPicker(TEXT("Mustache"), Look.Mustache, Count([](const UShinobiCreatorCatalog& C, EShinobiSex S) { return C.CountGrooms(S, EShinobiGroomSlot::Mustache); }), true);
		break;
	case ECategory::Clothes:
	default:
		// (no None: a crowd frame's skin lives inside its clothes)
		AddPicker(TEXT("Top"), Look.Top, Count([](const UShinobiCreatorCatalog& C, EShinobiSex S) { return C.CountStyles(S, EShinobiWardrobeSlot::Top); }), false);
		AddPicker(TEXT("Bottom"), Look.Bottom, Count([](const UShinobiCreatorCatalog& C, EShinobiSex S) { return C.CountStyles(S, EShinobiWardrobeSlot::Bottom); }), false);
		AddPicker(TEXT("Shoes"), Look.Shoes, Count([](const UShinobiCreatorCatalog& C, EShinobiSex S) { return C.CountStyles(S, EShinobiWardrobeSlot::Shoes); }), false);
		AddPicker(TEXT("Cloak"), Cloak, UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits), false,
			[](int32 V) { return FString(AShinobiPlayerCharacter::Outfits[V].Name); });
		break;
	}
}

void UShinobiCreatorPage::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (bRedressPending)
	{
		bRedressPending = false;
		Refresh(true);
	}
	ShinobiMenuStyle::TickFocus(*WidgetTree);
}
