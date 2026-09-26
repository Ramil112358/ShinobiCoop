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

/** Health / chakra / stamina bars, Substitution pips and the four jutsu slots. Built in code; polled each tick. */
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

	/** Binds the red "can't cast" flash to the current pawn's ability system; rebinds when the pawn changes. */
	void BindFailureFlash(UAbilitySystemComponent* ASC);
	void OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Reasons);

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

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle FailedHandle;
};
