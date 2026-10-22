#include "Shinobi/UI/ShinobiSkillsWidget.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
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
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"

namespace
{
	constexpr float SkillsCellSize = 56.f;
	constexpr int32 SkillsPoolColumns = 6;
	const FLinearColor SkillsBackdropColor(0.02f, 0.02f, 0.04f, 0.85f);
	const FLinearColor SkillsGoldColor(0.91f, 0.79f, 0.48f);
	const FLinearColor SkillsSlottedColor(0.55f, 0.25f, 1.f);
	const FLinearColor SkillsIdleColor(0.12f, 0.12f, 0.14f);
	const FLinearColor SkillsLockedColor(0.75f, 0.25f, 0.2f);
	const FLinearColor SkillsTextColor(0.85f, 0.85f, 0.88f);

	UTextBlock* SkillsMakeLabel(UWidgetTree& Tree, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		return Text;
	}

}

// --- UShinobiSkillCell ---

TSharedRef<SWidget> UShinobiSkillCell::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true); // gamepad: the D-pad walks the cells, A clicks
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Box"));
		Box->SetWidthOverride(SkillsCellSize);
		Box->SetHeightOverride(SkillsCellSize);
		WidgetTree->RootWidget = Box;
		FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FrameBorder->SetPadding(FMargin(3.f));
		FrameBorder->SetBrushColor(SkillsIdleColor);
		Box->AddChild(FrameBorder);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		FrameBorder->SetContent(Layers);
		Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.1f));
		UOverlaySlot* IconSlot = Layers->AddChildToOverlay(Icon);
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
		OverlayText = SkillsMakeLabel(*WidgetTree, 13, FLinearColor::White);
		UOverlaySlot* OverlaySlot = Layers->AddChildToOverlay(OverlayText);
		OverlaySlot->SetHorizontalAlignment(HAlign_Center);
		OverlaySlot->SetVerticalAlignment(VAlign_Center);
		KeyText = SkillsMakeLabel(*WidgetTree, 8, SkillsTextColor);
		UOverlaySlot* KeySlot = Layers->AddChildToOverlay(KeyText);
		KeySlot->SetHorizontalAlignment(HAlign_Center);
		KeySlot->SetVerticalAlignment(VAlign_Bottom);
	}
	return Super::RebuildWidget();
}

void UShinobiSkillCell::Show(TSubclassOf<UShinobiJutsuAbility> Jutsu, const FLinearColor& Frame, const FText& Overlay, const FText& Key, float Dim)
{
	if (!FrameBorder)
	{
		return;
	}
	if (ShownJutsu.Get() != Jutsu.Get())
	{
		ShownJutsu = Jutsu.Get();
		const UShinobiJutsuAbility* Ability = Jutsu ? Jutsu->GetDefaultObject<UShinobiJutsuAbility>() : nullptr;
		UTexture2D* Texture = Ability && !Ability->Icon.IsNull() ? Ability->Icon.LoadSynchronous() : nullptr;
		Icon->SetBrushFromTexture(Texture);
		Icon->SetColorAndOpacity(Texture ? FLinearColor::White : FLinearColor(0.08f, 0.08f, 0.1f)); // an empty slot is a dark square
	}
	FrameColor = Frame;
	FrameBorder->SetBrushColor(bFocused ? FLinearColor::White : FrameColor);
	if (!OverlayText->GetText().EqualTo(Overlay))
	{
		OverlayText->SetText(Overlay);
	}
	if (!KeyText->GetText().EqualTo(Key))
	{
		KeyText->SetText(Key);
	}
	Icon->SetRenderOpacity(Dim);
}

FReply UShinobiSkillCell::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (UShinobiSkillsWidget* Screen = Owner.Get())
		{
			Screen->OnCellRightClicked(*this);
		}
		return FReply::Handled();
	}
	return UWidgetBlueprintLibrary::DetectDragIfPressed(Event, this, EKeys::LeftMouseButton).NativeReply;
}

FReply UShinobiSkillCell::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (UShinobiSkillsWidget* Screen = Owner.Get())
		{
			Screen->OnCellClicked(*this);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(Geometry, Event);
}

void UShinobiSkillCell::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& OutOperation)
{
	if (!ShownJutsu.IsValid())
	{
		return; // nothing to carry
	}
	UDragDropOperation* Operation = NewObject<UDragDropOperation>(this);
	Operation->Payload = this;
	UImage* Visual = NewObject<UImage>(this);
	Visual->SetBrush(Icon->GetBrush());
	Visual->SetDesiredSizeOverride(FVector2D(SkillsCellSize, SkillsCellSize));
	Visual->SetRenderOpacity(0.8f);
	Operation->DefaultDragVisual = Visual;
	Operation->Pivot = EDragPivot::CenterCenter;
	OutOperation = Operation;
}

bool UShinobiSkillCell::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	const UShinobiSkillCell* From = Operation ? Cast<UShinobiSkillCell>(Operation->Payload) : nullptr;
	UShinobiSkillsWidget* Screen = Owner.Get();
	if (!From || !Screen)
	{
		return false;
	}
	Screen->OnCellDropped(*From, *this);
	return true;
}

FReply UShinobiSkillCell::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	UShinobiSkillsWidget* Screen = Owner.Get();
	const FKey Key = Event.GetKey();
	if (Screen && (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Enter || Key == EKeys::SpaceBar))
	{
		Screen->OnCellClicked(*this);
		return FReply::Handled();
	}
	if (Screen && bSlot && Key == EKeys::Gamepad_FaceButton_Top) // Y empties a slot, as right-click does
	{
		Screen->OnCellRightClicked(*this);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UShinobiSkillCell::NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Super::NativeOnMouseEnter(Geometry, Event);
	if (UShinobiSkillsWidget* Screen = Owner.Get(); Screen && ShownJutsu.IsValid())
	{
		Screen->SelectJutsu(ShownJutsu.Get());
	}
}

void UShinobiSkillCell::NativeOnAddedToFocusPath(const FFocusEvent& Event)
{
	Super::NativeOnAddedToFocusPath(Event);
	bFocused = true;
	if (UShinobiSkillsWidget* Screen = Owner.Get(); Screen && ShownJutsu.IsValid())
	{
		Screen->SelectJutsu(ShownJutsu.Get());
	}
	if (FrameBorder)
	{
		FrameBorder->SetBrushColor(FLinearColor::White);
	}
}

void UShinobiSkillCell::NativeOnRemovedFromFocusPath(const FFocusEvent& Event)
{
	Super::NativeOnRemovedFromFocusPath(Event);
	bFocused = false;
	if (FrameBorder)
	{
		FrameBorder->SetBrushColor(FrameColor);
	}
}

// --- UShinobiSkillsWidget ---

AShinobiPlayerCharacter* UShinobiSkillsWidget::GetPlayer() const
{
	return Cast<AShinobiPlayerCharacter>(GetOwningPlayerPawn());
}

TSharedRef<SWidget> UShinobiSkillsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		SetIsFocusable(true); // K, Esc and B reach it when no cell has focus
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		const auto Place = [Root](UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
		{
			UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetAutoSize(true);
			return Slot;
		};
		if (!bMenuMode) // in the main menu the stage shows through
		{
			UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
			Backdrop->SetBrushColor(SkillsBackdropColor);
			UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
			BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			BackdropSlot->SetOffsets(FMargin(0.f));
		}

		// Left: every known jutsu, under its group's heading.
		UBorder* PoolPanel = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(20.f, 6.f, 20.f, 16.f));
		Place(PoolPanel, FAnchors(0.f, 0.f), FVector2D::ZeroVector, FVector2D(40.f, bMenuMode ? 20.f : 40.f));
		UVerticalBox* Pool = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Pool"));
		PoolPanel->SetContent(Pool);
		if (bMenuMode)
		{
			Known = GetDefault<AShinobiPlayerCharacter>()->GetKnownJutsu();
			MenuLoadout = ShinobiLoadout::LoadSaved(Known);
		}
		else
		{
			Pool->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, INVTEXT("JUTSU"), 34, FLinearColor::White, true));
			const AShinobiPlayerCharacter* Player = GetPlayer();
			Known = Player ? Player->GetKnownJutsu() : TArray<TSubclassOf<UShinobiJutsuAbility>>();
		}
		PoolCells.SetNum(Known.Num());
		for (const ShinobiLoadout::EGroup Group : { ShinobiLoadout::EGroup::Combat, ShinobiLoadout::EGroup::Elements, ShinobiLoadout::EGroup::SummonsAndDragons })
		{
			UUniformGridPanel* Grid = nullptr;
			int32 Count = 0;
			for (int32 Index = 0; Index < Known.Num(); ++Index)
			{
				if (ShinobiLoadout::GroupOf(Known[Index]) != Group)
				{
					continue;
				}
				if (!Grid)
				{
					Pool->AddChildToVerticalBox(ShinobiMenuStyle::Text(*WidgetTree, ShinobiLoadout::GroupName(Group), 12, ShinobiMenuStyle::Accent))
						->SetPadding(FMargin(0.f, 14.f, 0.f, 6.f));
					Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
					Grid->SetSlotPadding(FMargin(3.f));
					Pool->AddChildToVerticalBox(Grid);
				}
				UShinobiSkillCell* Cell = WidgetTree->ConstructWidget<UShinobiSkillCell>(UShinobiSkillCell::StaticClass());
				Cell->Owner = this;
				Cell->Index = Index;
				Grid->AddChildToUniformGrid(Cell, Count / SkillsPoolColumns, Count % SkillsPoolColumns);
				PoolCells[Index] = Cell;
				++Count;
			}
		}

		// Right: the jutsu under the cursor or focus.
		UBorder* Detail = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(26.f, 22.f));
		Place(Detail, FAnchors(1.f, 0.42f), FVector2D(1.f, 0.5f), FVector2D(-48.f, 0.f));
		USizeBox* DetailWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DetailWidth->SetWidthOverride(360.f);
		Detail->SetContent(DetailWidth);
		UVerticalBox* DetailColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		DetailWidth->AddChild(DetailColumn);
		DetailGroup = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 12, ShinobiMenuStyle::Accent);
		DetailColumn->AddChildToVerticalBox(DetailGroup);
		DetailName = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 30, FLinearColor::White, true);
		DetailName->SetAutoWrapText(true);
		DetailColumn->AddChildToVerticalBox(DetailName);
		DetailText = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 15, FLinearColor(0.9f, 0.9f, 0.92f));
		DetailText->SetAutoWrapText(true);
		DetailColumn->AddChildToVerticalBox(DetailText)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		DetailStats = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 14, ShinobiMenuStyle::TextDim);
		DetailColumn->AddChildToVerticalBox(DetailStats)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));

		// Bottom: the 8 slots with their keys, the hint or lock banner under them.
		UVerticalBox* Bottom = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Place(Bottom, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, bMenuMode ? -20.f : -110.f)); // in game: above the HUD bar
		UBorder* Bar = ShinobiMenuStyle::PanelBox(*WidgetTree, FMargin(12.f, 10.f));
		Bottom->AddChildToVerticalBox(Bar)->SetHorizontalAlignment(HAlign_Center);
		UHorizontalBox* Slots = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Bar->SetContent(Slots);
		for (int32 Index = 0; Index < ShinobiLoadout::NumSlots; ++Index)
		{
			UShinobiSkillCell* Cell = WidgetTree->ConstructWidget<UShinobiSkillCell>(UShinobiSkillCell::StaticClass());
			Cell->Owner = this;
			Cell->bSlot = true;
			Cell->Index = Index;
			Slots->AddChildToHorizontalBox(Cell)->SetPadding(FMargin(Index == 4 ? 18.f : 4.f, 0.f, 4.f, 0.f));
			SlotCells.Add(Cell);
		}
		Banner = ShinobiMenuStyle::Text(*WidgetTree, FText::GetEmpty(), 13, SkillsGoldColor);
		Banner->SetJustification(ETextJustify::Center);
		Bottom->AddChildToVerticalBox(Banner)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	SelectJutsu(Known.Num() > 0 ? Known[0] : nullptr);
	RefreshCells();
	return Result;
}

void UShinobiSkillsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bMenuMode) // the main menu focuses it when its tab opens
	{
		GetFocusTarget()->SetKeyboardFocus();
	}
}

UWidget* UShinobiSkillsWidget::GetFocusTarget() const
{
	return PoolCells.Num() > 0 ? static_cast<UWidget*>(PoolCells[0]) : const_cast<UShinobiSkillsWidget*>(this);
}

UShinobiSkillCell* UShinobiSkillsWidget::GetCell(bool bSlot, int32 Index) const
{
	const TArray<TObjectPtr<UShinobiSkillCell>>& Cells = bSlot ? SlotCells : PoolCells;
	return Cells.IsValidIndex(Index) ? Cells[Index].Get() : nullptr;
}

const ShinobiLoadout::FSlots& UShinobiSkillsWidget::GetShownLoadout() const
{
	static const ShinobiLoadout::FSlots Empty;
	if (bMenuMode)
	{
		return MenuLoadout;
	}
	const AShinobiPlayerCharacter* Player = GetPlayer();
	return Player ? Player->GetLoadout() : Empty;
}

void UShinobiSkillsWidget::SelectJutsu(TSubclassOf<UShinobiJutsuAbility> Jutsu)
{
	const UShinobiJutsuAbility* Ability = Jutsu ? Jutsu->GetDefaultObject<UShinobiJutsuAbility>() : nullptr;
	if (!DetailName || !Ability)
	{
		return;
	}
	DetailGroup->SetText(ShinobiLoadout::GroupName(ShinobiLoadout::GroupOf(Jutsu)));
	DetailName->SetText(Ability->DisplayName.ToUpper());
	DetailText->SetText(Ability->Description);
	DetailStats->SetText(FText::FromString(FString::Printf(TEXT("Chakra  %d\nCooldown  %d s\nSeal time  %.1f s"),
		FMath::RoundToInt(Ability->ChakraCost), FMath::RoundToInt(Ability->Cooldown), Ability->WindUp)));
}

void UShinobiSkillsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!bMenuMode && !GetPlayer())
	{
		Close(); // died, or the pawn went away
		return;
	}
	RefreshCells();
}

void UShinobiSkillsWidget::RefreshCells()
{
	AShinobiPlayerCharacter* Player = bMenuMode ? nullptr : GetPlayer();
	const ShinobiLoadout::FSlots& Loadout = GetShownLoadout();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const FText Reason = Player ? Player->GetLoadoutLockReason() : FText::GetEmpty();
	FText BannerText;
	if (Now < NoticeUntil)
	{
		BannerText = Notice;
	}
	else if (Player && Player->IsInCombat())
	{
		BannerText = FText::FromString(FString::Printf(TEXT("In combat: swaps unlock in %ds"), FMath::CeilToInt(Player->GetCombatSecondsLeft())));
	}
	else if (!Reason.IsEmpty())
	{
		BannerText = FText::FromString(FString::Printf(TEXT("Swaps locked: %s"), *Reason.ToString()));
	}
	else
	{
		BannerText = INVTEXT("Click a jutsu, then a slot (or drag it)  |  drag slots to reorder  |  right-click or Y empties a slot");
	}
	if (Banner && !Banner->GetText().EqualTo(BannerText))
	{
		Banner->SetText(BannerText);
	}

	for (int32 Index = 0; Index < PoolCells.Num(); ++Index)
	{
		const bool bSlotted = Loadout.Contains(Known[Index]);
		PoolCells[Index]->Show(Known[Index], Index == PickedPool ? SkillsGoldColor : (bSlotted ? ShinobiMenuStyle::Accent : SkillsIdleColor), FText::GetEmpty(), FText::GetEmpty(), 1.f);
	}
	const AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(GetOwningPlayer());
	const bool bPad = PC && PC->IsUsingGamepad();
	static const TCHAR* FaceLabels[] = { TEXT("A"), TEXT("B"), TEXT("X"), TEXT("Y") };
	for (int32 Index = 0; Index < SlotCells.Num(); ++Index)
	{
		const TSubclassOf<UShinobiJutsuAbility> Jutsu = Loadout.IsValidIndex(Index) ? Loadout[Index] : nullptr;
		const bool bLocked = Player && Player->IsSlotLocked(Index);
		FText Overlay;
		if (bLocked && Jutsu)
		{
			const FShinobiJutsuSlotState State = Jutsu->GetDefaultObject<UShinobiJutsuAbility>()->GetSlotState(*Player->GetAbilitySystemComponent());
			Overlay = State.CooldownRemaining > 0.f ? FText::FromString(FString::Printf(TEXT("%ds"), FMath::CeilToInt(State.CooldownRemaining))) : INVTEXT("IN USE");
		}
		const FText Key = bPad ? FText::FromString(FString::Printf(TEXT("%s%s"), Index < 4 ? TEXT("") : TEXT("2-"), FaceLabels[Index % 4]))
			: ShinobiKeyBindings::KeyLabel(ShinobiKeyBindings::Get(static_cast<ShinobiKeyBindings::EBindable>(Index)), true);
		SlotCells[Index]->Show(Jutsu, Index == PickedSlot ? SkillsGoldColor : (bLocked ? SkillsLockedColor : SkillsIdleColor), Overlay, Key, bLocked ? 0.45f : 1.f);
	}
}

void UShinobiSkillsWidget::Request(const ShinobiLoadout::FSlots& Slots)
{
	PickedPool = PickedSlot = INDEX_NONE;
	if (bMenuMode)
	{
		// No pawn, no combat: the bar is just saved, and the next game starts with it (LoadSaved on spawn).
		if (Slots != MenuLoadout && ShinobiLoadout::IsValid(Slots, Known))
		{
			MenuLoadout = Slots;
			ShinobiLoadout::Save(Slots);
		}
		RefreshCells();
		return;
	}
	AShinobiPlayerCharacter* Player = GetPlayer();
	if (!Player || Slots == Player->GetLoadout())
	{
		return;
	}
	const FText Reason = Player->GetLoadoutLockReason();
	const bool bSlotLocked = !ShinobiLoadout::CanChange(Player->GetLoadout(), Slots, [Player](int32 SlotIndex) { return Player->IsSlotLocked(SlotIndex); });
	if (Reason.IsEmpty() && !bSlotLocked)
	{
		Player->RequestLoadout(Slots); // the server checks again and has the last word
		return;
	}
	Notice = !Reason.IsEmpty() ? FText::FromString(FString::Printf(TEXT("Can't swap now: %s"), *Reason.ToString()))
		: INVTEXT("That jutsu is still cooling down or in use");
	NoticeUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + 2.0;
}

void UShinobiSkillsWidget::OnCellClicked(const UShinobiSkillCell& Cell)
{
	if (!bMenuMode && !GetPlayer())
	{
		return;
	}
	const ShinobiLoadout::FSlots Loadout = GetShownLoadout();
	if (!Cell.bSlot && Known.IsValidIndex(Cell.Index))
	{
		SelectJutsu(Known[Cell.Index]);
	}
	if (!Cell.bSlot)
	{
		if (PickedSlot != INDEX_NONE)
		{
			Request(ShinobiLoadout::Place(Loadout, Known[Cell.Index], PickedSlot));
			return;
		}
		PickedPool = PickedPool == Cell.Index ? INDEX_NONE : Cell.Index;
		RefreshCells();
		return;
	}
	if (PickedPool != INDEX_NONE)
	{
		Request(ShinobiLoadout::Place(Loadout, Known[PickedPool], Cell.Index));
		return;
	}
	if (PickedSlot != INDEX_NONE)
	{
		const int32 From = PickedSlot;
		PickedSlot = INDEX_NONE;
		if (From != Cell.Index)
		{
			Request(ShinobiLoadout::Swap(Loadout, From, Cell.Index));
		}
		return;
	}
	PickedSlot = Cell.Index;
	RefreshCells();
}

void UShinobiSkillsWidget::OnCellRightClicked(const UShinobiSkillCell& Cell)
{
	if ((bMenuMode || GetPlayer()) && Cell.bSlot)
	{
		Request(ShinobiLoadout::Clear(GetShownLoadout(), Cell.Index));
	}
}

void UShinobiSkillsWidget::OnCellDropped(const UShinobiSkillCell& From, const UShinobiSkillCell& To)
{
	if ((!bMenuMode && !GetPlayer()) || &From == &To)
	{
		return;
	}
	const ShinobiLoadout::FSlots Loadout = GetShownLoadout();
	if (!From.bSlot && To.bSlot)
	{
		Request(ShinobiLoadout::Place(Loadout, Known[From.Index], To.Index));
	}
	else if (From.bSlot && To.bSlot)
	{
		Request(ShinobiLoadout::Swap(Loadout, From.Index, To.Index));
	}
	else if (From.bSlot && !To.bSlot)
	{
		Request(ShinobiLoadout::Clear(Loadout, From.Index)); // back to the pool
	}
}

FReply UShinobiSkillsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	const bool bCancel = Key == EKeys::Gamepad_FaceButton_Right;
	if (bCancel && (PickedPool != INDEX_NONE || PickedSlot != INDEX_NONE))
	{
		PickedPool = PickedSlot = INDEX_NONE; // B first drops the pick
		return FReply::Handled();
	}
	if (bMenuMode)
	{
		return Super::NativeOnKeyDown(Geometry, Event); // the main menu's Esc / B / tabs
	}
	if (bCancel || Key == EKeys::Escape || Key == ShinobiKeyBindings::Get(ShinobiKeyBindings::EBindable::SkillsScreen).Key || Key == EKeys::Gamepad_Special_Right)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UShinobiSkillsWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	PickedPool = PickedSlot = INDEX_NONE; // a click on the backdrop drops the pick
	return FReply::Handled();
}

void UShinobiSkillsWidget::Close()
{
	if (AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(GetOwningPlayer()))
	{
		PC->ToggleSkillsScreen();
	}
}
