#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "ShinobiDropdown.generated.h"

class UBorder;
class UScrollBox;
class UTextBlock;

/**
 * A dropdown's open option list: its own layer over every menu (so it can hang past the panel it opened from), right
 * under the field, or above it near the bottom of the screen. The current option is ticked and focused. Click, A or
 * Enter picks; Up / Down (or the stick) move; a letter key jumps to the next option starting with it; Esc, B, or a
 * click outside closes it unchanged. Focus goes back to the field either way.
 */
UCLASS()
class SHINOBICOOP_API UShinobiDropdownList : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Opens InOptions under PlaceUnder (null: InAnchor) with InSelected marked; InOnPick gets the chosen index (not called
	 * when dismissed). Focus goes back to InAnchor when it closes. Closes any other open list first. Null when there's no game
	 * viewport to show it in (tests). InLocked (empty, or one per option) greys out the options with a non-empty reason: they
	 * are listed and can be focused, but not picked (the settings help line says why). */
	static UShinobiDropdownList* Open(UWidget& InAnchor, const TArray<FString>& InOptions, int32 InSelected, TFunction<void(int32)> InOnPick, UWidget* PlaceUnder = nullptr,
		const TArray<FString>& InLocked = TArray<FString>());
	/** The list on screen, if any. */
	static UShinobiDropdownList* GetOpen();
	/** Dismisses the list on screen, if any. */
	static void CloseOpen();

	/** Chooses Options[Index]: the list closes, then OnPick runs. A locked option: nothing. */
	void Pick(int32 Index);
	/** Closes without choosing. */
	void Dismiss();
	int32 GetOptionCount() const { return Options.Num(); }
	bool IsOptionLocked(int32 Index) const { return Locked.IsValidIndex(Index) && !Locked[Index].IsEmpty(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event, const FNavigationReply& DefaultReply) override;

private:
	void FocusItem(int32 Index);
	int32 GetFocusedItem() const;

	TArray<FString> Options;
	/** Why each option can't be picked ("" when it can); may be empty. */
	TArray<FString> Locked;
	int32 Selected = 0;
	TFunction<void(int32)> OnPick;
	TWeakObjectPtr<UWidget> Anchor;
	/** Top left (or, bAbove, bottom left) in viewport units, and the width: the field's. */
	FVector2D Position = FVector2D::ZeroVector;
	float Width = 240.f;
	bool bAbove = false;
	int32 LitItem = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UBorder> Panel;
	UPROPERTY()
	TObjectPtr<UScrollBox> List;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> Items;
};

/**
 * A dropdown field for a choice among many (more than two) options: the picked one and a chevron in a menu-style
 * button. A click (A / Enter) opens the list (UShinobiDropdownList); choosing from it shows the new option and runs
 * OnPicked. Two-option choices (On / Off, Male / Female) stay "< value >" steppers.
 */
UCLASS()
class SHINOBICOOP_API UShinobiDropdown : public UShinobiIndexButton
{
	GENERATED_BODY()

public:
	/** Runs with the chosen index, after the field shows it. */
	FShinobiIndexClicked OnPicked;

	/** Fills the field from Tree (its owner's widget tree), MinWidth wide at least, and arms it. Bind OnIndexClicked first
	 * to run something before the list opens (it must then call OpenList); left unbound, a click just opens it. */
	void Build(UWidgetTree& Tree, int32 FontSize = 15, float MinWidth = 220.f);
	void SetOptions(const TArray<FString>& NewOptions, int32 NewSelected);
	int32 GetSelected() const { return Selected; }
	const TArray<FString>& GetOptions() const { return Options; }
	/** Opens the option list under the field. */
	void OpenList();
	/** As if Options[Option] were chosen from the list: the field shows it and OnPicked runs. */
	void Choose(int32 Option);

private:
	void HandleOpen(int32 Unused);
	void ShowSelected();

	TArray<FString> Options;
	int32 Selected = 0;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;
};
