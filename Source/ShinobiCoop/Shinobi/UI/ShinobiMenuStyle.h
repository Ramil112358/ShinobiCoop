#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UBorder;
class UButton;
class UTextBlock;
class UTexture2D;
class UWidgetTree;

/**
 * The menus' look (docs/superpowers/specs/2026-10-20-main-menu-polish-design.md): dark translucent panels, one dusk-orange
 * accent, italic caps headings, parallelogram buttons and cards (a render-transform shear on the frame, undone on its content).
 */
namespace ShinobiMenuStyle
{
	SHINOBICOOP_API extern const FLinearColor Accent;
	SHINOBICOOP_API extern const FLinearColor Panel;
	SHINOBICOOP_API extern const FLinearColor TextDim;
	/** Horizontal shear of buttons and cards, in degrees; content gets the opposite. */
	constexpr float Shear = -12.f;

	/** Bold italic caps when bHeading. */
	SHINOBICOOP_API UTextBlock* Text(UWidgetTree& Tree, const FText& Text, int32 Size, const FLinearColor& Color, bool bHeading = false);
	/** Sheared panel button, accent on hover and focus; primary = filled accent with black text. */
	SHINOBICOOP_API UButton* Button(UWidgetTree& Tree, const FText& Label, int32 Size, bool bPrimary = false);
	/** Gives an existing button (and its text child, if any) the Button look. */
	SHINOBICOOP_API void StyleButton(UButton& Button, bool bPrimary);
	/** Call every tick: lights the focused styled button in Tree (FButtonStyle has no focused state). */
	SHINOBICOOP_API void TickFocus(UWidgetTree& Tree);
	/** Translucent dark panel with a faint accent outline. */
	SHINOBICOOP_API UBorder* PanelBox(UWidgetTree& Tree, const FMargin& Padding);
	SHINOBICOOP_API FSlateBrush TextureBrush(UTexture2D* Texture, const FVector2D& Size);
	/** A flat colour brush (tinted white box). */
	SHINOBICOOP_API FSlateBrush ColorBrush(const FLinearColor& Color);
}
