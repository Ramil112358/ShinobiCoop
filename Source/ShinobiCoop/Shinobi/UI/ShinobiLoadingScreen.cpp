#include "Shinobi/UI/ShinobiLoadingScreen.h"
#include "Brushes/SlateColorBrush.h"
#include "ContentStreaming.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeLock.h"
#include "MoviePlayer.h"
#include "PipelineStateCache.h"
#include "Rendering/DrawElements.h"
#include "ShaderPipelineCache.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "ShinobiCoop.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

#define LOCTEXT_NAMESPACE "ShinobiLoadingScreen"

namespace
{
	TAutoConsoleVariable<bool> CVarShinobiLoadingScreen(TEXT("Shinobi.LoadingScreen"), true,
		TEXT("Show the loading screen through travel until the world is ready (0: off, maps appear as they load)."));

	/** Above every menu and dropdown (they use up to 1000). */
	constexpr int32 LoadingScreenZOrder = 20000;

	const FSlateBrush* WhiteBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	FSlateFontInfo LoadingFont(const TCHAR* Face, int32 Size, int32 LetterSpacing = 0)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Face, Size);
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	double SmoothStep01(double X)
	{
		X = FMath::Clamp(X, 0.0, 1.0);
		return X * X * (3.0 - 2.0 * X);
	}

	/** Holds keyboard, mouse-button and controller presses away from the game while the screen is up. Releases pass, so
	 * a key held when the screen went up doesn't stay down in the game; Alt+F4 passes too. */
	class FShinobiLoadingInputBlocker : public IInputProcessor
	{
	public:
		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			return !(InKeyEvent.IsAltDown() && InKeyEvent.GetKey() == EKeys::F4);
		}
		virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override { return true; }
		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return true; }
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return true; }
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override { return true; }
	};
}

/**
 * What one loading screen shows. Written on the game thread, read by its widgets, which during a blocking map load
 * tick and paint on the MoviePlayer's Slate loading thread: everything but the live status is fixed once built.
 */
struct FShinobiLoadingView
{
	FText Title;
	FText Mode;
	FText Blurb;
	TArray<FText> Tips;
	int32 FirstTip = 0;
	/** FPlatformTime seconds the view was built: the clock every animation runs on, so the MoviePlayer's copy of the
	 * screen and the viewport's agree (same tip, same zoom) when one hands over to the other. */
	double StartTime = 0.0;
	/** The map art (its texture is kept by the subsystem); null: a plain dark backdrop. */
	TSharedPtr<FSlateBrush> Art;
	/** How much the art is darkened under the text, 0-1. */
	float ArtDim = 0.25f;

	void SetStatus(const FText& InLabel, const FText& InDetail, float InProgress)
	{
		FScopeLock Guard(&Lock);
		Label = InLabel;
		Detail = InDetail;
		Progress = InProgress;
	}
	FText GetLabel() const { FScopeLock Guard(&Lock); return Label; }
	FText GetDetail() const { FScopeLock Guard(&Lock); return Detail; }
	float GetProgress() const { FScopeLock Guard(&Lock); return Progress; }

	double Elapsed() const { return FPlatformTime::Seconds() - StartTime; }
	int32 TipIndex() const
	{
		return Tips.IsEmpty() ? INDEX_NONE : (FirstTip + FMath::FloorToInt32(Elapsed() / ShinobiLoadingScreen::TipSeconds)) % Tips.Num();
	}

private:
	mutable FCriticalSection Lock;
	FText Label;
	FText Detail;
	float Progress = 0.f;
};

using FShinobiLoadingViewPtr = TSharedPtr<FShinobiLoadingView>;

/** Full-screen backdrop: the map art filling the screen with a slow push-in, darkened toward the text corners. */
class SShinobiLoadingBackdrop : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SShinobiLoadingBackdrop) {}
		SLATE_ARGUMENT(FShinobiLoadingViewPtr, View)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		View = InArgs._View;
		SetClipping(EWidgetClipping::ClipToBounds);
		ForceVolatile(true);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.f, 1080.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
		const FVector2D Size(LocalSize.X, LocalSize.Y);
		const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), WhiteBrush(), ESlateDrawEffect::None,
			FLinearColor(0.012f, 0.012f, 0.02f, 1.f) * Tint);
		++LayerId;

		const FSlateBrush* Art = View.IsValid() ? View->Art.Get() : nullptr;
		if (Art && Art->GetResourceObject() && Art->ImageSize.X > 0.f && Art->ImageSize.Y > 0.f && Size.X > 0.f && Size.Y > 0.f)
		{
			// Cover the screen, then push in from 103% to 110% over ~25 s while drifting a little sideways (a Ken Burns
			// move: the screen is never a frozen picture, even when nothing else on it changes).
			const double T = SmoothStep01(View->Elapsed() / 25.0);
			const FVector2D Image(Art->ImageSize.X, Art->ImageSize.Y);
			const double Scale = FMath::Max(Size.X / Image.X, Size.Y / Image.Y) * FMath::Lerp(1.03, 1.10, T);
			const FVector2D Drawn = Image * Scale;
			FVector2D Offset = (Size - Drawn) * 0.5;
			Offset.X += (Drawn.X - Size.X) * 0.5 * FMath::Lerp(0.35, -0.35, T);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(Drawn, FSlateLayoutTransform(1.f, Offset)),
				Art, ESlateDrawEffect::None, Tint);
			++LayerId;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), WhiteBrush(), ESlateDrawEffect::None,
				FLinearColor(0.f, 0.f, 0.f, View->ArtDim) * Tint);
			++LayerId;
		}
		else
		{
			// No art: a faint dusk glow low on the screen instead of flat black.
			PaintBands(AllottedGeometry, OutDrawElements, LayerId, Size, true, 0.55, 1.0, 0.f, 0.22f, ShinobiMenuStyle::Accent * FLinearColor(0.35f, 0.25f, 0.2f, 1.f), Tint);
			++LayerId;
		}

		// Readability: darken the bottom (title, tip, status) and the left (title column).
		PaintBands(AllottedGeometry, OutDrawElements, LayerId, Size, true, 0.4, 1.0, 0.f, 0.88f, FLinearColor::Black, Tint);
		++LayerId;
		PaintBands(AllottedGeometry, OutDrawElements, LayerId, Size, false, 0.0, 0.6, 0.55f, 0.f, FLinearColor::Black, Tint);
		++LayerId;
		// ...and a thin vignette at the top for the brand line.
		PaintBands(AllottedGeometry, OutDrawElements, LayerId, Size, true, 0.0, 0.18, 0.5f, 0.f, FLinearColor::Black, Tint);
		return LayerId;
	}

private:
	/** A linear gradient from AlphaFrom to AlphaTo across [From, To] of the height (bVertical) or width, as thin bands. */
	static void PaintBands(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FVector2D& Size,
		bool bVertical, double From, double To, float AlphaFrom, float AlphaTo, const FLinearColor& Color, const FLinearColor& Tint)
	{
		constexpr int32 Bands = 48;
		const double Length = (bVertical ? Size.Y : Size.X) * (To - From);
		const double Start = (bVertical ? Size.Y : Size.X) * From;
		const double Band = Length / Bands;
		for (int32 Index = 0; Index < Bands; ++Index)
		{
			const float Alpha = FMath::Lerp(AlphaFrom, AlphaTo, (Index + 0.5f) / Bands);
			if (Alpha <= 0.002f)
			{
				continue;
			}
			// One pixel of overlap so no seams show between bands.
			const FVector2D BandSize = bVertical ? FVector2D(Size.X, Band + 1.0) : FVector2D(Band + 1.0, Size.Y);
			const FVector2D BandPos = bVertical ? FVector2D(0.0, Start + Band * Index) : FVector2D(Start + Band * Index, 0.0);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(BandSize, FSlateLayoutTransform(1.f, BandPos)),
				WhiteBrush(), ESlateDrawEffect::None, Color.CopyWithNewOpacity(Alpha) * Tint);
		}
	}

	FShinobiLoadingViewPtr View;
};

/** The busy indicator: an accent arc that turns and breathes round a faint ring. It moves every frame, so a hitch
 * shows as a hitch rather than as a hang. */
class SShinobiLoadingSpinner : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SShinobiLoadingSpinner) {}
		SLATE_ARGUMENT(float, Diameter)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Diameter = InArgs._Diameter > 0.f ? InArgs._Diameter : 36.f;
		ForceVolatile(true);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Diameter, Diameter); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2f Size = AllottedGeometry.GetLocalSize();
		const FVector2f Centre = Size * 0.5f;
		const float Radius = FMath::Min(Size.X, Size.Y) * 0.5f - 2.f;
		const double Time = FPlatformTime::Seconds();
		const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

		TArray<FVector2f> Ring;
		constexpr int32 RingSegments = 48;
		for (int32 Index = 0; Index <= RingSegments; ++Index)
		{
			const float Angle = 2.f * PI * Index / RingSegments;
			Ring.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, 0.14f) * Tint, true, 2.f);

		const float Start = float(FMath::Fmod(Time * 1.6 * PI, 2.0 * PI));
		const float Sweep = PI * (0.75f + 0.45f * float(FMath::Sin(Time * 2.6)));
		TArray<FVector2f> Arc;
		constexpr int32 ArcSegments = 32;
		for (int32 Index = 0; Index <= ArcSegments; ++Index)
		{
			const float Angle = Start + Sweep * Index / ArcSegments;
			Arc.Add(Centre + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Arc, ESlateDrawEffect::None,
			ShinobiMenuStyle::Accent * Tint, true, 3.f);
		return LayerId + 1;
	}

private:
	float Diameter = 36.f;
};

/** A thin bar: eases toward the view's progress (never backwards), with a light sweeping along the filled part. */
class SShinobiLoadingBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SShinobiLoadingBar) {}
		SLATE_ARGUMENT(FShinobiLoadingViewPtr, View)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		View = InArgs._View;
		Shown = View.IsValid() ? View->GetProgress() : 0.f;
		ForceVolatile(true);
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(360.f, 3.f); }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		const float Target = View.IsValid() ? View->GetProgress() : 0.f;
		Shown = FMath::Max(Shown, FMath::FInterpTo(Shown, Target, FMath::Min(InDeltaTime, 0.1f), 3.f));
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2f LocalSize = AllottedGeometry.GetLocalSize();
		const FVector2D Size(LocalSize.X, LocalSize.Y);
		const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), WhiteBrush(), ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, 0.16f) * Tint);
		const double Filled = Size.X * FMath::Clamp(Shown, 0.f, 1.f);
		if (Filled > 0.5)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2D(Filled, Size.Y), FSlateLayoutTransform(1.f, FVector2D::ZeroVector)),
				WhiteBrush(), ESlateDrawEffect::None, ShinobiMenuStyle::Accent * Tint);
			constexpr double Glint = 70.0;
			const double Phase = FMath::Fmod(FPlatformTime::Seconds(), 1.6) / 1.6;
			const double From = FMath::Max(0.0, Phase * (Filled + Glint) - Glint);
			const double To = FMath::Min(Filled, Phase * (Filled + Glint));
			if (To - From > 0.5)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
					AllottedGeometry.ToPaintGeometry(FVector2D(To - From, Size.Y), FSlateLayoutTransform(1.f, FVector2D(From, 0.0))),
					WhiteBrush(), ESlateDrawEffect::None, FLinearColor(1.f, 0.92f, 0.8f, 0.55f) * Tint);
			}
		}
		return LayerId + 2;
	}

private:
	FShinobiLoadingViewPtr View;
	float Shown = 0.f;
};

/**
 * The screen: backdrop; bottom left the mode, the map's name, its blurb and a tip; bottom right what is being waited for,
 * the spinner and the bar; top left the game's name. Laid out for 1080p (the viewport's DPI scale does the rest).
 */
class SShinobiLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShinobiLoadingScreen) {}
		SLATE_ARGUMENT(FShinobiLoadingViewPtr, View)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		View = InArgs._View;
		check(View.IsValid());
		const FShinobiLoadingViewPtr V = View;
		const FSlateColor Dim(ShinobiMenuStyle::TextDim);
		const FVector2D Shadow(1.5f, 1.5f);
		const FLinearColor ShadowColor(0.f, 0.f, 0.f, 0.7f);

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SShinobiLoadingBackdrop).View(V)
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(80.f, 56.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Brand", "SHINOBI COOP"))
				.Font(LoadingFont(TEXT("BoldItalic"), 14, 260))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.55f)))
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(80.f, 0.f, 0.f, 72.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(V->Mode)
					.Font(LoadingFont(TEXT("BoldItalic"), 15, 180))
					.ColorAndOpacity(FSlateColor(ShinobiMenuStyle::Accent))
					.ShadowOffset(Shadow).ShadowColorAndOpacity(ShadowColor)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(V->Title.ToUpper())
					.Font(LoadingFont(TEXT("BoldItalic"), 56, 40))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(ShadowColor)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(96.f).HeightOverride(4.f)
					[
						SNew(SImage).Image(WhiteBrush()).ColorAndOpacity(FSlateColor(ShinobiMenuStyle::Accent))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(SBox).MaxDesiredWidth(640.f)
					.Visibility(V->Blurb.IsEmpty() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible)
					[
						SNew(STextBlock)
						.Text(V->Blurb)
						.Font(LoadingFont(TEXT("Regular"), 17))
						.ColorAndOpacity(Dim)
						.AutoWrapText(true)
						.ShadowOffset(Shadow).ShadowColorAndOpacity(ShadowColor)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 40.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					.Visibility(V->Tips.IsEmpty() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 1.f, 14.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Tip", "TIP"))
						.Font(LoadingFont(TEXT("BoldItalic"), 14, 160))
						.ColorAndOpacity(FSlateColor(ShinobiMenuStyle::Accent))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(580.f)
						[
							SNew(STextBlock)
							.Text_Lambda([V] { const int32 Index = V->TipIndex(); return Index == INDEX_NONE ? FText::GetEmpty() : V->Tips[Index]; })
							.ColorAndOpacity_Lambda([V] { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, TipOpacity(V->Elapsed()))); })
							.Font(LoadingFont(TEXT("Regular"), 16))
							.AutoWrapText(true)
							.ShadowOffset(Shadow).ShadowColorAndOpacity(ShadowColor)
						]
					]
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 80.f, 72.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 18.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text_Lambda([V] { return V->GetLabel(); })
							.Font(LoadingFont(TEXT("BoldItalic"), 16, 140))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
							.Justification(ETextJustify::Right)
							.ShadowOffset(Shadow).ShadowColorAndOpacity(ShadowColor)
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 2.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Text_Lambda([V] { return V->GetDetail(); })
							.Font(LoadingFont(TEXT("Regular"), 13))
							.ColorAndOpacity(Dim)
							.Justification(ETextJustify::Right)
							.ShadowOffset(Shadow).ShadowColorAndOpacity(ShadowColor)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SShinobiLoadingSpinner).Diameter(38.f)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 16.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(360.f).HeightOverride(3.f)
					[
						SNew(SShinobiLoadingBar).View(V)
					]
				]
			]
		];
	}

	/** Clicks stop here: nothing under the screen can be pressed while it's up. */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }

private:
	/** Each tip fades in and out over half a second; the first is up at once (a frame frozen by a blocking load shows it). */
	static float TipOpacity(double Elapsed)
	{
		constexpr double Fade = 0.5;
		const double Phase = FMath::Fmod(FMath::Max(Elapsed, 0.0), ShinobiLoadingScreen::TipSeconds);
		const double In = Elapsed < ShinobiLoadingScreen::TipSeconds ? 1.0 : Phase / Fade;
		const double Out = (ShinobiLoadingScreen::TipSeconds - Phase) / Fade;
		return float(FMath::Clamp(FMath::Min(In, Out), 0.0, 1.0));
	}

	FShinobiLoadingViewPtr View;
};

// ---- ShinobiLoadingScreen ----

ShinobiLoadingScreen::FStatus ShinobiLoadingScreen::Evaluate(const FChecks& Checks)
{
	FStatus Status;
	auto Set = [&Status](EStep Step, float Progress, const FText& Label, const FText& Detail = FText::GetEmpty())
	{
		Status.Step = Step;
		Status.Progress = Progress;
		Status.Label = Label;
		Status.Detail = Detail;
	};
	if (Checks.bConnecting)
	{
		Set(EStep::Connecting, 0.04f, LOCTEXT("Connecting", "CONNECTING"), LOCTEXT("ConnectingDetail", "Reaching the host"));
	}
	else if (Checks.bTravelling)
	{
		Set(EStep::LoadingMap, 0.12f, LOCTEXT("LoadingMap", "LOADING MAP"));
	}
	else if (!Checks.bWorldStarted)
	{
		Set(EStep::StartingGame, 0.5f, LOCTEXT("StartingGame", "STARTING GAME"));
	}
	else if (Checks.bWaitingForPawn)
	{
		Set(EStep::StartingGame, 0.55f, LOCTEXT("StartingGame", "STARTING GAME"), LOCTEXT("WaitingForPawn", "Placing your shinobi"));
	}
	else if (Checks.LevelsStreaming > 0)
	{
		Set(EStep::StreamingWorld, 0.62f, LOCTEXT("StreamingWorld", "STREAMING WORLD"),
			FText::Format(LOCTEXT("AreasLeft", "{0} {0}|plural(one=area,other=areas) left"), FText::AsNumber(Checks.LevelsStreaming)));
	}
	else if (Checks.ShadersCompiling > 0)
	{
		const int32 Peak = FMath::Max(Checks.ShadersPeak, Checks.ShadersCompiling);
		const float Done = 1.f - float(Checks.ShadersCompiling) / float(Peak);
		Set(EStep::PreparingShaders, 0.7f + 0.18f * Done, LOCTEXT("PreparingShaders", "COMPILING SHADERS"),
			FText::Format(LOCTEXT("ShadersLeft", "{0} left"), FText::AsNumber(Checks.ShadersCompiling)));
	}
	else if (Checks.CharactersBuilding > 0)
	{
		Set(EStep::PreparingCharacters, 0.9f, LOCTEXT("PreparingCharacters", "PREPARING CHARACTERS"),
			FText::Format(LOCTEXT("CharactersLeft", "{0} {0}|plural(one=body,other=bodies) building"), FText::AsNumber(Checks.CharactersBuilding)));
	}
	else if (Checks.TexturesStreaming > 0 && !Checks.bTexturesTimedOut)
	{
		Set(EStep::StreamingTextures, 0.95f, LOCTEXT("StreamingTextures", "STREAMING TEXTURES"));
	}
	else
	{
		Set(EStep::Ready, 1.f, LOCTEXT("Ready", "READY"));
	}
	return Status;
}

TArray<FText> ShinobiLoadingScreen::Tips(bool bArena)
{
	using ShinobiKeyBindings::EBindable;
	// The player's own bindings, keyboard then controller: "Left Ctrl / B".
	auto Key = [](EBindable Which)
	{
		const FText Keyboard = ShinobiKeyBindings::KeyLabel(ShinobiKeyBindings::Get(Which));
		const ShinobiKeyBindings::FPadBinding Pad = ShinobiKeyBindings::GetPad(Which);
		return Pad.IsBound() ? FText::Format(LOCTEXT("KeyOrPad", "{0} / {1}"), Keyboard, ShinobiKeyBindings::PadLabel(Pad)) : Keyboard;
	};
	FFormatNamedArguments Keys;
	Keys.Add(TEXT("Dodge"), Key(EBindable::Dodge));
	Keys.Add(TEXT("Block"), Key(EBindable::Block));
	Keys.Add(TEXT("ChakraCharge"), Key(EBindable::ChakraCharge));
	Keys.Add(TEXT("Sprint"), Key(EBindable::Sprint));
	Keys.Add(TEXT("LockOn"), Key(EBindable::LockOn));
	Keys.Add(TEXT("SkillsScreen"), Key(EBindable::SkillsScreen));
	Keys.Add(TEXT("Map"), Key(EBindable::Map));
	Keys.Add(TEXT("Revive"), Key(EBindable::Revive));

	TArray<FText> Formats = {
		LOCTEXT("TipSubstitution", "Dodge ({Dodge}) just as a hit lands to Substitution: no damage, a log left behind, and you reappear behind your attacker, who staggers. Each one spends a charge."),
		LOCTEXT("TipBlock", "Hold {Block} to raise a Susanoo ribcage. Hits cost stamina instead of most of their damage, but you stand still behind it."),
		LOCTEXT("TipChakra", "Low on chakra? Hold {ChakraCharge} to charge it back. A staggering hit breaks the charge, so find some room first."),
		LOCTEXT("TipSprint", "Sprinting ({Sprint}) drains stamina, and stamina waits a moment before it refills. Save some for a dodge."),
		LOCTEXT("TipLockOn", "{LockOn} locks on to the living enemy nearest your crosshair. Press it again to let go."),
		LOCTEXT("TipLoadout", "Open the jutsu loadout with {SkillsScreen} to choose which jutsu sit in your eight slots."),
		LOCTEXT("TipMap", "{Map} opens the full map. The minimap in the corner follows you everywhere else."),
		LOCTEXT("TipRebind", "Every action can be rebound, on keyboard and controller, under CONTROLS in the menu."),
	};
	if (bArena)
	{
		Formats.Append({
			LOCTEXT("TipArenaRounds", "The first team to win two rounds takes the match. After the result, the arena reloads for a rematch."),
			LOCTEXT("TipArenaDowned", "Nobody can revive you in the arena: downed is out until the next round."),
		});
	}
	else
	{
		Formats.Append({
			LOCTEXT("TipRevive", "A downed teammate can be saved: stand next to them and hold {Revive} for 3 seconds. They get back up at 30% health."),
			LOCTEXT("TipVillagers", "Villagers have something to say. Walk up to one and press {Revive}."),
			LOCTEXT("TipInvite", "Invite friends from the pause menu: GAME, then INVITE FRIEND."),
		});
	}
	TArray<FText> Out;
	for (const FText& Format : Formats)
	{
		Out.Add(FText::Format(Format, Keys));
	}
	return Out;
}

int32 ShinobiLoadingScreen::FindMapByPath(const FString& PackagePath)
{
	FString Name = PackagePath;
	int32 Dot = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), Dot))
	{
		Name.LeftInline(Dot);
	}
	Name = UWorld::RemovePIEPrefix(FPackageName::GetShortName(Name));
	if (Name.IsEmpty())
	{
		return INDEX_NONE;
	}
	return UShinobiSessionSubsystem::GetMaps().IndexOfByPredicate([&Name](const FShinobiMap& Map)
	{
		return Name.Equals(FPackageName::GetShortName(Map.Path), ESearchCase::IgnoreCase);
	});
}

// ---- UShinobiLoadingScreenSubsystem ----

bool UShinobiLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UShinobiLoadingScreenSubsystem::IsEnabled()
{
	static const bool bOffByCommandLine = FParse::Param(FCommandLine::Get(), TEXT("NoLoadingScreen"));
	return CVarShinobiLoadingScreen.GetValueOnGameThread() && !bOffByCommandLine && !FApp::IsUnattended() && FApp::CanEverRender()
		&& !GIsAutomationTesting && !IsRunningCommandlet();
}

void UShinobiLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UShinobiLoadingScreenSubsystem::OnPreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UShinobiLoadingScreenSubsystem::OnPostLoadMap);
	// The game starting: in PIE its world is already here (and loads no map, so no PreLoadMap); in a game build the
	// first map's PreLoadMap follows at once and picks the right art.
	const UWorld* World = GetGameInstance()->GetWorld();
	Begin(World ? World->GetOutermost()->GetName() : FString());
}

void UShinobiLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	HideWidget();
	bShowing = false;
	View.Reset();
	Super::Deinitialize();
}

void UShinobiLoadingScreenSubsystem::Begin(const FString& InMapPath, const FString& InFormat)
{
	if (!IsEnabled())
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	// The same map as last time keeps its format when none is given: a rematch's PreLoadMap doesn't know it.
	const bool bSameMap = View.IsValid() && ShinobiLoadingScreen::FindMapByPath(InMapPath) == ShinobiLoadingScreen::FindMapByPath(MapPath);
	const FString NewFormat = bSameMap && InFormat.IsEmpty() ? Format : InFormat;
	// A screen already up for this map stays as it is (a host's Begin, then that load's PreLoadMap).
	const bool bRebuild = !bShowing || !bSameMap || NewFormat != Format;
	MapPath = InMapPath;
	Format = NewFormat;
	if (!bShowing)
	{
		ShownAt = Now;
		ShadersPeak = 0;
		bLoggedTimeout = false;
		UE_LOG(LogShinobiCoop, Log, TEXT("Loading screen: up for %s"), MapPath.IsEmpty() ? TEXT("(start-up)") : *MapPath);
	}
	bShowing = true;
	bFading = false;
	LoadedAt = StartedAt = AlmostReadyAt = ReadyAt = 0.0;
	if (bRebuild)
	{
		BuildView();
	}
	ShowWidget();
	if (Widget.IsValid())
	{
		Widget->SetRenderOpacity(1.f);
	}
	// Until the next tick looks at the world: travel is what's under way.
	ShinobiLoadingScreen::FChecks Travelling;
	Travelling.bTravelling = true;
	Status = ShinobiLoadingScreen::Evaluate(Travelling);
	View->SetStatus(Status.Label, Status.Detail, Status.Progress);
}

void UShinobiLoadingScreenSubsystem::BuildView()
{
	using namespace ShinobiLoadingScreen;
	const int32 MapIndex = FindMapByPath(MapPath);
	const FShinobiMap* Map = MapIndex != INDEX_NONE ? &UShinobiSessionSubsystem::GetMaps()[MapIndex] : nullptr;
	const TSharedRef<FShinobiLoadingView> NewView = MakeShared<FShinobiLoadingView>();
	if (Map)
	{
		FString Title = Map->DisplayName;
		Title.RemoveFromStart(TEXT("Arena: "));
		NewView->Title = FText::FromString(Title);
		NewView->Blurb = FText::FromString(Map->Blurb);
		if (Map->bArena)
		{
			NewView->Mode = Format.IsEmpty() ? LOCTEXT("ModeArena", "PVP ARENA")
				: FText::Format(LOCTEXT("ModeArenaFormat", "PVP ARENA  ·  {0}"), FText::FromString(Format.ToUpper()));
		}
		else
		{
			NewView->Mode = Map->bTest ? LOCTEXT("ModeTest", "TEST LEVEL")
				: FText::Format(LOCTEXT("ModeCoop", "CO-OP  ·  UP TO {0} PLAYERS"), FText::AsNumber(UShinobiSessionSubsystem::MaxPlayers));
		}
		Art = ShinobiMenuArt::LoadArt(ShinobiMenuArt::MapImagePath(*Map));
		NewView->ArtDim = 0.2f;
	}
	else
	{
		// The main menu (or start-up): the first map's picture as key art, darker.
		NewView->Title = LOCTEXT("MenuTitle", "Shinobi Coop");
		NewView->Mode = LOCTEXT("ModeMenu", "MAIN MENU");
		Art = ShinobiMenuArt::LoadArt(ShinobiMenuArt::MapImagePath(UShinobiSessionSubsystem::GetMaps()[0]));
		NewView->ArtDim = 0.45f;
	}
	if (Art)
	{
		NewView->Art = MakeShared<FSlateBrush>(ShinobiMenuStyle::TextureBrush(Art, FVector2D(Art->GetSizeX(), Art->GetSizeY())));
	}
	NewView->Tips = Tips(Map && Map->bArena);
	NewView->FirstTip = NewView->Tips.IsEmpty() ? 0 : FMath::RandRange(0, NewView->Tips.Num() - 1);
	NewView->StartTime = FPlatformTime::Seconds();
	if (View.IsValid())
	{
		NewView->SetStatus(View->GetLabel(), View->GetDetail(), View->GetProgress());
	}
	View = NewView;

	// The old widget showed the old view: a new one replaces it.
	HideWidget();
}

void UShinobiLoadingScreenSubsystem::ShowWidget()
{
	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!Viewport || !View.IsValid())
	{
		return; // no viewport yet (start-up): Tick adds it once there is one
	}
	if (Widget.IsValid() && WidgetViewport.Get() == Viewport && Widget->GetParentWidget().IsValid())
	{
		return;
	}
	if (Widget.IsValid() && WidgetViewport.IsValid())
	{
		WidgetViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	if (!Widget.IsValid())
	{
		Widget = SNew(SShinobiLoadingScreen).View(View);
	}
	Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), LoadingScreenZOrder);
	WidgetViewport = Viewport;
	SetInputBlocked(!bFading);
}

void UShinobiLoadingScreenSubsystem::HideWidget()
{
	if (Widget.IsValid() && WidgetViewport.IsValid())
	{
		WidgetViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	Widget.Reset();
	WidgetViewport.Reset();
	SetInputBlocked(false);
}

void UShinobiLoadingScreenSubsystem::SetInputBlocked(bool bBlocked)
{
	// Not in the editor: the preprocessor is global, and would hold the editor's keys (Shift+F1, Esc) too.
	if (GIsEditor || !FSlateApplication::IsInitialized())
	{
		return;
	}
	if (bBlocked && !InputBlocker.IsValid())
	{
		InputBlocker = MakeShared<FShinobiLoadingInputBlocker>();
		FSlateApplication::Get().RegisterInputPreProcessor(InputBlocker);
	}
	else if (!bBlocked && InputBlocker.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputBlocker);
		InputBlocker.Reset();
	}
}

void UShinobiLoadingScreenSubsystem::OnPreLoadMap(const FWorldContext& Context, const FString& MapName)
{
	if (Context.OwningGameInstance != GetGameInstance() || !IsEnabled())
	{
		return;
	}
	bInLoadMap = true;
	Begin(MapName);

	// The engine blocks in the load from here on. In a game build the MoviePlayer draws the screen on the Slate loading
	// thread meanwhile, so the spinner, tips and art keep moving (not in the editor: PIE has no MoviePlayer).
	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
	if (!GIsEditor && MoviePlayer && IsMoviePlayerEnabled() && View.IsValid())
	{
		FLoadingScreenAttributes Attributes;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		Attributes.bMoviesAreSkippable = false;
		Attributes.bWaitForManualStop = false;
		Attributes.MinimumLoadingScreenDisplayTime = -1.f;
		Attributes.WidgetLoadingScreen = SNew(SShinobiLoadingScreen).View(View);
		MoviePlayer->SetupLoadingScreen(Attributes);
		// The MoviePlayer's own PreLoadMap handler may already have run (finding nothing to play): start it here, and
		// finish it in OnPostLoadMap. If it is already playing, the MoviePlayer finishes it itself.
		if (!MoviePlayer->IsMovieCurrentlyPlaying())
		{
			bOwnMovie = MoviePlayer->PlayMovie();
		}
	}
	// No MoviePlayer: draw the screen once now, so the frame frozen through the load is the screen, not the old map.
	if (!GIsEditor && !(MoviePlayer && MoviePlayer->IsMovieCurrentlyPlaying()) && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().Tick();
	}
}

void UShinobiLoadingScreenSubsystem::OnPostLoadMap(UWorld* World)
{
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	bInLoadMap = false;
	if (bOwnMovie)
	{
		bOwnMovie = false;
		IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
		if (MoviePlayer && MoviePlayer->IsMovieCurrentlyPlaying())
		{
			MoviePlayer->WaitForMovieToFinish();
		}
	}
	if (bShowing)
	{
		ShowWidget(); // the MoviePlayer hands back to the viewport: the screen stays up there until the world is ready
	}
}

ShinobiLoadingScreen::FChecks UShinobiLoadingScreenSubsystem::GatherChecks()
{
	using namespace ShinobiLoadingScreen;
	const double Now = FPlatformTime::Seconds();
	FChecks Checks;
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance->GetWorld();
	const FWorldContext* Context = GameInstance->GetWorldContext();

	Checks.bConnecting = Context && Context->PendingNetGame != nullptr;
	Checks.bTravelling = bInLoadMap || Checks.bConnecting || !World || (Context && !Context->TravelURL.IsEmpty())
		|| !World->NextURL.IsEmpty() || World->IsInSeamlessTravel();
	if (Checks.bTravelling)
	{
		return Checks;
	}

	Checks.bWorldStarted = World->HasBegunPlay() && World->GetGameState() != nullptr;
	if (Checks.bWorldStarted && StartedAt == 0.0)
	{
		StartedAt = Now;
	}
	const bool bMenu = World->GetOutermost()->GetName().EndsWith(TEXT("Lvl_MainMenu"));
	if (Checks.bWorldStarted && !bMenu)
	{
		const APlayerController* Controller = GameInstance->GetFirstLocalPlayerController(World);
		Checks.bWaitingForPawn = (!Controller || !Controller->GetPawn()) && Now - StartedAt < PawnWaitSeconds;
	}

	for (const ULevelStreaming* Level : World->GetStreamingLevels())
	{
		if (Level && Level->IsStreamingStatePending())
		{
			++Checks.LevelsStreaming;
		}
	}

	// Rendering the world behind the screen is what compiles these; the screen just waits for them to run out.
	Checks.ShadersCompiling = int32(FShaderPipelineCache::NumPrecompilesRemaining()) + int32(PipelineStateCache::NumActivePrecacheRequests());
#if WITH_EDITOR
	if (GShaderCompilingManager)
	{
		Checks.ShadersCompiling += GShaderCompilingManager->GetNumRemainingJobs();
	}
#endif
	ShadersPeak = FMath::Max(ShadersPeak, Checks.ShadersCompiling);
	Checks.ShadersPeak = ShadersPeak;

	for (TObjectIterator<UShinobiBodyComponent> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetWorld() == World && It->IsMutableBuildPending())
		{
			++Checks.CharactersBuilding;
		}
	}

	Checks.TexturesStreaming = IStreamingManager::Get().GetNumWantingResources();
	Checks.bTexturesTimedOut = AlmostReadyAt > 0.0 && Now - AlmostReadyAt > TextureHoldSeconds;
	return Checks;
}

void UShinobiLoadingScreenSubsystem::UpdateStatus(double Now)
{
	using namespace ShinobiLoadingScreen;
	const FChecks Checks = GatherChecks();
	Status = Evaluate(Checks);

	if (Checks.bTravelling)
	{
		LoadedAt = StartedAt = 0.0;
	}
	else if (LoadedAt == 0.0)
	{
		LoadedAt = Now;
	}
	if (Status.Step >= EStep::StreamingTextures)
	{
		AlmostReadyAt = AlmostReadyAt > 0.0 ? AlmostReadyAt : Now;
	}
	else
	{
		AlmostReadyAt = 0.0;
	}

	// Safety nets: never hold a loaded game forever on something that doesn't finish (logged, so it can be looked at);
	// and never hold at all for much longer than any connect or load could take.
	const bool bTimedOut = (LoadedAt > 0.0 && Now - LoadedAt > MaxHoldSeconds) || Now - ShownAt > MaxHoldSeconds * 3.0;
	if (bTimedOut && Status.Step != EStep::Ready)
	{
		if (!bLoggedTimeout)
		{
			bLoggedTimeout = true;
			UE_LOG(LogShinobiCoop, Warning, TEXT("Loading screen: gave up waiting on '%s' (%s) after %.0f s"),
				*Status.Label.ToString(), *Status.Detail.ToString(), Now - ShownAt);
		}
		FChecks Done;
		Done.bWorldStarted = true;
		Status = Evaluate(Done);
	}

	if (Status.Step == EStep::Ready)
	{
		ReadyAt = ReadyAt > 0.0 ? ReadyAt : Now;
	}
	else
	{
		ReadyAt = 0.0;
	}
	View->SetStatus(Status.Label, Status.Detail, Status.Progress);

	if (!bFading && ReadyAt > 0.0 && Now - ReadyAt >= SettleSeconds && Now - ShownAt >= MinShowSeconds)
	{
		bFading = true;
		FadeFrom = Now;
		SetInputBlocked(false); // the game is the player's again while the screen fades
		UE_LOG(LogShinobiCoop, Log, TEXT("Loading screen: ready after %.1f s"), Now - ShownAt);
	}
}

void UShinobiLoadingScreenSubsystem::Tick(float DeltaTime)
{
	if (!bShowing || !View.IsValid())
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	ShowWidget(); // the first one at start-up, or back after anything took it off the viewport
	UpdateStatus(Now);
	if (!bFading)
	{
		return;
	}
	if (Status.Step != ShinobiLoadingScreen::EStep::Ready)
	{
		// Something new to wait for (travel again): back up.
		bFading = false;
		SetInputBlocked(true);
		if (Widget.IsValid())
		{
			Widget->SetRenderOpacity(1.f);
		}
		return;
	}
	const float Opacity = 1.f - float(FMath::Clamp((Now - FadeFrom) / ShinobiLoadingScreen::FadeSeconds, 0.0, 1.0));
	if (Widget.IsValid())
	{
		Widget->SetRenderOpacity(Opacity * Opacity); // ease out: most of the fade is in the first half
	}
	if (Opacity <= 0.f)
	{
		HideWidget();
		bShowing = false;
		bFading = false;
	}
}

ETickableTickType UShinobiLoadingScreenSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UShinobiLoadingScreenSubsystem::IsTickable() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return bShowing && GameInstance && GameInstance->GetGameViewportClient();
}

TStatId UShinobiLoadingScreenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UShinobiLoadingScreenSubsystem, STATGROUP_Tickables);
}

UWorld* UShinobiLoadingScreenSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

#undef LOCTEXT_NAMESPACE
