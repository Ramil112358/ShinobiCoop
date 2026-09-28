#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiHUDWidget.generated.h"

class UAbilitySystemComponent;
class UBorder;
class UCanvasPanel;
class UGameplayAbility;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
struct FGameplayTagContainer;

/**
 * Health / chakra / stamina bars, Substitution pips and the four jutsu slots; the arena's wave line, teammates,
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
	UProgressBar* AddBar(UVerticalBox* Box, FName Name, const FLinearColor& Color, float Height);
	void BuildSubstitutionPips(UVerticalBox* Bars);
	void BuildJutsuSlots(UCanvasPanel* Root);
	UTextBlock* AddLabel(UOverlay* Layers, int32 FontSize, EHorizontalAlignment HorizontalAlign, EVerticalAlignment VerticalAlign, const FLinearColor& Tint);
	void UpdateJutsuSlots(const UAbilitySystemComponent& ASC);
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
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> ChakraBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SubstitutionPips;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotFrames;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotGlyphs;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCosts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCooldownTexts;

	/** Dark overlay whose height is the remaining share of the cooldown. */
	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> SlotShades;

	/** World time until which each slot shows the red flash. */
	TArray<double> SlotFlashUntil;

	/** For the wave-start gong and the victory chime. */
	uint8 LastPhase = 0;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle FailedHandle;
};
