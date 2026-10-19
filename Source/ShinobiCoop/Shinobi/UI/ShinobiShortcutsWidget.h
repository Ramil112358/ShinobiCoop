#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiShortcutsWidget.generated.h"

class UShinobiIndexButton;
class UTextBlock;
class UVerticalBox;

/**
 * Settings > Shortcuts: the keyboard/mouse key of each jutsu slot and core action (ShinobiKeyBindings). Click a key, then
 * press the new one (Alt allowed); a key already in use swaps over. Reset to defaults; Back returns to Settings.
 */
UCLASS()
class SHINOBICOOP_API UShinobiShortcutsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The menu hidden underneath, shown again on Back. */
	TWeakObjectPtr<UUserWidget> Opener;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	void AddRow(UVerticalBox* Column, int32 Bindable);
	void OnKeyChip(int32 Bindable);
	void OnFooter(int32 Index);
	/** Rebinds the listening row to Key (Alt held or not) and says what happened. */
	void Bind(const FKey& Key, bool bAlt);
	void RefreshChips();
	void SetMessage(const FString& Text, const FLinearColor& Color);

	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> Chips;
	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;

	int32 Listening = INDEX_NONE;
};
