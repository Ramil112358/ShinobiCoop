#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

const FLinearColor ShinobiMenuStyle::Accent = FLinearColor::FromSRGBColor(FColor(255, 122, 26));
const FLinearColor ShinobiMenuStyle::Panel(0.02f, 0.02f, 0.04f, 0.78f);
const FLinearColor ShinobiMenuStyle::TextDim(0.7f, 0.7f, 0.75f);

namespace
{
	FSlateBrush MenuStyleBox(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Fill;
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(2.f, 2.f, 2.f, 2.f), FSlateColor(Outline), OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	FButtonStyle MenuStyleButtonStyle(bool bPrimary, bool bFocused)
	{
		using namespace ShinobiMenuStyle;
		const FLinearColor Hot = bPrimary ? FLinearColor(1.f, 0.62f, 0.3f) : Accent.CopyWithNewOpacity(0.4f);
		const FSlateBrush Normal = bPrimary ? MenuStyleBox(Accent, Accent, 0.f) : MenuStyleBox(Panel, FLinearColor(1.f, 1.f, 1.f, 0.12f), 1.f);
		const FSlateBrush Hovered = MenuStyleBox(Hot, Accent, 2.f);
		FButtonStyle Style;
		Style.SetNormal(bFocused ? Hovered : Normal);
		Style.SetHovered(Hovered);
		// Only a primary button's pressed fill is opaque: TickFocus tells the two kinds apart by it.
		Style.SetPressed(MenuStyleBox(bPrimary ? Accent * FLinearColor(0.8f, 0.8f, 0.8f, 1.f) : Accent.CopyWithNewOpacity(0.6f), Accent, 2.f));
		Style.SetDisabled(MenuStyleBox(Panel.CopyWithNewOpacity(0.35f), FLinearColor(1.f, 1.f, 1.f, 0.05f), 1.f));
		Style.SetNormalPadding(FMargin(18.f, 8.f));
		Style.SetPressedPadding(FMargin(18.f, 9.f, 18.f, 7.f));
		return Style;
	}
}

UTextBlock* ShinobiMenuStyle::Text(UWidgetTree& Tree, const FText& Text, int32 Size, const FLinearColor& Color, bool bHeading)
{
	UTextBlock* Block = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	if (bHeading)
	{
		Font.TypefaceFontName = TEXT("BoldItalic"); // engine Roboto has it; an unknown face falls back to the default one
		Font.LetterSpacing = 60;
	}
	Block->SetFont(Font);
	Block->SetText(bHeading ? Text.ToUpper() : Text);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowOffset(FVector2D(1.f, 1.f));
	Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	return Block;
}

UButton* ShinobiMenuStyle::Button(UWidgetTree& Tree, const FText& Label, int32 Size, bool bPrimary)
{
	UButton* Result = Tree.ConstructWidget<UButton>(UButton::StaticClass());
	Result->AddChild(Text(Tree, Label, Size, FLinearColor::White, bPrimary));
	StyleButton(*Result, bPrimary);
	return Result;
}

void ShinobiMenuStyle::StyleButton(UButton& Button, bool bPrimary)
{
	Button.SetStyle(MenuStyleButtonStyle(bPrimary, false));
	Button.SetRenderShear(FVector2D(Shear, 0.f));
	if (UWidget* Content = Button.GetChildAt(0))
	{
		Content->SetRenderShear(FVector2D(-Shear, 0.f));
		if (UTextBlock* Label = Cast<UTextBlock>(Content))
		{
			Label->SetColorAndOpacity(FSlateColor(bPrimary ? FLinearColor::Black : FLinearColor::White));
		}
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Content->Slot))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}
}

void ShinobiMenuStyle::TickFocus(UWidgetTree& Tree)
{
	Tree.ForEachWidget([](UWidget* Widget)
	{
		UButton* Button = Cast<UButton>(Widget);
		if (!Button)
		{
			return;
		}
		// Gamepad and keyboard users see the focused button lit like a hovered one (FButtonStyle has no focused state).
		const bool bPrimary = Button->GetStyle().Pressed.TintColor.GetSpecifiedColor().A > 0.99f;
		const bool bFocused = Button->HasAnyUserFocus();
		const FButtonStyle Wanted = MenuStyleButtonStyle(bPrimary, bFocused);
		if (!(Button->GetStyle().Normal == Wanted.Normal))
		{
			Button->SetStyle(Wanted);
		}
	});
}

UBorder* ShinobiMenuStyle::PanelBox(UWidgetTree& Tree, const FMargin& Padding)
{
	UBorder* Box = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrush(MenuStyleBox(Panel, Accent.CopyWithNewOpacity(0.25f), 1.f));
	Box->SetPadding(Padding);
	return Box;
}

FSlateBrush ShinobiMenuStyle::TextureBrush(UTexture2D* Texture, const FVector2D& Size)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = Size;
	Brush.SetResourceObject(Texture);
	return Brush;
}

FSlateBrush ShinobiMenuStyle::ColorBrush(const FLinearColor& Color)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.TintColor = Color;
	return Brush;
}
