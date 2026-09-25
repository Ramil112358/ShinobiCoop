#include "Shinobi/UI/ShinobiHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"

namespace
{
	const FLinearColor HealthColor(0.2f, 0.85f, 0.3f);
	const FLinearColor LowHealthColor(0.9f, 0.15f, 0.1f);
	const FLinearColor ChakraColor(0.2f, 0.5f, 1.f);
	const FLinearColor StaminaColor(1.f, 0.8f, 0.2f);
}

TSharedRef<SWidget> UShinobiHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		// Bars are read-only; never eat cursor/click input meant for the game or other widgets.
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
	}
	return Super::RebuildWidget();
}

UProgressBar* UShinobiHUDWidget::AddBar(UVerticalBox* Box, FName Name, const FLinearColor& Color, float Height)
{
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetHeightOverride(Height);
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
	Bar->SetFillColorAndOpacity(Color);
	SizeBox->AddChild(Bar);
	UVerticalBoxSlot* BarSlot = Box->AddChildToVerticalBox(SizeBox);
	BarSlot->SetPadding(FMargin(0.f, 2.f));
	return Bar;
}

void UShinobiHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const AShinobiCharacterBase* Pawn = Cast<AShinobiCharacterBase>(GetOwningPlayerPawn());
	const UShinobiAttributeSet* Stats = Pawn ? Pawn->GetAttributes() : nullptr;
	if (!Stats || !HealthBar)
	{
		return;
	}
	const float HealthRatio = Stats->GetHealth() / FMath::Max(Stats->GetMaxHealth(), 1.f);
	HealthBar->SetPercent(HealthRatio);
	HealthBar->SetFillColorAndOpacity(HealthRatio < 0.3f ? LowHealthColor : HealthColor);
	ChakraBar->SetPercent(Stats->GetChakra() / FMath::Max(Stats->GetMaxChakra(), 1.f));
	StaminaBar->SetPercent(Stats->GetStamina() / FMath::Max(Stats->GetMaxStamina(), 1.f));
}
