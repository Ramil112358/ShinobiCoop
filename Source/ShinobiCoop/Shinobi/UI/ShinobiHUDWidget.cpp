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
#include "Components/ScaleBox.h"
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
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/Pvp/ShinobiPvpText.h"
#include "Shinobi/VFX/ShinobiSfx.h"
#include "Shinobi/World/ShinobiCamp.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiDragonMount.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "Shinobi/UI/ShinobiMiniMapData.h"
#include "ShinobiCoop.h"
#include "Kismet/GameplayStatics.h"

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
	// Compass and minimap markers.
	const FLinearColor VillageMarkColor(0.95f, 0.9f, 0.7f);
	const FLinearColor CampMarkColor(1.f, 0.3f, 0.2f);
	const FLinearColor BossMarkColor(0.75f, 0.3f, 1.f);
	const FLinearColor ClearedMarkColor(0.5f, 0.5f, 0.5f);
	const FLinearColor MateMarkColor(0.3f, 0.75f, 1.f);
	constexpr float MiniMapEdge = 250.f; // the map's long edge on screen
	constexpr float MiniMapMargin = 24.f;
	constexpr float MiniMapArrowSize = 18.f;
	constexpr float BigMapShare = 0.86f; // of the screen, on the side the map fills
	constexpr float BigMapMarkScale = 1.4f;
	// Map zoom steps (1 = the whole map in view). A step is offered only while the view still spans MinMapViewSpan cm
	// and a texel of the art covers at most MaxMapArtStretch screen units.
	constexpr float MapZoomLevels[] = { 1.f, 2.f, 4.f, 8.f };
	constexpr float MinMapViewSpan = 6000.f;
	constexpr float MaxMapArtStretch = 2.5f;
	// Scripts/minimap/bake_minimaps.py imports SourceArt/UI/minimap_arrow.png here.
	const TCHAR* MiniMapArrowPath = TEXT("/Game/Shinobi/UI/Minimaps/T_MiniMap_Arrow.T_MiniMap_Arrow");
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
		BuildPagePips(Root);
		BuildAimCrosshair(Root);
		BuildDragonCrosshair(Root);
		BuildMiniMap(Root);

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
		OpponentText = AddCanvasText(Root, TEXT("OpponentText"), FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-40.f, 24.f), 14, ETextJustify::Right);
		BoostText = AddCanvasText(Root, TEXT("BoostText"), FAnchors(0.5f, 0.68f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, 16, ETextJustify::Center);
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

		// The name sits in a box a little wider than the slot and shrinks to fit it, so a long one ("Summon Crimson Dragon") cannot stretch
		// its column and make that slot wider than the others.
		UTextBlock* Name = MakeText(12, NameColor, true);
		USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NameBox->SetWidthOverride(SlotSize + 12.f);
		UScaleBox* NameScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		NameScale->SetStretch(EStretch::ScaleToFit);
		NameScale->SetStretchDirection(EStretchDirection::DownOnly);
		NameScale->AddChild(Name);
		NameBox->AddChild(NameScale);
		UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(NameBox);
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

		SlotColumns.Add(Column);
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

float UShinobiHUDWidget::GetJutsuSlotWidth(int32 Index) const
{
	return SlotColumns.IsValidIndex(Index) ? SlotColumns[Index]->GetDesiredSize().X : 0.f;
}

bool UShinobiHUDWidget::IsDragonCrosshairShown() const
{
	return DragonCrosshair && DragonCrosshair->GetVisibility() != ESlateVisibility::Collapsed;
}

void UShinobiHUDWidget::BuildDragonCrosshair(UCanvasPanel* Root)
{
	// A ring with a dot in it, drawn in the dragon's fire colour; positioned each frame by UpdateDragonCrosshair.
	DragonCrosshair = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DragonCrosshair"));
	DragonCrosshair->SetWidthOverride(44.f);
	DragonCrosshair->SetHeightOverride(44.f);
	DragonCrosshair->SetVisibility(ESlateVisibility::Collapsed);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	DragonCrosshair->AddChild(Layers);
	UBorder* Ring = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Ring->SetBrush(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.12f), 22.f, FLinearColor::White, 2.5f));
	UOverlaySlot* RingSlot = Layers->AddChildToOverlay(Ring);
	RingSlot->SetHorizontalAlignment(HAlign_Fill);
	RingSlot->SetVerticalAlignment(VAlign_Fill);
	UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dot->SetBrush(RoundedBrush(FLinearColor::White, 4.f));
	USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DotBox->SetWidthOverride(7.f);
	DotBox->SetHeightOverride(7.f);
	DotBox->AddChild(Dot);
	UOverlaySlot* DotSlot = Layers->AddChildToOverlay(DotBox);
	DotSlot->SetHorizontalAlignment(HAlign_Center);
	DotSlot->SetVerticalAlignment(VAlign_Center);
	DragonCrosshairParts = { Ring, Dot };
	UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(DragonCrosshair);
	CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetAutoSize(true);
}

void UShinobiHUDWidget::UpdateDragonCrosshair(const AShinobiPlayerCharacter* Player)
{
	if (!DragonCrosshair)
	{
		return;
	}
	const AShinobiDragonMount* Dragon = Player && Player->IsLocallyControlled() && Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Mounted)
		? AShinobiDragonMount::Find(*Player) : nullptr;
	FVector2D Screen;
	if (!Dragon || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), AShinobiDragonMount::BreathTarget(*Player, Dragon->GetKind()), Screen, true))
	{
		DragonCrosshair->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	DragonCrosshair->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DragonCrosshair->Slot))
	{
		CanvasSlot->SetPosition(Screen);
	}
	const FLinearColor Colour = Dragon->GetKind() == EShinobiDragonKind::Tairn ? FLinearColor(0.55f, 0.8f, 1.f) : FLinearColor(1.f, 0.62f, 0.2f);
	for (UBorder* Part : DragonCrosshairParts)
	{
		Part->SetBrushColor(Colour);
	}
}

void UShinobiHUDWidget::BuildAimCrosshair(UCanvasPanel* Root)
{
	AimCrosshair = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AimCrosshair"));
	AimCrosshair->SetWidthOverride(22.f);
	AimCrosshair->SetHeightOverride(22.f);
	AimCrosshair->SetVisibility(ESlateVisibility::Collapsed);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	AimCrosshair->AddChild(Layers);
	UBorder* Ring = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Ring->SetBrush(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.08f), 11.f, FLinearColor(1.f, 1.f, 1.f, 0.9f), 1.5f));
	UOverlaySlot* RingSlot = Layers->AddChildToOverlay(Ring);
	RingSlot->SetHorizontalAlignment(HAlign_Fill);
	RingSlot->SetVerticalAlignment(VAlign_Fill);
	UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dot->SetBrush(RoundedBrush(FLinearColor::White, 2.f));
	USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DotBox->SetWidthOverride(4.f);
	DotBox->SetHeightOverride(4.f);
	DotBox->AddChild(Dot);
	UOverlaySlot* DotSlot = Layers->AddChildToOverlay(DotBox);
	DotSlot->SetHorizontalAlignment(HAlign_Center);
	DotSlot->SetVerticalAlignment(VAlign_Center);
	UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(AimCrosshair);
	CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetAutoSize(true);
}

void UShinobiHUDWidget::UpdateAimCrosshair(const AShinobiPlayerCharacter* Player, const FVector2D& ViewSize)
{
	if (!AimCrosshair)
	{
		return;
	}
	const bool bShow = Player && Player->IsLocallyControlled()
		&& !Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Mounted);
	AimCrosshair->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		const FVector2D AimPosition = Player->GetJutsuTargetScreenPosition();
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AimCrosshair->Slot))
		{
			CanvasSlot->SetPosition(FVector2D((AimPosition.X - 0.5f) * ViewSize.X, (AimPosition.Y - 0.5f) * ViewSize.Y));
		}
	}
}

void UShinobiHUDWidget::BuildPagePips(UCanvasPanel* Root)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PagePips"));
	UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
	RowSlot->SetAnchors(FAnchors(0.5f, 1.f));
	RowSlot->SetAlignment(FVector2D(0.5f, 1.f));
	RowSlot->SetPosition(FVector2D(0.f, -10.f));
	RowSlot->SetAutoSize(true);
	for (int32 Page = 1; Page <= ShinobiTags::NumJutsuPages; ++Page)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(9.f);
		Box->SetHeightOverride(9.f);
		Box->SetRenderTransformAngle(45.f); // a diamond, like the Substitution pips without art
		UBorder* Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Pip->SetBrush(RoundedBrush(PipOffColor, 2.f, TrimColor, 1.f));
		Box->AddChild(Pip);
		Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(6.f, 0.f));
		PagePips.Add(Pip);
	}
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
	const AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(Pawn);
	UpdateAimCrosshair(Player, MyGeometry.GetLocalSize());
	const UShinobiAttributeSet* Stats = Pawn ? Pawn->GetAttributes() : nullptr;
	UAbilitySystemComponent* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
	BindFailureFlash(ASC);
	if (!Stats || !ASC || !HealthBar)
	{
		UpdateDragonCrosshair(nullptr);
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
	UpdateJutsuSlots(*ASC, Player ? Player->GetJutsuPage() : 1, Player);
	UpdateArena(Player);
	UpdateCompass();
	UpdateMiniMap(Pawn, MyGeometry.GetLocalSize());
	UpdateDragonCrosshair(Player);
}

void UShinobiHUDWidget::BuildMiniMap(UCanvasPanel* Root)
{
	const FString Level = UGameplayStatics::GetCurrentLevelName(this, true);
	MiniMapData = UShinobiMiniMapData::ForLevel(Level);
	if (!MiniMapData || !MiniMapData->HasValidBounds() || !MiniMapData->HasValidTileGrid())
	{
		UE_CLOG(Level.StartsWith(TEXT("Lvl_")), LogShinobiCoop, Warning, TEXT("%s has no valid minimap: add it to Scripts/minimap/minimap_maps.json and bake"), *Level);
		MiniMapData = nullptr;
		return;
	}
	const FVector2D Extent = MiniMapData->WorldMax - MiniMapData->WorldMin;
	MiniMapSize = Extent / Extent.GetMax() * MiniMapEdge;

	BigMapShade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BigMapShade"));
	BigMapShade->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	BigMapShade->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ShadeSlot = Root->AddChildToCanvas(BigMapShade);
	ShadeSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ShadeSlot->SetOffsets(FMargin(0.f));

	MiniMapFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MiniMapFrame"));
	MiniMapFrame->SetBrush(RoundedBrush(InkColor, 6.f, TrimColor, 1.5f));
	MiniMapFrame->SetPadding(FMargin(4.f));
	Root->AddChildToCanvas(MiniMapFrame); // laid out by UpdateMiniMap

	MiniMapCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MiniMapCanvas"));
	MiniMapCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	MiniMapFrame->SetContent(MiniMapCanvas);
	for (const FShinobiMiniMapTile& Tile : MiniMapData->Tiles)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrushFromTexture(Tile.Base);
		MiniMapCanvas->AddChildToCanvas(Image);
		MiniMapTiles.Add(Image);
	}

	MiniMapArrow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MiniMapArrow"));
	if (UTexture2D* Arrow = LoadObject<UTexture2D>(nullptr, MiniMapArrowPath, nullptr, LOAD_NoWarn))
	{
		MiniMapArrow->SetBrush(ArtBrush(Arrow, FVector2D(MiniMapArrowSize)));
	}
	else
	{
		MiniMapArrow->SetBrush(RoundedBrush(FLinearColor::White, -1.f, InkColor, 2.f));
	}
	MiniMapArrow->SetRenderTransformPivot(FVector2D(0.5f));
	UCanvasPanelSlot* ArrowSlot = MiniMapCanvas->AddChildToCanvas(MiniMapArrow);
	ArrowSlot->SetAlignment(FVector2D(0.5f));
	ArrowSlot->SetZOrder(1); // above the dots

	BigMapHint = MakeText(13, NameColor);
	BigMapHint->SetText(FText::FromString(TEXT("[M] Close     [Wheel] [+] [-] Zoom")));
	BigMapHint->SetShadowOffset(FVector2D(1.f));
	BigMapHint->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	UCanvasPanelSlot* HintSlot = MiniMapCanvas->AddChildToCanvas(BigMapHint);
	HintSlot->SetAnchors(FAnchors(0.f, 1.f));
	HintSlot->SetAlignment(FVector2D(0.f, 1.f));
	HintSlot->SetPosition(FVector2D(10.f, -8.f));
	HintSlot->SetAutoSize(true);
	HintSlot->SetZOrder(2);
	BigMapHint->SetVisibility(ESlateVisibility::Collapsed);
}

void UShinobiHUDWidget::ToggleBigMap()
{
	bBigMap = MiniMapData && !bBigMap;
}

int32 UShinobiHUDWidget::MaxMapZoom(float ViewLong) const
{
	const float WorldLong = (MiniMapData->WorldMax - MiniMapData->WorldMin).GetMax();
	// Asked each time: in the editor the tile textures report 0 x 0 until they finish compiling.
	FVector2D ArtSize = FVector2D::ZeroVector;
	for (const FShinobiMiniMapTile& Tile : MiniMapData->Tiles)
	{
		ArtSize.X += Tile.Y == 0 ? Tile.Base->GetSizeX() : 0;
		ArtSize.Y += Tile.X == 0 ? Tile.Base->GetSizeY() : 0;
	}
	const float MapArtLong = ArtSize.GetMax();
	int32 Index = 0;
	while (Index + 1 < UE_ARRAY_COUNT(MapZoomLevels)
		&& WorldLong / MapZoomLevels[Index + 1] >= MinMapViewSpan
		&& ViewLong * MapZoomLevels[Index + 1] <= MapArtLong * MaxMapArtStretch)
	{
		++Index;
	}
	return Index;
}

void UShinobiHUDWidget::ZoomMap(int32 Steps, bool bWrap)
{
	if (!MiniMapData)
	{
		return;
	}
	int32& Zoom = bBigMap ? BigZoom : MiniZoom;
	const int32 Max = MaxMapZoom(MapViewSize.GetMax());
	Zoom = bWrap && Zoom + Steps > Max ? 0 : FMath::Clamp(Zoom + Steps, 0, Max);
}

void UShinobiHUDWidget::UpdateMiniMap(const AShinobiCharacterBase* Pawn, const FVector2D& ScreenSize)
{
	if (!MiniMapData || !Pawn)
	{
		return;
	}
	// The corner map, or the whole map fitted into most of the screen.
	const FVector2D Extent = MiniMapData->WorldMax - MiniMapData->WorldMin;
	const float BigScale = FMath::Min(ScreenSize.X / Extent.X, ScreenSize.Y / Extent.Y) * BigMapShare;
	const FVector2D View = bBigMap ? FVector2D(FMath::RoundToFloat(Extent.X * BigScale), FMath::RoundToFloat(Extent.Y * BigScale)) : MiniMapSize;
	if (!View.Equals(MapViewSize) || bBigMap != bMapLaidOutBig)
	{
		MapViewSize = View;
		bMapLaidOutBig = bBigMap;
		UCanvasPanelSlot* FrameSlot = Cast<UCanvasPanelSlot>(MiniMapFrame->Slot);
		FrameSlot->SetAnchors(bBigMap ? FAnchors(0.5f, 0.5f) : FAnchors(1.f, 1.f));
		FrameSlot->SetAlignment(bBigMap ? FVector2D(0.5f) : FVector2D(1.f));
		FrameSlot->SetPosition(bBigMap ? FVector2D::ZeroVector : FVector2D(-MiniMapMargin));
		FrameSlot->SetSize(View + FVector2D(8.f));
		Cast<UCanvasPanelSlot>(MiniMapArrow->Slot)->SetSize(FVector2D(MiniMapArrowSize * (bBigMap ? BigMapMarkScale : 1.f)));
		BigMapShade->SetVisibility(bBigMap ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		BigMapHint->SetVisibility(bBigMap ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Zoomed in, the map is centred on the player but never pans past its own edge.
	int32& Zoom = bBigMap ? BigZoom : MiniZoom;
	Zoom = FMath::Min(Zoom, MaxMapZoom(View.GetMax()));
	const FVector2D Full = View * MapZoomLevels[Zoom];
	const FVector2D PlayerAt = MiniMapData->WorldToUV(Pawn->GetActorLocation()) * Full;
	const FVector2D Offset(FMath::RoundToFloat(FMath::Clamp(View.X / 2.f - PlayerAt.X, View.X - Full.X, 0.f)),
		FMath::RoundToFloat(FMath::Clamp(View.Y / 2.f - PlayerAt.Y, View.Y - Full.Y, 0.f)));
	if (!Offset.Equals(TileOffset) || !Full.Equals(TileFull))
	{
		TileOffset = Offset;
		TileFull = Full;
		for (int32 Index = 0; Index < MiniMapTiles.Num(); ++Index)
		{
			const FShinobiMiniMapTile& Tile = MiniMapData->Tiles[Index];
			// Whole pixels on both edges, so neighbouring tiles meet without a seam.
			const FVector2D Min(FMath::RoundToFloat(Offset.X + Tile.UVMin.X * Full.X), FMath::RoundToFloat(Offset.Y + Tile.UVMin.Y * Full.Y));
			const FVector2D Max(FMath::RoundToFloat(Offset.X + Tile.UVMax.X * Full.X), FMath::RoundToFloat(Offset.Y + Tile.UVMax.Y * Full.Y));
			UCanvasPanelSlot* TileSlot = Cast<UCanvasPanelSlot>(MiniMapTiles[Index]->Slot);
			TileSlot->SetPosition(Min);
			TileSlot->SetSize(Max - Min);
		}
	}

	auto ToMap = [this, &Offset, &Full](const FVector& Where) { return Offset + MiniMapData->WorldToUV(Where) * Full; };
	Cast<UCanvasPanelSlot>(MiniMapArrow->Slot)->SetPosition(ToMap(Pawn->GetActorLocation()));
	// The arrow art points up (north, -Y); yaw 0 faces +X (east, right) and grows clockwise on the map.
	MiniMapArrow->SetRenderTransformAngle(Pawn->GetActorRotation().Yaw + 90.f);

	const float Mark = bBigMap ? BigMapMarkScale : 1.f;
	int32 Used = 0;
	const AShinobiGameState* State = GetWorld()->GetGameState<AShinobiGameState>();
	if (State && State->NumCamps > 0)
	{
		MiniMapDot(Used++, ToMap(State->VillageLocation), 12.f * Mark, VillageMarkColor);
		for (TActorIterator<AShinobiCamp> It(GetWorld()); It; ++It)
		{
			const bool bCleared = It->GetState() == EShinobiCampState::Cleared;
			MiniMapDot(Used++, ToMap(It->GetActorLocation()), (It->bBoss ? 14.f : 10.f) * Mark,
				bCleared ? ClearedMarkColor : (It->bBoss ? BossMarkColor : CampMarkColor));
		}
	}
	// Teammates: every other player on our side (never the opposing side in a PvP arena).
	if (State)
	{
		for (const APlayerState* Mate : State->PlayerArray)
		{
			const AShinobiCharacterBase* Body = Mate ? Cast<AShinobiCharacterBase>(Mate->GetPawn()) : nullptr;
			if (Body && Body != Pawn && !Pawn->IsHostileTo(Body))
			{
				MiniMapDot(Used++, ToMap(Body->GetActorLocation()), 10.f * Mark, MateMarkColor);
			}
		}
	}
	for (int32 Index = Used; Index < MiniMapDots.Num(); ++Index)
	{
		MiniMapDots[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UShinobiHUDWidget::MiniMapDot(int32 Index, const FVector2D& Position, float Size, const FLinearColor& Colour)
{
	while (MiniMapDots.Num() <= Index)
	{
		UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		MiniMapCanvas->AddChildToCanvas(Dot)->SetAlignment(FVector2D(0.5f));
		MiniMapDots.Add(Dot);
	}
	UBorder* Dot = MiniMapDots[Index];
	Dot->SetBrush(RoundedBrush(Colour, -1.f, InkColor, 1.5f));
	Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* DotSlot = Cast<UCanvasPanelSlot>(Dot->Slot);
	DotSlot->SetPosition(Position);
	DotSlot->SetSize(FVector2D(Size));
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
	const bool bShow = State && State->NumCamps > 0 && PC && PC->PlayerCameraManager && RootCanvas && !bBigMap;
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
		// As on the minimap: +X is east, so north is -Y, and yaw grows clockwise (N, E, S, W).
		Mark(-90.f, TEXT("N"), FLinearColor::White);
		Mark(0.f, TEXT("E"), FLinearColor(0.8f, 0.8f, 0.8f));
		Mark(90.f, TEXT("S"), FLinearColor(0.8f, 0.8f, 0.8f));
		Mark(180.f, TEXT("W"), FLinearColor(0.8f, 0.8f, 0.8f));
		MarkAt(State->VillageLocation, State->HubName, VillageMarkColor);
		for (TActorIterator<AShinobiCamp> It(GetWorld()); It; ++It)
		{
			const bool bCleared = It->GetState() == EShinobiCampState::Cleared;
			const FLinearColor Colour = bCleared ? ClearedMarkColor : (It->bBoss ? BossMarkColor : CampMarkColor);
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
	if (State && State->IsPvp())
	{
		// The arena match: score strip on top, the round's words in the centre. The wave banners below are PvE's.
		Wave = ShinobiPvp::ScoreText(*State, State->GetPhaseTimeRemaining());
		Banner = ShinobiPvp::BannerText(*State, Player ? Player->GetTeam() : EShinobiTeam::TeamA, State->GetPhaseTimeRemaining());
	}
	else if (State)
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

	// Teammates: every other player on our side (in a PvP arena the opposing side is not listed).
	FString Team;
	if (State)
	{
		for (const APlayerState* Mate : State->PlayerArray)
		{
			const AShinobiCharacterBase* Body = Mate ? Cast<AShinobiCharacterBase>(Mate->GetPawn()) : nullptr;
			if (!Body || Body == Player || (Player && Player->IsHostileTo(Body)))
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
			Banner = State && State->IsPvp() ? TEXT("DEAD\nBack next round") : TEXT("DEAD\nBack at the next wave");
		}
		else if (Banner.IsEmpty() && ASC->HasMatchingGameplayTag(ShinobiTags::State_Downed))
		{
			Banner = State && State->IsPvp() ? FString(TEXT("DOWNED")) // revive is off in PvP
				: FString::Printf(TEXT("DOWNED\nBleeding out in %d - a teammate can revive you"), FMath::CeilToInt(Player->GetBleedOutRemaining()));
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
	const bool bPvp = State && State->IsPvp();
	WaveText->SetText(FText::FromString(Wave));
	TeamText->SetText(FText::FromString(Team));
	OpponentText->SetText(FText::FromString(bPvp ? OpponentList(Player) : FString()));
	BoostText->SetText(FText::FromString(bPvp ? BoostList(Player) : FString()));
	BannerText->SetText(FText::FromString(Banner));
	HintText->SetText(FText::FromString(Hint));
}

FString UShinobiHUDWidget::OpponentList(const AShinobiPlayerCharacter* Player) const
{
	FString Opponents;
	if (!Player || !GetWorld())
	{
		return Opponents;
	}
	int32 BotNumber = 0;
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const AShinobiCharacterBase* Body = *It;
		if (!IsPvpTeam(Body->GetTeam()) || !Player->IsHostileTo(Body) || Body->IsA<AShinobiSummon>())
		{
			continue;
		}
		// ponytail: a body with no player state is a bot; numbered in world order, no names yet
		const APlayerState* Owner = Body->GetPlayerState();
		const FString Name = Owner ? Owner->GetPlayerName() : FString::Printf(TEXT("Bot %d"), ++BotNumber);
		const UAbilitySystemComponent* ASC = Body->GetAbilitySystemComponent();
		const TCHAR* Status = ASC->HasMatchingGameplayTag(ShinobiTags::State_Dead) ? TEXT("  DEAD") : TEXT("");
		Opponents += FString::Printf(TEXT("%s  %d / %d%s\n"), *Name, FMath::RoundToInt(Body->GetAttributes()->GetHealth()),
			FMath::RoundToInt(Body->GetAttributes()->GetMaxHealth()), Status);
	}
	return Opponents;
}

FString UShinobiHUDWidget::BoostList(const AShinobiPlayerCharacter* Player) const
{
	FString Boosts;
	if (!Player)
	{
		return Boosts;
	}
	// The pickups (Task 5) own these tags; looked up by name so the HUD works before they exist.
	static const TPair<const TCHAR*, const TCHAR*> BoostTags[] = {
		{ TEXT("State.AttackBoost"), TEXT("Attack boost") },
		{ TEXT("State.SpeedBoost"), TEXT("Speed boost") },
		{ TEXT("State.Shield"), TEXT("Shield") },
	};
	const UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	for (const TPair<const TCHAR*, const TCHAR*>& Boost : BoostTags)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Boost.Key), false);
		if (!Tag.IsValid())
		{
			continue;
		}
		float Remaining = 0.f;
		for (const float Left : ASC->GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag))))
		{
			Remaining = FMath::Max(Remaining, Left);
		}
		if (Remaining > 0.f)
		{
			Boosts += FString::Printf(TEXT("%s %ds\n"), Boost.Value, FMath::CeilToInt(Remaining));
		}
	}
	return Boosts;
}

void UShinobiHUDWidget::UpdateJutsuSlots(const UAbilitySystemComponent& ASC, int32 Page, const AShinobiPlayerCharacter* Player)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (int32 Index = 0; Index < PagePips.Num(); ++Index)
	{
		PagePips[Index]->SetBrush(RoundedBrush(Index + 1 == Page ? TrimColor : PipOffColor, 2.f, TrimColor, 1.f));
		PagePips[Index]->GetParent()->SetVisibility(!Player || Player->PageHasJutsu(Index + 1) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	TArray<const UShinobiJutsuAbility*, TInlineAllocator<NumJutsuSlots>> OnPage;
	OnPage.Init(nullptr, NumJutsuSlots);
	for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
	{
		const UShinobiJutsuAbility* Jutsu = Cast<UShinobiJutsuAbility>(Spec.Ability);
		const int32 Index = Jutsu && Jutsu->Page == Page ? JutsuSlotIndex(Jutsu->InputID) : INDEX_NONE;
		if (OnPage.IsValidIndex(Index))
		{
			OnPage[Index] = Jutsu;
		}
	}
	for (int32 Index = 0; Index < NumJutsuSlots; ++Index)
	{
		const UShinobiJutsuAbility* Jutsu = OnPage[Index];
		SlotColumns[Index]->SetVisibility(Jutsu ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (!Jutsu)
		{
			SlotJutsu[Index] = nullptr; // refill the icon when a jutsu comes back to this slot
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
	// Off-page jutsu share this key and fail by design; during Kamui and the Teleport window the same key is
	// answered by another ability, so the jutsu's own failure is expected too.
	const AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(GetOwningPlayerPawn());
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!Jutsu || !Player || !ASC || Jutsu->Page != Player->GetJutsuPage()
		|| ASC->HasMatchingGameplayTag(ShinobiTags::State_Phased) || ASC->HasMatchingGameplayTag(ShinobiTags::State_TeleportMarked))
	{
		return;
	}
	const int32 Index = JutsuSlotIndex(Jutsu->InputID);
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
