#include "Shinobi/UI/ShinobiDropdown.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"

namespace
{
	/** Over every menu (the pause menu is 10, its pages' popups never go higher). */
	constexpr int32 DropdownZOrder = 1000;
	constexpr float DropdownItemHeight = 34.f;
	constexpr float DropdownMaxHeight = 380.f;
	constexpr float DropdownMargin = 12.f;
	const FLinearColor DropdownPanelColor(0.03f, 0.03f, 0.06f, 0.97f);

	TWeakObjectPtr<UShinobiDropdownList> DropdownOpenList;
}

// --- UShinobiDropdownList ---

UShinobiDropdownList* UShinobiDropdownList::Open(UWidget& InAnchor, const TArray<FString>& InOptions, int32 InSelected, TFunction<void(int32)> InOnPick, UWidget* PlaceUnder,
	const TArray<FString>& InLocked)
{
	UWorld* World = InAnchor.GetWorld();
	if (!World || !World->GetGameViewport() || InOptions.Num() == 0)
	{
		return nullptr;
	}
	CloseOpen();
	APlayerController* Player = InAnchor.GetOwningPlayer();
	UShinobiDropdownList* Popup = Player ? CreateWidget<UShinobiDropdownList>(Player, StaticClass()) : CreateWidget<UShinobiDropdownList>(World, StaticClass());
	if (!Popup)
	{
		return nullptr;
	}
	Popup->Options = InOptions;
	Popup->Locked = InLocked;
	Popup->Selected = FMath::Clamp(InSelected, 0, InOptions.Num() - 1);
	Popup->OnPick = MoveTemp(InOnPick);
	Popup->Anchor = &InAnchor;

	// Under the field, as wide as it; above it when there's no room below.
	const FGeometry& Field = (PlaceUnder ? *PlaceUnder : InAnchor).GetCachedGeometry();
	const FVector2D FieldPosition = Field.GetAbsolutePosition();
	const FVector2D FieldSize = Field.GetAbsoluteSize();
	FVector2D Pixel, Corner;
	USlateBlueprintLibrary::AbsoluteToViewport(World, FieldPosition, Pixel, Corner);
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(World), 0.01f);
	const FVector2D Size = FieldSize / Scale;
	const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(World) / Scale;
	const double Height = FMath::Min(double(InOptions.Num()) * DropdownItemHeight + 8.0, double(DropdownMaxHeight));
	const double Margin = DropdownMargin;
	const double ListWidth = FMath::Max(Size.X, 200.0);
	Popup->Width = float(ListWidth);
	Popup->bAbove = Corner.Y + Size.Y + Height > View.Y - Margin && Corner.Y - Height > Margin;
	Popup->Position = FVector2D(FMath::Clamp(Corner.X, Margin, FMath::Max(Margin, View.X - ListWidth - Margin)),
		Popup->bAbove ? Corner.Y - 4.0 : Corner.Y + Size.Y + 4.0);

	Popup->AddToViewport(DropdownZOrder);
	DropdownOpenList = Popup;
	Popup->FocusItem(Popup->Selected);
	return Popup;
}

UShinobiDropdownList* UShinobiDropdownList::GetOpen()
{
	UShinobiDropdownList* Popup = DropdownOpenList.Get();
	return Popup && Popup->IsInViewport() ? Popup : nullptr;
}

void UShinobiDropdownList::CloseOpen()
{
	if (UShinobiDropdownList* Popup = GetOpen())
	{
		Popup->Dismiss();
	}
}

TSharedRef<SWidget> UShinobiDropdownList::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true); // Esc / B reach it if no option has focus
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		// The whole screen takes the clicks: one outside the list closes it, and nothing underneath reacts.
		UBorder* Catcher = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Catcher"));
		Catcher->SetBrush(ShinobiMenuStyle::ColorBrush(FLinearColor::Transparent));
		UCanvasPanelSlot* CatcherSlot = Root->AddChildToCanvas(Catcher);
		CatcherSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CatcherSlot->SetOffsets(FMargin(0.f));

		Panel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(4.f));
		Panel->SetBrushColor(DropdownPanelColor);
		UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
		PanelSlot->SetAlignment(FVector2D(0.f, bAbove ? 1.f : 0.f));
		PanelSlot->SetPosition(Position);
		PanelSlot->SetAutoSize(true);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Width);
		Box->SetMaxDesiredHeight(DropdownMaxHeight);
		Panel->SetContent(Box);
		List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Options"));
		Box->AddChild(List);

		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			const bool bCurrent = Index == Selected;
			UShinobiIndexButton* Item = WidgetTree->ConstructWidget<UShinobiIndexButton>(UShinobiIndexButton::StaticClass());
			Item->Index = Index;
			Item->OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiDropdownList::Pick);
			Item->Arm();
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Item->AddChild(Line);
			USizeBox* TickBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			TickBox->SetWidthOverride(22.f);
			TickBox->AddChild(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(bCurrent ? TEXT("✓") : TEXT("")), 14, ShinobiMenuStyle::Accent));
			Line->AddChildToHorizontalBox(TickBox)->SetVerticalAlignment(VAlign_Center);
			const bool bLocked = IsOptionLocked(Index);
			UTextBlock* Name = ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(Options[Index]), 15,
				bLocked ? ShinobiMenuStyle::TextDim : bCurrent ? ShinobiMenuStyle::Accent : FLinearColor::White);
			UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Name);
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Center);
			if (bLocked)
			{
				// Greyed out with a tag; the full reason is on the settings help line.
				UHorizontalBoxSlot* TagSlot = Line->AddChildToHorizontalBox(ShinobiMenuStyle::Text(*WidgetTree, FText::FromString(TEXT("Locked")), 12, ShinobiMenuStyle::TextDim));
				TagSlot->SetVerticalAlignment(VAlign_Center);
				TagSlot->SetPadding(FMargin(8.f, 0.f, 6.f, 0.f));
			}
			ShinobiMenuStyle::StyleListItem(*Item, false);
			List->AddChild(Item);
			Cast<UButtonSlot>(Line->Slot)->SetHorizontalAlignment(HAlign_Fill);
			Cast<UScrollBoxSlot>(Item->Slot)->SetHorizontalAlignment(HAlign_Fill);
			Items.Add(Item);
		}
	}
	return Super::RebuildWidget();
}

void UShinobiDropdownList::FocusItem(int32 Index)
{
	if (Items.IsValidIndex(Index))
	{
		Items[Index]->SetKeyboardFocus();
		List->ScrollWidgetIntoView(Items[Index], false);
	}
	else
	{
		SetKeyboardFocus();
	}
}

int32 UShinobiDropdownList::GetFocusedItem() const
{
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Items[Index]->HasAnyUserFocus())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UShinobiDropdownList::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	// Keyboard and gamepad see the focused option lit like a hovered one.
	const int32 Focused = GetFocusedItem();
	if (Focused != LitItem)
	{
		if (Items.IsValidIndex(LitItem))
		{
			ShinobiMenuStyle::StyleListItem(*Items[LitItem], false);
		}
		if (Items.IsValidIndex(Focused))
		{
			ShinobiMenuStyle::StyleListItem(*Items[Focused], true);
		}
		LitItem = Focused;
	}
	if (!Anchor.IsValid() || !Anchor->IsVisible())
	{
		Dismiss(); // the field went away under it (a tab switch, the menu closing)
	}
}

void UShinobiDropdownList::Pick(int32 Index)
{
	if (IsOptionLocked(Index))
	{
		return; // stays open: pick another
	}
	TFunction<void(int32)> Picked = MoveTemp(OnPick);
	Dismiss();
	if (Picked && Options.IsValidIndex(Index))
	{
		Picked(Index);
	}
}

void UShinobiDropdownList::Dismiss()
{
	if (DropdownOpenList.Get() == this)
	{
		DropdownOpenList.Reset();
	}
	if (!IsInViewport())
	{
		return;
	}
	RemoveFromParent();
	if (UWidget* Field = Anchor.Get())
	{
		Field->SetKeyboardFocus(); // back where the list came from
	}
}

FReply UShinobiDropdownList::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right || Key == EKeys::BackSpace)
	{
		Dismiss();
		return FReply::Handled();
	}
	// A letter jumps to the next option starting with it (long lists: the roster, frame caps...).
	const FString Name = Key.GetFName().ToString();
	if (!Key.IsGamepadKey() && Name.Len() == 1 && FChar::IsAlnum(Name[0]))
	{
		const int32 From = GetFocusedItem();
		for (int32 Step = 1; Step <= Options.Num(); ++Step)
		{
			const int32 Index = (FMath::Max(From, 0) + Step) % Options.Num();
			if (Options[Index].StartsWith(Name, ESearchCase::IgnoreCase))
			{
				FocusItem(Index);
				break;
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UShinobiDropdownList::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// Options take their own clicks; this is a press on the list's frame or outside it.
	if (!Panel || !Panel->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
	{
		Dismiss();
	}
	return FReply::Handled();
}

FNavigationReply UShinobiDropdownList::NativeOnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event, const FNavigationReply& DefaultReply)
{
	// Up / Down walk the options (clamped); nothing leaves the list while it's open.
	const EUINavigation Direction = Event.GetNavigationType();
	if (Direction == EUINavigation::Up || Direction == EUINavigation::Down)
	{
		const int32 From = GetFocusedItem();
		FocusItem(From == INDEX_NONE ? Selected : FMath::Clamp(From + (Direction == EUINavigation::Up ? -1 : 1), 0, Items.Num() - 1));
	}
	return FNavigationReply::Stop();
}

// --- UShinobiDropdown ---

void UShinobiDropdown::Build(UWidgetTree& Tree, int32 FontSize, float MinWidth)
{
	UHorizontalBox* Line = Tree.ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddChild(Line);
	ValueText = ShinobiMenuStyle::Text(Tree, FText::GetEmpty(), FontSize, FLinearColor::White);
	ValueText->SetMinDesiredWidth(FMath::Max(0.f, MinWidth - 56.f));
	UHorizontalBoxSlot* ValueSlot = Line->AddChildToHorizontalBox(ValueText);
	ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ValueSlot->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* Chevron = Line->AddChildToHorizontalBox(ShinobiMenuStyle::Text(Tree, FText::FromString(TEXT("▾")), FontSize, ShinobiMenuStyle::Accent));
	Chevron->SetVerticalAlignment(VAlign_Center);
	Chevron->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	ShinobiMenuStyle::StyleButton(*this, false, FMargin(14.f, 6.f, 12.f, 6.f));
	Cast<UButtonSlot>(Line->Slot)->SetHorizontalAlignment(HAlign_Fill);
	if (!OnIndexClicked.IsBound())
	{
		OnIndexClicked = FShinobiIndexClicked::CreateUObject(this, &UShinobiDropdown::HandleOpen);
	}
	Arm();
	ShowSelected();
}

void UShinobiDropdown::SetOptions(const TArray<FString>& NewOptions, int32 NewSelected)
{
	Options = NewOptions;
	Selected = Options.IsValidIndex(NewSelected) ? NewSelected : 0;
	ShowSelected();
}

void UShinobiDropdown::HandleOpen(int32 Unused)
{
	OpenList();
}

void UShinobiDropdown::OpenList()
{
	TWeakObjectPtr<UShinobiDropdown> Field = this;
	UShinobiDropdownList::Open(*this, Options, Selected, [Field](int32 Picked)
	{
		if (UShinobiDropdown* Self = Field.Get())
		{
			Self->Choose(Picked);
		}
	});
}

void UShinobiDropdown::Choose(int32 Option)
{
	if (!Options.IsValidIndex(Option))
	{
		return;
	}
	Selected = Option;
	ShowSelected();
	OnPicked.ExecuteIfBound(Option);
}

void UShinobiDropdown::ShowSelected()
{
	if (ValueText)
	{
		ValueText->SetText(FText::FromString(Options.IsValidIndex(Selected) ? Options[Selected] : FString()));
	}
}
