#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "ShinobiShortcutsWidget.generated.h"

class UShinobiIndexButton;
class UTextBlock;
class UVerticalBox;

/**
 * The CONTROLS tab (main menu and pause menu): what each jutsu slot and action is on, with a KEYBOARD & MOUSE tab and a
 * CONTROLLER tab (LT / RT switch them) (ShinobiKeyBindings). Pick an action, then press the new key (Alt + key works) or
 * button (hold RB or RT first for a two-button combo); one already in use swaps over. Reset to defaults puts back the
 * shown device's keys or buttons.
 */
UCLASS()
class SHINOBICOOP_API UShinobiShortcutsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Where the panel's centre sits on its page (0-1). Set before it's built. */
	FVector2D PanelAnchor = FVector2D(0.5f, 0.5f);

	/** Slot 1's chip: focus goes there when the tab opens. */
	UWidget* GetFocusTarget() const;
	/** Drops a "Press a key..." (leaving the tab). */
	void StopListening();
	bool IsListening() const { return Listening != INDEX_NONE; }

	/** The CONTROLLER tab (bPad) or the KEYBOARD & MOUSE tab. */
	void ShowDevice(bool bPad);
	bool IsShowingPad() const { return bShowingPad; }
	/** What Bindable's chip says on the shown tab ("Space", "RB + A", "None"). */
	FText GetChipText(int32 Bindable) const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	void AddRow(UVerticalBox* Column, int32 Bindable);
	void OnChip(int32 Bindable);
	void OnDeviceTab(int32 Index);
	void OnFooter(int32 Index);
	/** Rebinds the listening row to Key (Alt held or not) and says what happened. */
	void Bind(const FKey& Key, bool bAlt);
	/** Rebinds the listening row to Hold (RB / RT, or none) + Button and says what happened. */
	void BindPad(const FKey& Hold, const FKey& Button);
	void Cancel();
	void RefreshChips();
	void SetMessage(const FString& Text, const FLinearColor& Color);

	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> Chips;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> DeviceTabs;
	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;
	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;

	bool bShowingPad = false;
	/** The row waiting for a key or button (a Bindable), or INDEX_NONE. */
	int32 Listening = INDEX_NONE;
	/** RB or RT held while a controller row listens: the first of a two-button combo. */
	FKey PendingHold;
};
