#include "Shinobi/UI/ShinobiRosterPage.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiDropdown.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Shinobi/UI/ShinobiSettings.h"

namespace
{
	constexpr int32 RosterCount = UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster);
	constexpr int32 RosterOutfitCount = UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits);
	const FVector2D RosterCardSize(120.f, 160.f);
	const FMargin RosterCardPadding(6.f);

	int32 RosterSavedPick()
	{
		return FMath::Max(0, AShinobiPlayerCharacter::FindRosterIndex(ShinobiSettings::GetSavedCharacter()));
	}

	int32 RosterSavedOutfit()
	{
		return FMath::Max(0, AShinobiPlayerCharacter::FindOutfitIndex(ShinobiSettings::GetSavedOutfit()));
	}

	int32 RosterSavedFace(int32 Body)
	{
		return FMath::Max(0, AShinobiPlayerCharacter::FindFaceIndex(Body, ShinobiSettings::GetSavedFace()));
	}
}

UShinobiIndexButton* UShinobiRosterPage::MakeCard(UTexture2D* Picture, const FString& Initial, int32 InitialSize, const FString& Label, int32 Index,
	FShinobiIndexClicked Handler, UTextBlock*& OutLabel)
{
	UShinobiIndexButton* Card = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
	Card->Index = Index;
	Card->OnIndexClicked = MoveTemp(Handler);
	Card->Arm();
	UVerticalBox* Face = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->AddChild(Face);
	USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PortraitBox->SetWidthOverride(RosterCardSize.X);
	PortraitBox->SetHeightOverride(RosterCardSize.Y);
	UOverlay* Portrait = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	PortraitBox->AddChild(Portrait);
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	// No picture (not captured yet, a made character, "+"): a dark accent card with the initial on it.
	Image->SetBrush(Picture ? ShinobiMenuStyle::TextureBrush(Picture, RosterCardSize)
		: ShinobiMenuStyle::ColorBrush(ShinobiMenuStyle::Accent * FLinearColor(0.35f, 0.35f, 0.35f, 1.f)));
	UOverlaySlot* PictureSlot = Portrait->AddChildToOverlay(Image);
	PictureSlot->SetHorizontalAlignment(HAlign_Fill);
	PictureSlot->SetVerticalAlignment(VAlign_Fill);
	if (!Picture)
	{
		UTextBlock* Glyph = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Initial), InitialSize,
			FLinearColor(1.f, 1.f, 1.f, Initial == TEXT("+") ? 0.85f : 0.35f), true); // ("+" reads as a button)
		UOverlaySlot* GlyphSlot = Portrait->AddChildToOverlay(Glyph);
		GlyphSlot->SetHorizontalAlignment(HAlign_Center);
		GlyphSlot->SetVerticalAlignment(VAlign_Center);
	}
	Face->AddChildToVerticalBox(PortraitBox);
	OutLabel = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), 14, FLinearColor::White, true);
	UVerticalBoxSlot* NameSlot = Face->AddChildToVerticalBox(OutLabel);
	NameSlot->SetHorizontalAlignment(HAlign_Center);
	NameSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	return Card;
}

TSharedRef<SWidget> UShinobiRosterPage::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Bottom: one card per character, then the made ones and "+".
		UHorizontalBox* Strip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UCanvasPanelSlot* StripSlot = Root->AddChildToCanvas(Strip);
		StripSlot->SetAnchors(FAnchors(0.5f, 1.f));
		StripSlot->SetAlignment(FVector2D(0.5f, 1.f));
		StripSlot->SetPosition(FVector2D(0.f, -24.f));
		StripSlot->SetAutoSize(true);
		for (int32 Index = 0; Index < RosterCount; ++Index)
		{
			const FString Name = AShinobiPlayerCharacter::Roster[Index].Name;
			UTextBlock* Label = nullptr;
			UShinobiIndexButton* Card = MakeCard(ShinobiMenuArt::LoadArt(ShinobiMenuArt::PortraitPath(Index)), Name.Left(1), 64, Name, Index,
				FShinobiIndexClicked::CreateUObject(this, &UShinobiRosterPage::Pick), Label);
			CardNames.Add(Label);
			Strip->AddChildToHorizontalBox(Card)->SetPadding(FMargin(6.f, 0.f));
			Cards.Add(Card);
		}
		MadeStrip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Strip->AddChildToHorizontalBox(MadeStrip);

		// Right: who, and what they wear.
		UBorder* Panel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(28.f, 22.f));
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(1.f, 0.36f));
		PanelSlot->SetAlignment(FVector2D(1.f, 0.5f));
		PanelSlot->SetPosition(FVector2D(-48.f, 0.f));
		PanelSlot->SetAutoSize(true);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		Column->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("SHINOBI"), 12, ShinobiMenuStyle::Accent));
		NameText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 44, FLinearColor::White, true);
		Column->AddChildToVerticalBox(NameText);
		// "< value >" under a label: the arrows step it by -1 / +1.
		const auto Stepper = [this, Column](const FText& Label, void (UShinobiRosterPage::*Step)(int32), TObjectPtr<UTextBlock>& Value)
		{
			UTextBlock* Heading = ShinobiMenuStyle::Text(*WidgetTree, Label, 12, ShinobiMenuStyle::TextDim);
			Column->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.f, 14.f, 0.f, 4.f));
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Column->AddChildToVerticalBox(Row);
			const auto Arrow = [this, Step](const TCHAR* Glyph, int32 Direction)
			{
				UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
				Button->Index = Direction;
				Button->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, Step);
				Button->Arm();
				Button->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Glyph), 16, FLinearColor::White));
				ShinobiMenuStyle::StyleButton(*Button, false);
				return Button;
			};
			Row->AddChildToHorizontalBox(Arrow(TEXT("<"), -1));
			Value = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 20, FLinearColor::White);
			Value->SetMinDesiredWidth(200.f);
			Value->SetJustification(ETextJustify::Center);
			UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value);
			ValueSlot->SetVerticalAlignment(VAlign_Center);
			ValueSlot->SetPadding(FMargin(10.f, 0.f));
			Row->AddChildToHorizontalBox(Arrow(TEXT(">"), +1));
			return TArray<TObjectPtr<UWidget>>{Heading, Row};
		};
		FaceRow = Stepper(INVTEXT("FACE"), &UShinobiRosterPage::StepFace, FaceText);
		// The outfits: a list to pick from (it grows with every outfit imported).
		Column->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("OUTFIT"), 12, ShinobiMenuStyle::TextDim))->SetPadding(FMargin(0.f, 14.f, 0.f, 4.f));
		OutfitField = WidgetTree->ConstructWidget<UShinobiDropdown>(UShinobiDropdown::StaticClass());
		OutfitField->OnPicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiRosterPage::PickOutfit);
		OutfitField->Build(*WidgetTree, 18, 300.f);
		Column->AddChildToVerticalBox(OutfitField);
		Column->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("LT / RT  change outfit"), 11, ShinobiMenuStyle::TextDim))->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		// A made character: back into the creator, or away.
		UHorizontalBox* MadeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(MadeRow)->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		MadeButtons = MadeRow;
		const auto MadeButton = [this, MadeRow](const TCHAR* Label, bool bPrimary, TFunction<void()> Action)
		{
			UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
			Button->OnIndexClicked = FShinobiIndexClicked::CreateWeakLambda(this, [Action = MoveTemp(Action)](int32) { Action(); });
			Button->Arm();
			Button->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), 15, FLinearColor::White));
			ShinobiMenuStyle::StyleButton(*Button, bPrimary);
			MadeRow->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			return Button;
		};
		EditButton = MadeButton(TEXT("EDIT"), true, [this]()
		{
			const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
			if (Made->GetActiveLook())
			{
				OnOpenCreator.ExecuteIfBound(Made->Active);
			}
		});
		DeleteButton = MadeButton(TEXT("DELETE"), false, [this]()
		{
			const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
			if (!Made->GetActiveLook())
			{
				return;
			}
			if (!bDeleteArmed)
			{
				bDeleteArmed = true; // (a second press deletes)
				if (UTextBlock* Label = Cast<UTextBlock>(DeleteButton->GetChildAt(0)))
				{
					Label->SetText(INVTEXT("CONFIRM DELETE"));
				}
				return;
			}
			DeleteMade(Made->Active);
			if (UWidget* Target = GetFocusTarget(); Target && GetOwningPlayer())
			{
				Target->SetKeyboardFocus(); // (the button is hidden now)
			}
		});
		Show();
	}
	return Super::RebuildWidget();
}

void UShinobiRosterPage::Pick(int32 RosterIndex)
{
	ShinobiSettings::SetSavedCharacter(AShinobiPlayerCharacter::Roster[FMath::Clamp(RosterIndex, 0, RosterCount - 1)].Name);
	Show();
}

void UShinobiRosterPage::PickMade(int32 SlotIndex)
{
	UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
	if (Made->Slots.IsValidIndex(SlotIndex) && !Made->Slots[SlotIndex].Name.IsEmpty() && Made->Active != SlotIndex)
	{
		Made->Active = SlotIndex;
		Made->Save();
	}
	Show();
}

void UShinobiRosterPage::DeleteMade(int32 SlotIndex)
{
	UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
	if (!Made->Slots.IsValidIndex(SlotIndex))
	{
		return;
	}
	Made->Slots.RemoveAt(SlotIndex);
	Made->Active = Made->Active == SlotIndex ? INDEX_NONE : Made->Active > SlotIndex ? Made->Active - 1 : Made->Active;
	Made->Save();
	Show();
}

int32 UShinobiRosterPage::FindFreeSlot()
{
	const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
	for (int32 SlotIndex = 0; SlotIndex < UShinobiCharacterSave::MaxSlots; ++SlotIndex)
	{
		if (!Made->Slots.IsValidIndex(SlotIndex) || Made->Slots[SlotIndex].Name.IsEmpty())
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

TArray<int32> UShinobiRosterPage::MadeSlots()
{
	return UShinobiCharacterSave::Load()->GetMadeSlots();
}

UShinobiIndexButton* UShinobiRosterPage::GetCreateCard() const
{
	return CreateCard && CreateCard->GetVisibility() != ESlateVisibility::Collapsed ? CreateCard.Get() : nullptr;
}

UShinobiIndexButton* UShinobiRosterPage::GetMadeCard(int32 SlotIndex) const
{
	for (UShinobiIndexButton* Card : MadeCards)
	{
		if (Card && Card->Index == SlotIndex)
		{
			return Card;
		}
	}
	return nullptr;
}

void UShinobiRosterPage::RebuildMadeCards()
{
	if (!MadeStrip)
	{
		return;
	}
	const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
	const TArray<int32> Slots = MadeSlots();
	TArray<FString> Names;
	for (const int32 SlotIndex : Slots)
	{
		const FShinobiCharacterSlot& Slot = Made->Slots[SlotIndex];
		Names.Add(FString::Printf(TEXT("%s|%08x"), *Slot.Name, FCrc::MemCrc32(Slot.Portrait.GetData(), Slot.Portrait.Num()))); // (a new portrait rebuilds too)
	}
	Names.Add(FindFreeSlot() != INDEX_NONE ? TEXT("+") : TEXT("")); // ("+" comes and goes too)
	if (CreateCard && Names == BuiltMadeNames)
	{
		return;
	}
	// A focused made card or "+" hands focus to the card in its place once rebuilt.
	int32 FocusedPlace = INDEX_NONE;
	for (int32 Place = 0; Place < MadeStrip->GetChildrenCount(); ++Place)
	{
		FocusedPlace = MadeStrip->GetChildAt(Place)->HasAnyUserFocus() ? Place : FocusedPlace;
	}
	BuiltMadeNames = Names;
	MadeStrip->ClearChildren();
	MadeCards.Reset();
	MadeCardNames.Reset();
	for (const int32 SlotIndex : Slots)
	{
		const FString& Name = Made->Slots[SlotIndex].Name;
		UTextBlock* Label = nullptr;
		UShinobiIndexButton* Card = MakeCard(ShinobiMenuArt::MadePortrait(Made->Slots[SlotIndex]), Name.Left(1), 64, Name.Left(12), SlotIndex,
			FShinobiIndexClicked::CreateUObject(this, &UShinobiRosterPage::PickMade), Label);
		MadeStrip->AddChildToHorizontalBox(Card)->SetPadding(FMargin(6.f, 0.f));
		MadeCards.Add(Card);
		MadeCardNames.Add(Label);
	}
	UTextBlock* Label = nullptr;
	CreateCard = MakeCard(nullptr, TEXT("+"), 72, TEXT("CREATE"), 0, FShinobiIndexClicked::CreateWeakLambda(this, [this](int32)
	{
		if (const int32 Free = FindFreeSlot(); Free != INDEX_NONE)
		{
			OnOpenCreator.ExecuteIfBound(Free);
		}
	}), Label);
	CreateCardName = Label;
	CreateCard->SetToolTipText(INVTEXT("Make your own shinobi"));
	CreateCard->SetVisibility(FindFreeSlot() != INDEX_NONE ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	MadeStrip->AddChildToHorizontalBox(CreateCard)->SetPadding(FMargin(6.f, 0.f));
	if (FocusedPlace != INDEX_NONE && GetOwningPlayer())
	{
		TArray<UWidget*> Shown;
		for (int32 Place = 0; Place < MadeStrip->GetChildrenCount(); ++Place)
		{
			if (MadeStrip->GetChildAt(Place)->GetVisibility() != ESlateVisibility::Collapsed)
			{
				Shown.Add(MadeStrip->GetChildAt(Place));
			}
		}
		UWidget* Target = Shown.Num() > 0 ? Shown[FMath::Min(FocusedPlace, Shown.Num() - 1)] : GetFocusTarget();
		if (Target)
		{
			Target->SetKeyboardFocus();
		}
	}
}

void UShinobiRosterPage::StepOutfit(int32 Direction)
{
	PickOutfit(((RosterSavedOutfit() + Direction) % RosterOutfitCount + RosterOutfitCount) % RosterOutfitCount);
}

void UShinobiRosterPage::PickOutfit(int32 OutfitIndex)
{
	ShinobiSettings::SetSavedOutfit(AShinobiPlayerCharacter::Outfits[FMath::Clamp(OutfitIndex, 0, RosterOutfitCount - 1)].Name);
	Show();
}

void UShinobiRosterPage::StepFace(int32 Direction)
{
	const int32 Picked = RosterSavedPick();
	const int32 Count = AShinobiPlayerCharacter::NumFaces(Picked);
	if (const FShinobiFaceEntry* Face = AShinobiPlayerCharacter::FindFace(Picked, ((RosterSavedFace(Picked) + Direction) % Count + Count) % Count))
	{
		ShinobiSettings::SetSavedFace(Face->Name);
		Show();
	}
}

UWidget* UShinobiRosterPage::GetFocusTarget() const
{
	const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
	if (UShinobiIndexButton* MadeCard = Made->GetActiveLook() ? GetMadeCard(Made->Active) : nullptr)
	{
		return MadeCard;
	}
	const int32 Picked = RosterSavedPick();
	return Cards.IsValidIndex(Picked) ? Cards[Picked].Get() : nullptr;
}

void UShinobiRosterPage::Show(bool bWear)
{
	RebuildMadeCards();
	const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
	const int32 Active = Made->GetActiveLook() ? Made->Active : INDEX_NONE; // a made character is played
	const bool bMade = Active != INDEX_NONE;
	const int32 Picked = RosterSavedPick();
	const int32 Outfit = RosterSavedOutfit();
	const auto StyleCard = [](UShinobiIndexButton& Card, UTextBlock& Name, bool bPicked)
	{
		ShinobiMenuStyle::StyleButton(Card, bPicked, RosterCardPadding);
		Card.SetRenderTranslation(FVector2D(0.f, bPicked ? -10.f : 0.f));
		Card.SetRenderScale(FVector2D(bPicked ? 1.08f : 1.f));
		Name.SetColorAndOpacity(FSlateColor(bPicked ? FLinearColor::Black : FLinearColor::White)); // on the filled card
	};
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		StyleCard(*Cards[Index], *CardNames[Index], !bMade && Index == Picked);
	}
	for (int32 Index = 0; Index < MadeCards.Num(); ++Index)
	{
		StyleCard(*MadeCards[Index], *MadeCardNames[Index], MadeCards[Index]->Index == Active);
	}
	if (CreateCard)
	{
		StyleCard(*CreateCard, *CreateCardName, false);
	}
	if (NameText)
	{
		NameText->SetText(FText::FromString((bMade ? Made->Slots[Active].Name : FString(AShinobiPlayerCharacter::Roster[Picked].Name)).ToUpper()));
	}
	const int32 Face = RosterSavedFace(Picked);
	for (UWidget* Part : FaceRow)
	{
		Part->SetVisibility(!bMade && AShinobiPlayerCharacter::NumFaces(Picked) > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (const FShinobiFaceEntry* Entry = FaceText ? AShinobiPlayerCharacter::FindFace(Picked, Face) : nullptr)
	{
		FString Line = Entry->Name;
		if (!AShinobiPlayerCharacter::IsFaceBuilt(Picked, Face))
		{
			Line += TEXT(" (not built)");
		}
		FaceText->SetText(FText::FromString(Line));
	}
	if (OutfitField)
	{
		TArray<FString> Names;
		for (int32 Index = 0; Index < RosterOutfitCount; ++Index)
		{
			// (what's built depends on the roster character; a made one wears the outfit on its own build)
			Names.Add(FString(AShinobiPlayerCharacter::Outfits[Index].Name) + (bMade || AShinobiPlayerCharacter::IsOutfitBuilt(Index, Picked) ? TEXT("") : TEXT(" (not built)")));
		}
		OutfitField->SetOptions(Names, Outfit);
	}
	if (MadeButtons)
	{
		MadeButtons->SetVisibility(bMade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		bDeleteArmed = false;
		if (UTextBlock* Label = Cast<UTextBlock>(DeleteButton->GetChildAt(0)))
		{
			Label->SetText(INVTEXT("DELETE"));
		}
	}
	if (AShinobiMenuStage* Shown = bWear ? Stage.Get() : nullptr)
	{
		if (bMade)
		{
			Shown->ShowAppearance(Made->Slots[Active].Look, Outfit);
			if (Made->Slots[Active].Portrait.IsEmpty())
			{
				RequestMadePortrait(*Shown, Made->Slots[Active].Look);
			}
		}
		else
		{
			Shown->Show(Picked, Outfit, Face);
		}
	}
}

void UShinobiRosterPage::RequestMadePortrait(AShinobiMenuStage& Shown, const FShinobiAppearance& Look)
{
	Shown.RequestPortrait(Look, [WeakThis = TWeakObjectPtr<UShinobiRosterPage>(this), Look](TArray<uint8>&& Png)
	{
		// Every made character wearing that look gets it (the slots may have changed while it was filmed).
		UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
		bool bChanged = false;
		for (FShinobiCharacterSlot& Slot : Made->Slots)
		{
			if (!Slot.Name.IsEmpty() && Slot.Look == Look && Slot.Portrait.IsEmpty())
			{
				Slot.Portrait = Png;
				bChanged = true;
			}
		}
		if (bChanged && Made->Save())
		{
			if (UShinobiRosterPage* Page = WeakThis.Get())
			{
				Page->Show(false); // its card shows the picture (the stage keeps what it wears)
			}
		}
	});
}

void UShinobiRosterPage::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	ShinobiMenuStyle::TickFocus(*WidgetTree);
}

FReply UShinobiRosterPage::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Gamepad_LeftTrigger || Event.GetKey() == EKeys::Gamepad_RightTrigger)
	{
		StepOutfit(Event.GetKey() == EKeys::Gamepad_RightTrigger ? 1 : -1);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}
