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
#include "Shinobi/ShinobiPlayerCharacter.h"
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
}

TSharedRef<SWidget> UShinobiRosterPage::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Bottom: one card per character.
		UHorizontalBox* Strip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UCanvasPanelSlot* StripSlot = Root->AddChildToCanvas(Strip);
		StripSlot->SetAnchors(FAnchors(0.5f, 1.f));
		StripSlot->SetAlignment(FVector2D(0.5f, 1.f));
		StripSlot->SetPosition(FVector2D(0.f, -24.f));
		StripSlot->SetAutoSize(true);
		for (int32 Index = 0; Index < RosterCount; ++Index)
		{
			UShinobiIndexButton* Card = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
			Card->Index = Index;
			Card->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiRosterPage::Pick);
			Card->Arm();
			UVerticalBox* Face = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Card->AddChild(Face);
			USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			PortraitBox->SetWidthOverride(RosterCardSize.X);
			PortraitBox->SetHeightOverride(RosterCardSize.Y);
			UOverlay* Portrait = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			PortraitBox->AddChild(Portrait);
			UImage* Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			UTexture2D* Art = ShinobiMenuArt::LoadArt(ShinobiMenuArt::PortraitPath(Index));
			// Not captured yet: a dark accent card with the initial on it.
			Picture->SetBrush(Art ? ShinobiMenuStyle::TextureBrush(Art, RosterCardSize)
				: ShinobiMenuStyle::ColorBrush(ShinobiMenuStyle::Accent * FLinearColor(0.35f, 0.35f, 0.35f, 1.f)));
			UOverlaySlot* PictureSlot = Portrait->AddChildToOverlay(Picture);
			PictureSlot->SetHorizontalAlignment(HAlign_Fill);
			PictureSlot->SetVerticalAlignment(VAlign_Fill);
			if (!Art)
			{
				UTextBlock* Initial = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(FString(AShinobiPlayerCharacter::Roster[Index].Name).Left(1)),
					64, FLinearColor(1.f, 1.f, 1.f, 0.35f), true);
				UOverlaySlot* InitialSlot = Portrait->AddChildToOverlay(Initial);
				InitialSlot->SetHorizontalAlignment(HAlign_Center);
				InitialSlot->SetVerticalAlignment(VAlign_Center);
			}
			Face->AddChildToVerticalBox(PortraitBox);
			UTextBlock* Name = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(AShinobiPlayerCharacter::Roster[Index].Name), 14, FLinearColor::White, true);
			UVerticalBoxSlot* NameSlot = Face->AddChildToVerticalBox(Name);
			NameSlot->SetHorizontalAlignment(HAlign_Center);
			NameSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			CardNames.Add(Name);
			Strip->AddChildToHorizontalBox(Card)->SetPadding(FMargin(6.f, 0.f));
			Cards.Add(Card);
		}

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
		Column->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("OUTFIT"), 12, ShinobiMenuStyle::TextDim))->SetPadding(FMargin(0.f, 14.f, 0.f, 4.f));
		UHorizontalBox* OutfitRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(OutfitRow);
		const auto Arrow = [this](const TCHAR* Label, int32 Direction)
		{
			UShinobiIndexButton* Button = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
			Button->Index = Direction;
			Button->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiRosterPage::StepOutfit);
			Button->Arm();
			Button->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Label), 16, FLinearColor::White));
			ShinobiMenuStyle::StyleButton(*Button, false);
			return Button;
		};
		OutfitRow->AddChildToHorizontalBox(Arrow(TEXT("<"), -1));
		OutfitText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 20, FLinearColor::White);
		OutfitText->SetMinDesiredWidth(200.f);
		OutfitText->SetJustification(ETextJustify::Center);
		UHorizontalBoxSlot* OutfitSlot = OutfitRow->AddChildToHorizontalBox(OutfitText);
		OutfitSlot->SetVerticalAlignment(VAlign_Center);
		OutfitSlot->SetPadding(FMargin(10.f, 0.f));
		OutfitRow->AddChildToHorizontalBox(Arrow(TEXT(">"), +1));
		Column->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("LT / RT  change outfit"), 11, ShinobiMenuStyle::TextDim))->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		Show();
	}
	return Super::RebuildWidget();
}

void UShinobiRosterPage::Pick(int32 RosterIndex)
{
	ShinobiSettings::SetSavedCharacter(AShinobiPlayerCharacter::Roster[FMath::Clamp(RosterIndex, 0, RosterCount - 1)].Name);
	Show();
}

void UShinobiRosterPage::StepOutfit(int32 Direction)
{
	const int32 Next = ((RosterSavedOutfit() + Direction) % RosterOutfitCount + RosterOutfitCount) % RosterOutfitCount;
	ShinobiSettings::SetSavedOutfit(AShinobiPlayerCharacter::Outfits[Next].Name);
	Show();
}

UWidget* UShinobiRosterPage::GetFocusTarget() const
{
	const int32 Picked = RosterSavedPick();
	return Cards.IsValidIndex(Picked) ? Cards[Picked].Get() : nullptr;
}

void UShinobiRosterPage::Show()
{
	const int32 Picked = RosterSavedPick();
	const int32 Outfit = RosterSavedOutfit();
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const bool bPicked = Index == Picked;
		ShinobiMenuStyle::StyleButton(*Cards[Index], bPicked, RosterCardPadding);
		Cards[Index]->SetRenderTranslation(FVector2D(0.f, bPicked ? -10.f : 0.f));
		Cards[Index]->SetRenderScale(FVector2D(bPicked ? 1.08f : 1.f));
		CardNames[Index]->SetColorAndOpacity(FSlateColor(bPicked ? FLinearColor::Black : FLinearColor::White)); // on the filled card
	}
	if (NameText)
	{
		NameText->SetText(FText::FromString(FString(AShinobiPlayerCharacter::Roster[Picked].Name).ToUpper()));
	}
	if (OutfitText)
	{
		FString Line = AShinobiPlayerCharacter::Outfits[Outfit].Name;
		if (!AShinobiPlayerCharacter::IsOutfitBuilt(Outfit, Picked))
		{
			Line += TEXT(" (not built)");
		}
		OutfitText->SetText(FText::FromString(Line));
	}
	if (AShinobiMenuStage* Shown = Stage.Get())
	{
		Shown->Show(Picked, Outfit);
	}
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
