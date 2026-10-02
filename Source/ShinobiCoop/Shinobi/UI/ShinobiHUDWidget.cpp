#include "Shinobi/UI/ShinobiHUDWidget.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/GA_Revive.h"
#include "Shinobi/VFX/ShinobiSfx.h"
#include "Shinobi/World/ShinobiCamp.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"

namespace
{
	const FLinearColor HealthColor(0.72f, 0.06f, 0.06f); // tints the grey fill of the SpriteCook bar
	const FLinearColor LowHealthColor(1.f, 0.3f, 0.05f);
	const FLinearColor TrailColor(1.f, 0.85f, 0.6f, 0.9f);
	const FLinearColor ChakraColor(0.04f, 0.3f, 1.f);
	const FLinearColor StaminaColor(1.f, 0.6f, 0.05f);
	const FLinearColor PipOnColor(0.85f, 0.6f, 0.3f); // a wooden log
	const FLinearColor PipOffColor(0.15f, 0.15f, 0.15f, 0.6f);
	// Storm-style frame: dark ink plates with thin gold trim.
	const FLinearColor InkColor(0.02f, 0.02f, 0.03f, 0.85f);
	const FLinearColor BarBackColor(0.04f, 0.04f, 0.05f, 0.9f);
	const FLinearColor TrimColor(0.85f, 0.65f, 0.3f);
	const FLinearColor PortraitBackColor(0.3f, 0.04f, 0.03f);
	const FLinearColor PortraitRimColor(0.55f, 0.08f, 0.05f);
	const FLinearColor NameColor(1.f, 0.95f, 0.85f);
	const FLinearColor SlotColor(0.02f, 0.02f, 0.03f, 0.92f);
	const FLinearColor SlotFlashColor(0.95f, 0.1f, 0.05f);
	const FLinearColor ShadeColor(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor DimIconColor(0.3f, 0.3f, 0.35f);
	const FLinearColor KeyColor(0.9f, 0.9f, 0.9f);
	const FLinearColor CostColor(0.45f, 0.7f, 1.f);
	const FLinearColor NoChakraColor(1.f, 0.3f, 0.25f);
	constexpr float SlotSize = 88.f;
	constexpr float SlotBorder = 10.f; // the ornate frame art
	constexpr float PortraitSize = 124.f;
	constexpr float PipSize = 22.f;
	constexpr float StatusSize = 30.f;
	constexpr double FlashSeconds = 0.3;
	constexpr double TrailHoldSeconds = 0.5;
	constexpr float TrailDrainPerSecond = 0.6f;
	constexpr int32 NumPips = 3; // MaxSubstitutionCharges
	constexpr float CompassHalfWidth = 220.f;
	// Scripts/setup_hud.py imports SourceArt/UI/portrait.png here.
	const TCHAR* PortraitPath = TEXT("/Game/Shinobi/UI/T_Portrait.T_Portrait");

	/** A SpriteCook HUD texture (Scripts/setup_sprite_hud.py), or null while it hasn't been imported. */
	UTexture2D* HudTexture(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/Shinobi/UI/Hud/T_Hud_%s.T_Hud_%s"), Name, Name);
		return LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn);
	}

	FSlateBrush ArtBrush(UTexture2D* Texture, const FVector2D& Size, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = Tint;
		Brush.ImageSize = Size;
		Brush.SetResourceObject(Texture);
		return Brush;
	}

	int32 JutsuSlotIndex(EShinobiAbilityInputID InputID)
	{
		return static_cast<int32>(InputID) - static_cast<int32>(EShinobiAbilityInputID::Jutsu1);
	}

	/** A rounded box (Radius < 0: fully round ends, a circle when square), optionally outlined and textured. */
	FSlateBrush RoundedBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f, UTexture2D* Image = nullptr)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Fill;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FMath::Max(Radius, 0.f), Outline, OutlineWidth);
		if (Radius < 0.f)
		{
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		}
		Brush.SetResourceObject(Image);
		return Brush;
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

		BuildUnitFrame(Root);
		BuildJutsuSlots(Root);

		WaveText = AddCanvasText(Root, TEXT("WaveText"), FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 24.f), 22, ETextJustify::Center);
		RootCanvas = Root;
		CompassBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompassBar"));
		if (UTexture2D* CompassArt = HudTexture(TEXT("Compass")))
		{
			CompassBar->SetBrush(ArtBrush(CompassArt, FVector2D(520.f, 32.f), FLinearColor(1.f, 1.f, 1.f, 0.9f)));
		}
		else
		{
			CompassBar->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
		}
		UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(CompassBar);
		BarSlot->SetAnchors(FAnchors(0.5f, 0.f));
		BarSlot->SetAlignment(FVector2D(0.5f, 0.f));
		BarSlot->SetPosition(FVector2D(0.f, 58.f));
		BarSlot->SetSize(FVector2D(CompassHalfWidth * 2.f + 40.f, 32.f));
		CompassBar->SetVisibility(ESlateVisibility::Collapsed);
		TeamText = AddCanvasText(Root, TEXT("TeamText"), FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(40.f, 24.f), 14, ETextJustify::Left);
		BannerText = AddCanvasText(Root, TEXT("BannerText"), FAnchors(0.5f, 0.35f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, 40, ETextJustify::Center);
		HintText = AddCanvasText(Root, TEXT("HintText"), FAnchors(0.5f, 0.6f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, 18, ETextJustify::Center);
	}
	return Super::RebuildWidget();
}

void UShinobiHUDWidget::BuildUnitFrame(UCanvasPanel* Root)
{
	// A capsule-shaped ink plate: the round portrait at its left end, the name and bars sweeping right, each bar
	// shorter than the one above.
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("UnitFrame"));
	const bool bBarArt = HudTexture(TEXT("BarFrame")) != nullptr;
	// With the SpriteCook frames the bars carry their own trim: the plate is only a soft shade behind them.
	Plate->SetBrush(bBarArt ? RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.3f), 16.f) : RoundedBrush(InkColor, -1.f, TrimColor, 1.5f));
	Plate->SetPadding(FMargin(6.f, 6.f, 32.f, 6.f));
	UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate);
	PlateSlot->SetAnchors(FAnchors(0.f, 1.f));
	PlateSlot->SetAlignment(FVector2D(0.f, 1.f));
	PlateSlot->SetPosition(FVector2D(32.f, -32.f));
	PlateSlot->SetAutoSize(true);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Plate->SetContent(Row);

	// Portrait: gold ring, then the picture clipped to a circle with a crimson rim.
	USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PortraitBox->SetWidthOverride(PortraitSize);
	PortraitBox->SetHeightOverride(PortraitSize);
	UBorder* Ring = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	UTexture2D* RingArt = HudTexture(TEXT("PortraitRing"));
	Ring->SetBrush(RoundedBrush(RingArt ? FLinearColor::Transparent : TrimColor, -1.f));
	Ring->SetPadding(RingArt ? FMargin(0.f) : FMargin(3.f));
	PortraitBox->AddChild(Ring);
	UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Ring->SetContent(Face);
	UImage* Photo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Portrait"));
	UTexture2D* PhotoTexture = LoadObject<UTexture2D>(nullptr, PortraitPath, nullptr, LOAD_NoWarn);
	Photo->SetBrush(RoundedBrush(PhotoTexture ? FLinearColor::White : PortraitBackColor, -1.f, PortraitRimColor, 2.f, PhotoTexture));
	UOverlaySlot* PhotoSlot = Face->AddChildToOverlay(Photo);
	PhotoSlot->SetHorizontalAlignment(HAlign_Fill);
	PhotoSlot->SetVerticalAlignment(VAlign_Fill);
	if (RingArt)
	{
		PhotoSlot->SetPadding(FMargin(PortraitSize * 0.085f)); // the photo sits inside the ring
		UImage* RingImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitRing"));
		RingImage->SetBrush(ArtBrush(RingArt, FVector2D(PortraitSize, PortraitSize)));
		UOverlaySlot* RingSlot = Face->AddChildToOverlay(RingImage);
		RingSlot->SetHorizontalAlignment(HAlign_Fill);
		RingSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (!PhotoTexture)
	{
		AddLabel(Face, 44, HAlign_Center, VAlign_Center, TrimColor)->SetText(INVTEXT("忍")); // shinobi
	}
	Row->AddChildToHorizontalBox(PortraitBox)->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Bars"));
	UHorizontalBoxSlot* BarsSlot = Row->AddChildToHorizontalBox(Bars);
	BarsSlot->SetVerticalAlignment(VAlign_Center);
	BarsSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

	NameText = MakeText(15, NameColor, true);
	Bars->AddChildToVerticalBox(NameText)->SetPadding(FMargin(2.f, 0.f, 0.f, 2.f));

	UOverlay* HealthRow = AddBarRow(Bars, 340.f, 26.f);
	HealthTrailBar = AddBar(HealthRow, TEXT("HealthTrailBar"), TrailColor, true);
	HealthBar = AddBar(HealthRow, TEXT("HealthBar"), HealthColor, false);
	HealthText = AddLabel(HealthRow, 10, HAlign_Center, VAlign_Center, FLinearColor::White);

	UOverlay* ChakraRow = AddBarRow(Bars, 300.f, 20.f);
	ChakraBar = AddBar(ChakraRow, TEXT("ChakraBar"), ChakraColor, true);
	ChakraText = AddLabel(ChakraRow, 9, HAlign_Center, VAlign_Center, FLinearColor::White);

	StaminaBar = AddBar(AddBarRow(Bars, 260.f, 16.f), TEXT("StaminaBar"), StaminaColor, true);
	BuildSubstitutionPips(Bars);
	BuildStatusIcons(Bars);
}

UOverlay* UShinobiHUDWidget::AddBarRow(UVerticalBox* Bars, float Width, float Height)
{
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	// Without an explicit width, SProgressBar::ComputeDesiredSize falls back to the 20x12 marquee image
	// (the runtime UMG style's Common/ProgressBar_Marquee brush, set up in UMGCoreStyle.cpp), so the
	// auto-sized canvas slot would otherwise collapse to ~20 px wide.
	SizeBox->SetWidthOverride(Width);
	SizeBox->SetHeightOverride(Height);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeBox->AddChild(Layers);
	Bars->AddChildToVerticalBox(SizeBox)->SetPadding(FMargin(0.f, 2.f));
	return Layers;
}

UProgressBar* UShinobiHUDWidget::AddBar(UOverlay* Row, FName Name, const FLinearColor& Color, bool bBacked)
{
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
	FProgressBarStyle Style;
	// An unbacked bar sits on top of a backed one in the same row (health over its trail).
	UTexture2D* FrameArt = HudTexture(TEXT("BarFrame"));
	UTexture2D* FillArt = HudTexture(TEXT("BarFill"));
	if (FrameArt && FillArt)
	{
		// SpriteCook frame under a grey fill that the bar's colour tints; the fill sits inside the frame's trim.
		Style.SetBackgroundImage(bBacked ? ArtBrush(FrameArt, FVector2D(256.f, 24.f)) : RoundedBrush(FLinearColor::Transparent, -1.f));
		Style.SetFillImage(ArtBrush(FillArt, FVector2D(256.f, 16.f)));
		Bar->SetWidgetStyle(Style);
		Bar->SetBorderPadding(FVector2D(11.f, 4.f));
	}
	else
	{
		Style.SetBackgroundImage(bBacked ? RoundedBrush(BarBackColor, -1.f, TrimColor, 1.f) : RoundedBrush(FLinearColor::Transparent, -1.f));
		Style.SetFillImage(RoundedBrush(FLinearColor::White, -1.f));
		Bar->SetWidgetStyle(Style);
		Bar->SetBorderPadding(FVector2D(2.f, 2.f));
	}
	Bar->SetFillColorAndOpacity(Color);
	UOverlaySlot* BarSlot = Row->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Fill);
	return Bar;
}

void UShinobiHUDWidget::BuildSubstitutionPips(UVerticalBox* Bars)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SubstitutionPips"));
	Bars->AddChildToVerticalBox(Row)->SetPadding(FMargin(4.f, 6.f, 0.f, 0.f));
	UTexture2D* OnArt = HudTexture(TEXT("PipOn"));
	UTexture2D* OffArt = HudTexture(TEXT("PipOff"));
	const bool bArt = OnArt && OffArt;
	PipOnBrush = ArtBrush(OnArt, FVector2D(PipSize, PipSize));
	PipOffBrush = ArtBrush(OffArt, FVector2D(PipSize, PipSize));
	for (int32 Index = 0; Index < NumPips; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		UBorder* Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		if (bArt)
		{
			Box->SetWidthOverride(PipSize);
			Box->SetHeightOverride(PipSize);
			Pip->SetBrush(PipOffBrush);
		}
		else
		{
			// A gold-trimmed square turned 45 degrees: a diamond.
			Box->SetWidthOverride(11.f);
			Box->SetHeightOverride(11.f);
			Box->SetRenderTransformAngle(45.f);
			Pip->SetBrush(RoundedBrush(PipOffColor, 2.f, TrimColor, 1.f));
		}
		Box->AddChild(Pip);
		Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(0.f, 0.f, bArt ? 6.f : 10.f, 0.f));
		SubstitutionPips.Add(Pip);
	}
}

void UShinobiHUDWidget::BuildStatusIcons(UVerticalBox* Bars)
{
	// Burn, black flame, shock, stagger: shown only while the player carries that state (see NativeTick).
	static const TCHAR* Names[] = { TEXT("StatusBurn"), TEXT("StatusBlackflame"), TEXT("StatusShock"), TEXT("StatusStagger") };
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StatusIcons"));
	Bars->AddChildToVerticalBox(Row)->SetPadding(FMargin(2.f, 6.f, 0.f, 0.f));
	for (const TCHAR* Name : Names)
	{
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetBrush(ArtBrush(HudTexture(Name), FVector2D(StatusSize, StatusSize)));
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		Row->AddChildToHorizontalBox(Icon)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		StatusIcons.Add(Icon);
	}
}

void UShinobiHUDWidget::BuildJutsuSlots(UCanvasPanel* Root)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("JutsuSlots"));
	UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
	RowSlot->SetAnchors(FAnchors(0.5f, 1.f));
	RowSlot->SetAlignment(FVector2D(0.5f, 1.f));
	RowSlot->SetPosition(FVector2D(0.f, -32.f));
	RowSlot->SetAutoSize(true);

	static const TCHAR* KeyLabels[NumJutsuSlots] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4") };
	for (int32 Index = 0; Index < NumJutsuSlots; ++Index)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Row->AddChildToHorizontalBox(Column)->SetPadding(FMargin(6.f, 0.f));
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(SlotSize);
		Box->SetHeightOverride(SlotSize);
		Column->AddChildToVerticalBox(Box);
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		UTexture2D* FrameArt = HudTexture(TEXT("SlotFrame"));
		bFrameArt = FrameArt != nullptr;
		Frame->SetBrush(FrameArt ? ArtBrush(FrameArt, FVector2D(SlotSize, SlotSize)) : RoundedBrush(SlotColor, 9.f, TrimColor, 2.f));
		Frame->SetPadding(FMargin(FrameArt ? SlotBorder : 3.f));
		Box->AddChild(Frame);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Frame->SetContent(Layers);

		// Back to front: icon (or the name while it's missing), cooldown shade, cooldown seconds, key, cost.
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetVisibility(ESlateVisibility::Collapsed); // until a jutsu with an icon fills the slot
		UOverlaySlot* IconSlot = Layers->AddChildToOverlay(Icon);
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
		UTextBlock* Fallback = AddLabel(Layers, 12, HAlign_Center, VAlign_Center, FLinearColor::White);
		USizeBox* Shade = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Shade->SetHeightOverride(0.f);
		UBorder* ShadeFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ShadeFill->SetBrush(RoundedBrush(ShadeColor, 6.f));
		Shade->AddChild(ShadeFill);
		UOverlaySlot* ShadeSlot = Layers->AddChildToOverlay(Shade);
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Bottom);
		UTextBlock* CooldownText = AddLabel(Layers, 24, HAlign_Center, VAlign_Center, FLinearColor::White);

		USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyBox->SetWidthOverride(24.f);
		KeyBox->SetHeightOverride(24.f);
		UBorder* KeyBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		UTexture2D* BadgeArt = HudTexture(TEXT("KeyBadge"));
		KeyBadge->SetBrush(BadgeArt ? ArtBrush(BadgeArt, FVector2D(24.f, 24.f)) : RoundedBrush(InkColor, -1.f, TrimColor, 1.f));
		KeyBadge->SetPadding(FMargin(0.f));
		KeyBadge->SetHorizontalAlignment(HAlign_Center);
		KeyBadge->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Key = MakeText(10, KeyColor, true);
		Key->SetText(FText::FromString(KeyLabels[Index]));
		KeyBadge->SetContent(Key);
		KeyBox->AddChild(KeyBadge);
		UOverlaySlot* KeySlot = Layers->AddChildToOverlay(KeyBox);
		KeySlot->SetHorizontalAlignment(HAlign_Left);
		KeySlot->SetVerticalAlignment(VAlign_Top);
		KeySlot->SetPadding(FMargin(bFrameArt ? 0.f : 2.f)); // the art frame has its own margin
		UTextBlock* CostText = AddLabel(Layers, 12, HAlign_Right, VAlign_Bottom, CostColor);

		UTextBlock* Name = MakeText(12, NameColor, true);
		UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(Name);
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

		SlotFrames.Add(Frame);
		SlotIcons.Add(Icon);
		SlotFallbacks.Add(Fallback);
		SlotNames.Add(Name);
		SlotShades.Add(Shade);
		SlotCooldownTexts.Add(CooldownText);
		SlotCosts.Add(CostText);
	}
	SlotFlashUntil.Init(0.0, NumJutsuSlots);
	SlotJutsu.Init(nullptr, NumJutsuSlots);
}

UTextBlock* UShinobiHUDWidget::MakeText(int32 FontSize, const FLinearColor& Tint, bool bBold)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Font.OutlineSettings.OutlineSize = 1; // readable over bright bursts, the sky and the icons
	if (bBold)
	{
		Font.TypefaceFontName = TEXT("Bold");
	}
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Tint));
	return Text;
}

UTextBlock* UShinobiHUDWidget::AddCanvasText(UCanvasPanel* Root, FName Name, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position, int32 FontSize, ETextJustify::Type Justify)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Font.OutlineSettings.OutlineSize = 2; // readable over bright bursts and the sky
	Text->SetFont(Font);
	Text->SetJustification(Justify);
	UCanvasPanelSlot* TextSlot = Root->AddChildToCanvas(Text);
	TextSlot->SetAnchors(Anchors);
	TextSlot->SetAlignment(Alignment);
	TextSlot->SetPosition(Position);
	TextSlot->SetAutoSize(true);
	return Text;
}

UTextBlock* UShinobiHUDWidget::AddLabel(UOverlay* Layers, int32 FontSize, EHorizontalAlignment HorizontalAlign, EVerticalAlignment VerticalAlign, const FLinearColor& Tint)
{
	UTextBlock* Label = MakeText(FontSize, Tint, true);
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
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const APlayerState* Me = GetOwningPlayerState();
	NameText->SetText(Me ? FText::FromString(Me->GetPlayerName()) : FText::GetEmpty());

	const float HealthRatio = Stats->GetHealth() / FMath::Max(Stats->GetMaxHealth(), 1.f);
	HealthBar->SetPercent(HealthRatio);
	HealthBar->SetFillColorAndOpacity(HealthRatio < 0.3f ? LowHealthColor : HealthColor);
	// The trail snaps up on heals; on damage it holds a moment, then drains down to the new health.
	if (HealthRatio >= HealthTrail)
	{
		HealthTrail = HealthRatio;
	}
	else if (HealthRatio < LastHealthRatio)
	{
		TrailHoldUntil = Now + TrailHoldSeconds;
	}
	else if (Now > TrailHoldUntil)
	{
		HealthTrail = FMath::FInterpConstantTo(HealthTrail, HealthRatio, InDeltaTime, TrailDrainPerSecond);
	}
	LastHealthRatio = HealthRatio;
	HealthTrailBar->SetPercent(HealthTrail);
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Stats->GetHealth()), FMath::RoundToInt(Stats->GetMaxHealth()))));
	ChakraBar->SetPercent(Stats->GetChakra() / FMath::Max(Stats->GetMaxChakra(), 1.f));
	ChakraText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::FloorToInt(Stats->GetChakra()), FMath::RoundToInt(Stats->GetMaxChakra()))));
	StaminaBar->SetPercent(Stats->GetStamina() / FMath::Max(Stats->GetMaxStamina(), 1.f));

	const int32 Charges = FMath::RoundToInt(Stats->GetSubstitutionCharges());
	for (int32 Index = 0; Index < SubstitutionPips.Num(); ++Index)
	{
		if (PipOnBrush.GetResourceObject())
		{
			SubstitutionPips[Index]->SetBrush(Index < Charges ? PipOnBrush : PipOffBrush);
		}
		else
		{
			SubstitutionPips[Index]->SetBrushColor(Index < Charges ? PipOnColor : PipOffColor);
		}
	}
	const FGameplayTag StatusTags[] = { ShinobiTags::State_Burning_Normal, ShinobiTags::State_Burning_Black, ShinobiTags::State_Shocked, ShinobiTags::State_Staggered };
	for (int32 Index = 0; Index < StatusIcons.Num(); ++Index)
	{
		const bool bOn = ASC->HasMatchingGameplayTag(StatusTags[Index]);
		StatusIcons[Index]->SetVisibility(bOn && StatusIcons[Index]->GetBrush().GetResourceObject() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	UpdateJutsuSlots(*ASC);
	UpdateArena(Cast<AShinobiPlayerCharacter>(Pawn));
	UpdateCompass();
}

UTextBlock* UShinobiHUDWidget::CompassLabel(int32 Index)
{
	while (CompassLabels.Num() <= Index)
	{
		UTextBlock* Label = AddCanvasText(RootCanvas, NAME_None, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D::ZeroVector, 12, ETextJustify::Center);
		CompassLabels.Add(Label);
	}
	return CompassLabels[Index];
}

void UShinobiHUDWidget::UpdateCompass()
{
	const AShinobiGameState* State = GetWorld() ? GetWorld()->GetGameState<AShinobiGameState>() : nullptr;
	const APlayerController* PC = GetOwningPlayer();
	const bool bShow = State && State->NumCamps > 0 && PC && PC->PlayerCameraManager && RootCanvas;
	if (CompassBar)
	{
		CompassBar->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	int32 Used = 0;
	if (bShow)
	{
		const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
		const float CameraYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;
		auto Mark = [&](float Yaw, const FString& Text, const FLinearColor& Colour)
		{
			const float Delta = FRotator::NormalizeAxis(Yaw - CameraYaw);
			if (FMath::Abs(Delta) > 90.f)
			{
				return;
			}
			UTextBlock* Label = CompassLabel(Used++);
			Label->SetText(FText::FromString(Text));
			Label->SetColorAndOpacity(FSlateColor(Colour));
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
			Cast<UCanvasPanelSlot>(Label->Slot)->SetPosition(FVector2D(Delta / 90.f * CompassHalfWidth, 62.f));
		};
		auto MarkAt = [&](const FVector& Where, const FString& Name, const FLinearColor& Colour)
		{
			Mark((Where - Eye).Rotation().Yaw, FString::Printf(TEXT("%s %dm"), *Name, FMath::RoundToInt(FVector::Dist2D(Where, Eye) / 100.f)), Colour);
		};
		// The layout's +Y is north.
		Mark(90.f, TEXT("N"), FLinearColor::White);
		Mark(0.f, TEXT("E"), FLinearColor(0.8f, 0.8f, 0.8f));
		Mark(-90.f, TEXT("S"), FLinearColor(0.8f, 0.8f, 0.8f));
		Mark(180.f, TEXT("W"), FLinearColor(0.8f, 0.8f, 0.8f));
		MarkAt(State->VillageLocation, State->HubName, FLinearColor(0.95f, 0.9f, 0.7f));
		for (TActorIterator<AShinobiCamp> It(GetWorld()); It; ++It)
		{
			const bool bCleared = It->GetState() == EShinobiCampState::Cleared;
			const FLinearColor Colour = bCleared ? FLinearColor(0.5f, 0.5f, 0.5f) : (It->bBoss ? FLinearColor(0.75f, 0.3f, 1.f) : FLinearColor(1.f, 0.3f, 0.2f));
			MarkAt(It->GetActorLocation(), It->bBoss ? TEXT("Boss") : TEXT("Camp"), Colour);
		}
	}
	for (int32 Index = Used; Index < CompassLabels.Num(); ++Index)
	{
		CompassLabels[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UShinobiHUDWidget::UpdateArena(const AShinobiPlayerCharacter* Player)
{
	const AShinobiGameState* State = GetWorld() ? GetWorld()->GetGameState<AShinobiGameState>() : nullptr;
	const int32 Countdown = State ? FMath::CeilToInt(State->GetPhaseTimeRemaining()) : 0;
	FString Wave;
	FString Banner;
	if (State && static_cast<uint8>(State->Phase) != LastPhase)
	{
		LastPhase = static_cast<uint8>(State->Phase);
		if (State->Phase == EShinobiArenaPhase::Fighting)
		{
			ShinobiSfx::Play2D(this, EShinobiSfx::Gong, 0.7f);
		}
		else if (State->Phase == EShinobiArenaPhase::Victory)
		{
			ShinobiSfx::Play2D(this, EShinobiSfx::Victory);
		}
	}
	if (State)
	{
		switch (State->Phase)
		{
		case EShinobiArenaPhase::Break:
			Wave = State->Wave == State->NumWaves ? FString::Printf(TEXT("Final wave in %d"), Countdown)
				: FString::Printf(TEXT("Wave %d / %d in %d"), State->Wave, State->NumWaves, Countdown);
			break;
		case EShinobiArenaPhase::Fighting:
			Wave = FString::Printf(TEXT("Wave %d / %d   Enemies left: %d"), State->Wave, State->NumWaves, State->EnemiesRemaining);
			break;
		case EShinobiArenaPhase::Wiped:
			Wave = FString::Printf(TEXT("Wave %d / %d"), State->Wave, State->NumWaves);
			Banner = FString::Printf(TEXT("WIPED OUT\nWave restarting in %d"), Countdown);
			break;
		case EShinobiArenaPhase::Victory:
			Banner = State->NumCamps > 0 ? State->VictoryText : TEXT("VICTORY");
			break;
		case EShinobiArenaPhase::OpenWorld:
			Wave = FString::Printf(TEXT("Camps cleared %d / %d"), State->CampsCleared, State->NumCamps);
			break;
		default:
			break;
		}
	}

	// Teammates: every other player in the session.
	FString Team;
	if (State)
	{
		for (const APlayerState* Mate : State->PlayerArray)
		{
			const AShinobiCharacterBase* Body = Mate ? Cast<AShinobiCharacterBase>(Mate->GetPawn()) : nullptr;
			if (!Body || Body == Player)
			{
				continue;
			}
			const UAbilitySystemComponent* MateASC = Body->GetAbilitySystemComponent();
			const TCHAR* Status = MateASC->HasMatchingGameplayTag(ShinobiTags::State_Dead) ? TEXT("  DEAD")
				: MateASC->HasMatchingGameplayTag(ShinobiTags::State_Downed) ? TEXT("  DOWNED") : TEXT("");
			Team += FString::Printf(TEXT("%s  %d / %d%s\n"), *Mate->GetPlayerName(), FMath::RoundToInt(Body->GetAttributes()->GetHealth()),
				FMath::RoundToInt(Body->GetAttributes()->GetMaxHealth()), Status);
		}
	}

	FString Hint;
	if (Player)
	{
		const UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		if (Banner.IsEmpty() && ASC->HasMatchingGameplayTag(ShinobiTags::State_Dead))
		{
			Banner = TEXT("DEAD\nBack at the next wave");
		}
		else if (Banner.IsEmpty() && ASC->HasMatchingGameplayTag(ShinobiTags::State_Downed))
		{
			Banner = FString::Printf(TEXT("DOWNED\nBleeding out in %d - a teammate can revive you"), FMath::CeilToInt(Player->GetBleedOutRemaining()));
		}
		if (const FGameplayAbilitySpec* Revive = ASC->FindAbilitySpecFromClass(UGA_Revive::StaticClass()))
		{
			if (const UGA_Revive* Instance = Cast<UGA_Revive>(Revive->GetPrimaryInstance()); Instance && Instance->IsActive())
			{
				Hint = FString::Printf(TEXT("Reviving... %d%%"), FMath::RoundToInt(Instance->GetProgress() * 100.f));
			}
		}
		if (Hint.IsEmpty() && Player->GetLockTarget())
		{
			Hint = TEXT("[ LOCKED ]");
		}
	}
	WaveText->SetText(FText::FromString(Wave));
	TeamText->SetText(FText::FromString(Team));
	BannerText->SetText(FText::FromString(Banner));
	HintText->SetText(FText::FromString(Hint));
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
		if (SlotJutsu[Index] != Jutsu)
		{
			// The static parts, once per jutsu: icon (or its name while the texture is missing), name, cost.
			SlotJutsu[Index] = Jutsu;
			UTexture2D* IconTexture = Jutsu->Icon.IsNull() ? nullptr : LoadObject<UTexture2D>(nullptr, *Jutsu->Icon.ToString(), nullptr, LOAD_NoWarn);
			SlotIcons[Index]->SetBrush(RoundedBrush(FLinearColor::White, 6.f, FLinearColor::Transparent, 0.f, IconTexture));
			SlotIcons[Index]->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			SlotFallbacks[Index]->SetText(IconTexture ? FText::GetEmpty() : Jutsu->DisplayName);
			SlotNames[Index]->SetText(Jutsu->DisplayName);
			SlotCosts[Index]->SetText(FText::AsNumber(FMath::RoundToInt(Jutsu->ChakraCost)));
		}
		const FShinobiJutsuSlotState State = Jutsu->GetSlotState(ASC);
		const bool bCooling = State.CooldownRemaining > 0.f && State.CooldownDuration > 0.f;
		SlotIcons[Index]->SetColorAndOpacity(State.bAffordable ? FLinearColor::White : DimIconColor);
		SlotFallbacks[Index]->SetRenderOpacity(State.bAffordable ? 1.f : 0.35f);
		SlotCosts[Index]->SetColorAndOpacity(FSlateColor(State.bAffordable ? CostColor : NoChakraColor));
		SlotCooldownTexts[Index]->SetText(bCooling ? FText::AsNumber(FMath::CeilToInt(State.CooldownRemaining)) : FText::GetEmpty());
		SlotShades[Index]->SetHeightOverride(bCooling ? (SlotSize - 2.f * SlotBorder) * State.CooldownRemaining / State.CooldownDuration : 0.f);
		if (bFrameArt)
		{
			SlotFrames[Index]->SetBrushColor(Now < SlotFlashUntil[Index] ? SlotFlashColor : FLinearColor::White);
		}
		else
		{
			SlotFrames[Index]->SetBrush(RoundedBrush(SlotColor, 9.f, Now < SlotFlashUntil[Index] ? SlotFlashColor : TrimColor, 2.f));
		}
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
