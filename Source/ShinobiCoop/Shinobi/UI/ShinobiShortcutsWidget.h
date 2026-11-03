#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiShortcutsWidget.generated.h"

class UShinobiIndexButton;
class UTextBlock;
class UVerticalBox;

/**
 * The CONTROLS tab (main menu and pause menu): the keyboard/mouse key of each jutsu slot and core action
 * (ShinobiKeyBindings). Click a key, then press the new one (Alt allowed); a key already in use swaps over. Reset to
 * defaults puts every key back.
 */
UCLASS()
class SHINOBICOOP_API UShinobiShortcutsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Where the panel's centre sits on its page (0-1). Set before it's built. */
	FVector2D PanelAnchor = FVector2D(0.5f, 0.5f);

	/** Slot 1's key: focus goes there when the tab opens. */
	UWidget* GetFocusTarget() const;
	/** Drops a "Press a key..." (leaving the tab). */
	void StopListening();
	bool IsListening() const { return Listening != INDEX_NONE; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
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
