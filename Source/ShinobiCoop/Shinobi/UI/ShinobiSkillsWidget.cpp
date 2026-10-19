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

namespace
{
	constexpr float SkillsCellSize = 56.f;
	constexpr int32 SkillsPoolColumns = 7;
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

	FText SkillsTooltipFor(const UShinobiJutsuAbility& Jutsu)
	{
		return FText::FromString(FString::Printf(TEXT("%s\nChakra %d  ·  Cooldown %ds\n%s"), *Jutsu.DisplayName.ToString(),
			FMath::RoundToInt(Jutsu.ChakraCost), FMath::RoundToInt(Jutsu.Cooldown), *Jutsu.Description.ToString()));
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
		SetToolTipText(Ability ? SkillsTooltipFor(*Ability) : FText::GetEmpty());
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

void UShinobiSkillCell::NativeOnAddedToFocusPath(const FFocusEvent& Event)
{
	Super::NativeOnAddedToFocusPath(Event);
	bFocused = true;
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
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrushColor(SkillsBackdropColor);
		Backdrop->SetHorizontalAlignment(HAlign_Center);
		Backdrop->SetVerticalAlignment(VAlign_Center);
		UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
		BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackdropSlot->SetOffsets(FMargin(0.f));

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Backdrop->SetContent(Column);
		const auto AddRow = [Column](UWidget* Widget, float Top)
		{
			UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Widget);
			RowSlot->SetHorizontalAlignment(HAlign_Center);
			RowSlot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		};
		UTextBlock* Title = SkillsMakeLabel(*WidgetTree, 26, SkillsGoldColor);
		Title->SetText(INVTEXT("J U T S U"));
		AddRow(Title, 0.f);
		UTextBlock* Hint = SkillsMakeLabel(*WidgetTree, 11, SkillsTextColor);
		Hint->SetText(INVTEXT("Hover a jutsu for details  ·  K or Esc closes"));
		AddRow(Hint, 4.f);

		UUniformGridPanel* Pool = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		Pool->SetSlotPadding(FMargin(4.f));
		AddRow(Pool, 16.f);
		const AShinobiPlayerCharacter* Player = GetPlayer();
		Known = Player ? Player->GetKnownJutsu() : TArray<TSubclassOf<UShinobiJutsuAbility>>();
		for (int32 Index = 0; Index < Known.Num(); ++Index)
		{
			UShinobiSkillCell* Cell = WidgetTree->ConstructWidget<UShinobiSkillCell>(UShinobiSkillCell::StaticClass());
			Cell->Owner = this;
			Cell->Index = Index;
			Pool->AddChildToUniformGrid(Cell, Index / SkillsPoolColumns, Index % SkillsPoolColumns);
			PoolCells.Add(Cell);
		}

		USizeBox* Rule = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Rule->SetWidthOverride(SkillsPoolColumns * (SkillsCellSize + 8.f));
		Rule->SetHeightOverride(1.f);
		UBorder* RuleLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		RuleLine->SetBrushColor(SkillsGoldColor * FLinearColor(1.f, 1.f, 1.f, 0.6f));
		Rule->AddChild(RuleLine);
		AddRow(Rule, 18.f);
		UTextBlock* SlotsLabel = SkillsMakeLabel(*WidgetTree, 11, SkillsTextColor);
		SlotsLabel->SetText(INVTEXT("Your 8 slots   (gamepad: RB page 1 | page 2)"));
		AddRow(SlotsLabel, 10.f);

		UHorizontalBox* Slots = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddRow(Slots, 6.f);
		for (int32 Index = 0; Index < ShinobiLoadout::NumSlots; ++Index)
		{
			UShinobiSkillCell* Cell = WidgetTree->ConstructWidget<UShinobiSkillCell>(UShinobiSkillCell::StaticClass());
			Cell->Owner = this;
			Cell->bSlot = true;
			Cell->Index = Index;
			Slots->AddChildToHorizontalBox(Cell)->SetPadding(FMargin(Index == 4 ? 18.f : 4.f, 0.f, 4.f, 0.f));
			SlotCells.Add(Cell);
		}
		Banner = SkillsMakeLabel(*WidgetTree, 13, SkillsGoldColor);
		AddRow(Banner, 16.f);
	}
	return Super::RebuildWidget();
}

void UShinobiSkillsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UWidget* First = PoolCells.Num() > 0 ? static_cast<UWidget*>(PoolCells[0]) : this;
	First->SetKeyboardFocus();
}

void UShinobiSkillsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	AShinobiPlayerCharacter* Player = GetPlayer();
	if (!Player)
	{
		Close(); // died, or the pawn went away
		return;
	}
	const UAbilitySystemComponent& ASC = *Player->GetAbilitySystemComponent();
	const ShinobiLoadout::FSlots& Loadout = Player->GetLoadout();
	const FText Reason = Player->GetLoadoutLockReason();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	FText BannerText;
	if (Now < NoticeUntil)
	{
		BannerText = Notice;
	}
	else if (Player->IsInCombat())
	{
		BannerText = FText::FromString(FString::Printf(TEXT("In combat: swaps unlock in %ds"), FMath::CeilToInt(Player->GetCombatSecondsLeft())));
	}
	else if (!Reason.IsEmpty())
	{
		BannerText = FText::FromString(FString::Printf(TEXT("Swaps locked: %s"), *Reason.ToString()));
	}
	else
	{
		BannerText = INVTEXT("Click a jutsu, then a slot (or drag it)  ·  drag slots to reorder  ·  right-click or Y empties a slot");
	}
	if (!Banner->GetText().EqualTo(BannerText))
	{
		Banner->SetText(BannerText);
	}

	for (int32 Index = 0; Index < PoolCells.Num(); ++Index)
	{
		const bool bSlotted = Loadout.Contains(Known[Index]);
		PoolCells[Index]->Show(Known[Index], Index == PickedPool ? SkillsGoldColor : (bSlotted ? SkillsSlottedColor : SkillsIdleColor), FText::GetEmpty(), FText::GetEmpty(), 1.f);
	}
	const AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(GetOwningPlayer());
	const bool bPad = PC && PC->IsUsingGamepad();
	static const TCHAR* FaceLabels[] = { TEXT("A"), TEXT("B"), TEXT("X"), TEXT("Y") };
	for (int32 Index = 0; Index < SlotCells.Num(); ++Index)
	{
		const TSubclassOf<UShinobiJutsuAbility> Jutsu = Loadout.IsValidIndex(Index) ? Loadout[Index] : nullptr;
		const bool bLocked = Player->IsSlotLocked(Index);
		FText Overlay;
		if (bLocked)
		{
			const FShinobiJutsuSlotState State = Jutsu->GetDefaultObject<UShinobiJutsuAbility>()->GetSlotState(ASC);
			Overlay = State.CooldownRemaining > 0.f ? FText::FromString(FString::Printf(TEXT("%ds"), FMath::CeilToInt(State.CooldownRemaining))) : INVTEXT("IN USE");
		}
		const FText Key = bPad ? FText::FromString(FString::Printf(TEXT("%s%s"), Index < 4 ? TEXT("") : TEXT("2·"), FaceLabels[Index % 4]))
			: ShinobiKeyBindings::KeyLabel(ShinobiKeyBindings::Get(static_cast<ShinobiKeyBindings::EBindable>(Index)), true);
		SlotCells[Index]->Show(Jutsu, Index == PickedSlot ? SkillsGoldColor : (bLocked ? SkillsLockedColor : SkillsIdleColor), Overlay, Key, bLocked ? 0.45f : 1.f);
	}
}

void UShinobiSkillsWidget::Request(const ShinobiLoadout::FSlots& Slots)
{
	PickedPool = PickedSlot = INDEX_NONE;
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
	const AShinobiPlayerCharacter* Player = GetPlayer();
	if (!Player)
	{
		return;
	}
	const ShinobiLoadout::FSlots& Loadout = Player->GetLoadout();
	if (!Cell.bSlot)
	{
		if (PickedSlot != INDEX_NONE)
		{
			Request(ShinobiLoadout::Place(Loadout, Known[Cell.Index], PickedSlot));
			return;
		}
		PickedPool = PickedPool == Cell.Index ? INDEX_NONE : Cell.Index;
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
}

void UShinobiSkillsWidget::OnCellRightClicked(const UShinobiSkillCell& Cell)
{
	if (const AShinobiPlayerCharacter* Player = GetPlayer(); Player && Cell.bSlot)
	{
		Request(ShinobiLoadout::Clear(Player->GetLoadout(), Cell.Index));
	}
}

void UShinobiSkillsWidget::OnCellDropped(const UShinobiSkillCell& From, const UShinobiSkillCell& To)
{
	const AShinobiPlayerCharacter* Player = GetPlayer();
	if (!Player || &From == &To)
	{
		return;
	}
	const ShinobiLoadout::FSlots& Loadout = Player->GetLoadout();
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
