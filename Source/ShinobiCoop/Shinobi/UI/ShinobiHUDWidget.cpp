#include "Shinobi/UI/ShinobiHUDWidget.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"

namespace
{
	const FLinearColor HealthColor(0.2f, 0.85f, 0.3f);
	const FLinearColor LowHealthColor(0.9f, 0.15f, 0.1f);
	const FLinearColor ChakraColor(0.2f, 0.5f, 1.f);
	const FLinearColor StaminaColor(1.f, 0.8f, 0.2f);
	const FLinearColor PipOnColor(0.85f, 0.6f, 0.3f); // a wooden log
	const FLinearColor PipOffColor(0.15f, 0.15f, 0.15f, 0.6f);
	const FLinearColor SlotColor(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor SlotFlashColor(0.8f, 0.05f, 0.05f, 0.85f);
	const FLinearColor ShadeColor(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor KeyColor(0.8f, 0.8f, 0.8f);
	const FLinearColor CostColor(0.45f, 0.7f, 1.f);
	constexpr float SlotSize = 76.f;
	constexpr double FlashSeconds = 0.3;
	constexpr int32 NumPips = 3; // MaxSubstitutionCharges

	int32 JutsuSlotIndex(EShinobiAbilityInputID InputID)
	{
		return static_cast<int32>(InputID) - static_cast<int32>(EShinobiAbilityInputID::Jutsu1);
	}
}

TSharedRef<SWidget> UShinobiHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		// Read-only HUD; never eat cursor/click input meant for the game or other widgets.
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);

		UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Bars"));
		UCanvasPanelSlot* BarsSlot = Root->AddChildToCanvas(Bars);
		BarsSlot->SetAnchors(FAnchors(0.f, 1.f));
		BarsSlot->SetAlignment(FVector2D(0.f, 1.f));
		BarsSlot->SetPosition(FVector2D(40.f, -40.f));
		BarsSlot->SetAutoSize(true);

		HealthBar = AddBar(Bars, TEXT("HealthBar"), HealthColor, 22.f);
		ChakraBar = AddBar(Bars, TEXT("ChakraBar"), ChakraColor, 16.f);
		StaminaBar = AddBar(Bars, TEXT("StaminaBar"), StaminaColor, 10.f);
		BuildSubstitutionPips(Bars);
		BuildJutsuSlots(Root);
	}
	return Super::RebuildWidget();
}

UProgressBar* UShinobiHUDWidget::AddBar(UVerticalBox* Box, FName Name, const FLinearColor& Color, float Height)
{
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	// Without an explicit width, SProgressBar::ComputeDesiredSize falls back to the 20x12 marquee image
	// (the runtime UMG style's Common/ProgressBar_Marquee brush, set up in UMGCoreStyle.cpp), so the
	// auto-sized canvas slot above would otherwise collapse to ~20 px wide.
	SizeBox->SetWidthOverride(380.f);
	SizeBox->SetHeightOverride(Height);
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
	Bar->SetFillColorAndOpacity(Color);
	SizeBox->AddChild(Bar);
	UVerticalBoxSlot* BarSlot = Box->AddChildToVerticalBox(SizeBox);
	BarSlot->SetPadding(FMargin(0.f, 2.f));
	return Bar;
}

void UShinobiHUDWidget::BuildSubstitutionPips(UVerticalBox* Bars)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SubstitutionPips"));
	Bars->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	for (int32 Index = 0; Index < NumPips; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(16.f);
		Box->SetHeightOverride(16.f);
		UBorder* Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Pip->SetBrushColor(PipOffColor);
		Box->AddChild(Pip);
		Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		SubstitutionPips.Add(Pip);
	}
}

void UShinobiHUDWidget::BuildJutsuSlots(UCanvasPanel* Root)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("JutsuSlots"));
	UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
	RowSlot->SetAnchors(FAnchors(0.5f, 1.f));
	RowSlot->SetAlignment(FVector2D(0.5f, 1.f));
	RowSlot->SetPosition(FVector2D(0.f, -40.f));
	RowSlot->SetAutoSize(true);

	static const TCHAR* KeyLabels[NumJutsuSlots] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4") };
	for (int32 Index = 0; Index < NumJutsuSlots; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(SlotSize);
		Box->SetHeightOverride(SlotSize);
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrushColor(SlotColor);
		Frame->SetPadding(FMargin(0.f));
		Box->AddChild(Frame);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Frame->SetContent(Layers);

		// Back to front: glyph, cooldown shade, cooldown seconds, key, cost.
		UTextBlock* Glyph = AddLabel(Layers, 34, HAlign_Center, VAlign_Center, FLinearColor::White);
		USizeBox* Shade = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Shade->SetHeightOverride(0.f);
		UBorder* ShadeFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ShadeFill->SetBrushColor(ShadeColor);
		Shade->AddChild(ShadeFill);
		UOverlaySlot* ShadeSlot = Layers->AddChildToOverlay(Shade);
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Bottom);
		UTextBlock* CooldownText = AddLabel(Layers, 22, HAlign_Center, VAlign_Center, FLinearColor::White);
		AddLabel(Layers, 12, HAlign_Left, VAlign_Top, KeyColor)->SetText(FText::FromString(KeyLabels[Index]));
		UTextBlock* CostText = AddLabel(Layers, 12, HAlign_Right, VAlign_Bottom, CostColor);

		Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(4.f, 0.f));
		SlotFrames.Add(Frame);
		SlotGlyphs.Add(Glyph);
		SlotShades.Add(Shade);
		SlotCooldownTexts.Add(CooldownText);
		SlotCosts.Add(CostText);
	}
	SlotFlashUntil.Init(0.0, NumJutsuSlots);
}

UTextBlock* UShinobiHUDWidget::AddLabel(UOverlay* Layers, int32 FontSize, EHorizontalAlignment HorizontalAlign, EVerticalAlignment VerticalAlign, const FLinearColor& Tint)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo LabelFont = Label->GetFont();
	LabelFont.Size = FontSize;
	Label->SetFont(LabelFont);
	Label->SetColorAndOpacity(FSlateColor(Tint));
	UOverlaySlot* LabelSlot = Layers->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HorizontalAlign);
	LabelSlot->SetVerticalAlignment(VerticalAlign);
	LabelSlot->SetPadding(FMargin(4.f, 2.f));
	return Label;
}

void UShinobiHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const AShinobiCharacterBase* Pawn = Cast<AShinobiCharacterBase>(GetOwningPlayerPawn());
	const UShinobiAttributeSet* Stats = Pawn ? Pawn->GetAttributes() : nullptr;
	UAbilitySystemComponent* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
	BindFailureFlash(ASC);
	if (!Stats || !ASC || !HealthBar)
	{
		return;
	}
	const float HealthRatio = Stats->GetHealth() / FMath::Max(Stats->GetMaxHealth(), 1.f);
	HealthBar->SetPercent(HealthRatio);
	HealthBar->SetFillColorAndOpacity(HealthRatio < 0.3f ? LowHealthColor : HealthColor);
	ChakraBar->SetPercent(Stats->GetChakra() / FMath::Max(Stats->GetMaxChakra(), 1.f));
	StaminaBar->SetPercent(Stats->GetStamina() / FMath::Max(Stats->GetMaxStamina(), 1.f));

	const int32 Charges = FMath::RoundToInt(Stats->GetSubstitutionCharges());
	for (int32 Index = 0; Index < SubstitutionPips.Num(); ++Index)
	{
		SubstitutionPips[Index]->SetBrushColor(Index < Charges ? PipOnColor : PipOffColor);
	}
	UpdateJutsuSlots(*ASC);
}

void UShinobiHUDWidget::UpdateJutsuSlots(const UAbilitySystemComponent& ASC)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// ponytail: per-frame poll of the granted abilities, like the bars; fine for a handful of specs
	for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
	{
		const UShinobiJutsuAbility* Jutsu = Cast<UShinobiJutsuAbility>(Spec.Ability);
		const int32 Index = Jutsu ? JutsuSlotIndex(Jutsu->InputID) : INDEX_NONE;
		if (!SlotFrames.IsValidIndex(Index))
		{
			continue;
		}
		const FShinobiJutsuSlotState State = Jutsu->GetSlotState(ASC);
		const bool bCooling = State.CooldownRemaining > 0.f && State.CooldownDuration > 0.f;
		SlotGlyphs[Index]->SetText(Jutsu->Glyph);
		SlotGlyphs[Index]->SetRenderOpacity(State.bAffordable ? 1.f : 0.35f);
		SlotCosts[Index]->SetText(FText::AsNumber(FMath::RoundToInt(Jutsu->ChakraCost)));
		SlotCooldownTexts[Index]->SetText(bCooling ? FText::AsNumber(FMath::CeilToInt(State.CooldownRemaining)) : FText::GetEmpty());
		SlotShades[Index]->SetHeightOverride(bCooling ? SlotSize * State.CooldownRemaining / State.CooldownDuration : 0.f);
		SlotFrames[Index]->SetBrushColor(Now < SlotFlashUntil[Index] ? SlotFlashColor : SlotColor);
	}
}

void UShinobiHUDWidget::BindFailureFlash(UAbilitySystemComponent* ASC)
{
	if (BoundASC == ASC)
	{
		return;
	}
	if (UAbilitySystemComponent* Old = BoundASC.Get())
	{
		Old->AbilityFailedCallbacks.Remove(FailedHandle);
	}
	BoundASC = ASC;
	FailedHandle = ASC ? ASC->AbilityFailedCallbacks.AddUObject(this, &UShinobiHUDWidget::OnAbilityFailed) : FDelegateHandle();
}

void UShinobiHUDWidget::OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Reasons)
{
	const UShinobiJutsuAbility* Jutsu = Cast<UShinobiJutsuAbility>(Ability);
	const int32 Index = Jutsu ? JutsuSlotIndex(Jutsu->InputID) : INDEX_NONE;
	if (SlotFlashUntil.IsValidIndex(Index) && GetWorld())
	{
		SlotFlashUntil[Index] = GetWorld()->GetTimeSeconds() + FlashSeconds;
	}
}

void UShinobiHUDWidget::NativeDestruct()
{
	BindFailureFlash(nullptr);
	Super::NativeDestruct();
}
