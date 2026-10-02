#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiHUDWidget.generated.h"

class UAbilitySystemComponent;
class UBorder;
class UCanvasPanel;
class UGameplayAbility;
class UImage;
class UOverlay;
class UProgressBar;
class UShinobiJutsuAbility;
class USizeBox;
class UTextBlock;
class UVerticalBox;
struct FGameplayTagContainer;

/**
 * Bottom-left unit frame (portrait, name, health / chakra / stamina bars, Substitution pips) and the four jutsu
 * slots (painted icon, key, chakra cost, name); the arena's wave line, teammates,
 * banners (downed, dead, wipe, victory), revive progress and lock-on. Built in code; polled each tick.
 */
UCLASS()
class SHINOBICOOP_API UShinobiHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 NumJutsuSlots = 4;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void BuildUnitFrame(UCanvasPanel* Root);
	/** A fixed-size row in Bars for one bar and its overlays (the trail, the numbers). */
	UOverlay* AddBarRow(UVerticalBox* Bars, float Width, float Height);
	UProgressBar* AddBar(UOverlay* Row, FName Name, const FLinearColor& Color, bool bBacked);
	void BuildSubstitutionPips(UVerticalBox* Bars);
	void BuildStatusIcons(UVerticalBox* Bars);
	void BuildJutsuSlots(UCanvasPanel* Root);
	UTextBlock* MakeText(int32 FontSize, const FLinearColor& Tint, bool bBold = false);
	UTextBlock* AddLabel(UOverlay* Layers, int32 FontSize, EHorizontalAlignment HorizontalAlign, EVerticalAlignment VerticalAlign, const FLinearColor& Tint);
	/** The slots show the jutsu on Page (the player's current jutsu page); slots with none on it are hidden. */
	void UpdateJutsuSlots(const UAbilitySystemComponent& ASC, int32 Page, const class AShinobiPlayerCharacter* Player);
	/** One small diamond per jutsu page under the slots; the current page is lit. */
	void BuildPagePips(UCanvasPanel* Root);
	UTextBlock* AddCanvasText(UCanvasPanel* Root, FName Name, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position, int32 FontSize, ETextJustify::Type Justify);
	void UpdateArena(const class AShinobiPlayerCharacter* Player);
	/** Open world: the compass strip (cardinal points, village, camps, boss) around the camera's yaw. */
	void UpdateCompass();
	UTextBlock* CompassLabel(int32 Index);

	/** Binds the red "can't cast" flash to the current pawn's ability system; rebinds when the pawn changes. */
	void BindFailureFlash(UAbilitySystemComponent* ASC);
	void OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Reasons);

	/** Top-centre: wave, phase countdown, enemies left. */
	UPROPERTY()
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UBorder> CompassBar;

	/** Pooled compass labels, positioned every tick; unused ones are hidden. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CompassLabels;

	/** Top-left: teammates' names and health. */
	UPROPERTY()
	TObjectPtr<UTextBlock> TeamText;

	/** Centre: downed / dead / wipe / victory. */
	UPROPERTY()
	TObjectPtr<UTextBlock> BannerText;

	/** Below the centre: revive progress, lock-on. */
	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar;

	/** Behind the health bar: holds the pre-damage level for a moment, then drains down to the health. */
	UPROPERTY()
	TObjectPtr<UProgressBar> HealthTrailBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChakraText;

	UPROPERTY()
	TObjectPtr<UProgressBar> ChakraBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SubstitutionPips;

	/** The SpriteCook pip art (T_Hud_PipOn / PipOff); the pips fall back to tinted diamonds while it's missing. */
	UPROPERTY()
	FSlateBrush PipOnBrush;

	UPROPERTY()
	FSlateBrush PipOffBrush;

	/** Status medallions under the pips (burn, black flame, shock, stagger), shown while the player carries that state. */
	UPROPERTY()
	TArray<TObjectPtr<UImage>> StatusIcons;

	/** True once the SpriteCook slot frame texture is loaded: the frames then flash by tint instead of a redrawn outline. */
	bool bFrameArt = false;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotFrames;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> SlotIcons;

	/** The jutsu's name inside the slot while its icon texture is missing. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotFallbacks;

	/** Under each slot. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotNames;

	/** Each slot's column (frame + name): hidden when the current page has no jutsu there. */
	UPROPERTY()
	TArray<TObjectPtr<UVerticalBox>> SlotColumns;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> PagePips;

	/** The jutsu each slot shows; its icon, name and cost are set when it changes. */
	TArray<TWeakObjectPtr<const UShinobiJutsuAbility>> SlotJutsu;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCosts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCooldownTexts;

	/** Dark overlay whose height is the remaining share of the cooldown. */
	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> SlotShades;

	float HealthTrail = 1.f;
	float LastHealthRatio = 1.f;
	double TrailHoldUntil = 0.0;

	/** World time until which each slot shows the red flash. */
	TArray<double> SlotFlashUntil;

	/** For the wave-start gong and the victory chime. */
	uint8 LastPhase = 0;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle FailedHandle;
};
