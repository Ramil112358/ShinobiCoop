#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Shinobi/Pvp/ShinobiPvpMatch.h"
#include "Shinobi/UI/ShinobiSettings.h"
#include "ShinobiMenuWidgets.generated.h"

class UShinobiSessionSubsystem;
class UBorder;
class UTextBlock;
class UVerticalBox;

DECLARE_DELEGATE_OneParam(FShinobiIndexClicked, int32);
DECLARE_DELEGATE_TwoParams(FShinobiIndexValue, int32, float);

/** A button that reports its index when clicked (session list rows, menu entries). */
UCLASS()
class SHINOBICOOP_API UShinobiIndexButton : public UButton
{
	GENERATED_BODY()

public:
	int32 Index = 0;
	FShinobiIndexClicked OnIndexClicked;

	/** Bind OnIndexClicked, then call this. */
	void Arm();

private:
	UFUNCTION()
	void HandleClicked();
};

/** A slider that reports its index with each value (the character creator's shape rows). */
UCLASS()
class SHINOBICOOP_API UShinobiIndexSlider : public USlider
{
	GENERATED_BODY()

public:
	int32 Index = 0;
	FShinobiIndexValue OnIndexValue;

	/** Bind OnIndexValue, then call this. */
	void Arm();

private:
	UFUNCTION()
	void HandleValue(float NewValue);
};

/** Base for the menus: a centred column of a title, a message line and buttons, built in code like the HUD. */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The first entry button (keyboard / gamepad focus starts there); null before it's built. */
	UWidget* GetFirstEntry() const { return EntryButtons.Num() > 0 ? EntryButtons[0].Get() : nullptr; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	/** Where the panel's centre sits on screen (0-1). */
	virtual FVector2D GetPanelAnchor() const { return FVector2D(0.5f, 0.5f); }
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

	/** Title, entry buttons and message line into Parent (RebuildWidget's centred panel, or a tab page). */
	void BuildColumn(class UContentWidget* Parent);

	/** Title and button labels, in order. */
	virtual FString GetTitle() const PURE_VIRTUAL(UShinobiMenuWidget::GetTitle, return FString(););
	virtual TArray<FString> GetEntries() const PURE_VIRTUAL(UShinobiMenuWidget::GetEntries, return {};);
	virtual void OnEntry(int32 Index) PURE_VIRTUAL(UShinobiMenuWidget::OnEntry, );
	/** Session state changed: refresh the message line (and anything else). */
	virtual void Refresh();

	UShinobiSessionSubsystem* GetSessions() const;
	UShinobiIndexButton* AddButton(UVerticalBox* Box, const FString& Label, int32 Index, FShinobiIndexClicked Handler);
	UTextBlock* AddText(UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color, bool bHeading = false);

	UPROPERTY()
	TObjectPtr<UVerticalBox> Column;

	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> EntryButtons;

	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;

	FDelegateHandle ChangedHandle;
};

/**
 * The settings panel (ShinobiSettings rows), a tab page of the main menu (SETTINGS) and the pause menu (SETTINGS). As in
 * current games' settings screens, the rows are split over category tabs along its top (CHARACTER in game only, DISPLAY,
 * GRAPHICS, UPSCALING: ShinobiSettings::ECategory), each with its own headings (SCREEN / FRAME RATE, QUALITY / RAY TRACING /
 * EFFECTS...). Click a category, or LT / RT (as on CONTROLS), to switch; Left / Right on a focused category tab walk them.
 * The panel keeps one size whichever is shown, and reopens on the category it was last left on. A row with many values
 * (quality, upscaling quality, frame cap, character...) is a dropdown: a click (A / Enter) opens the list of them to pick
 * from. An On / Off row is a stepper "< Value >": a click flips it. Left / Right (keys, D-pad, stick) step the focused row
 * either way. Every change applies and saves at once. The line under the rows explains the hovered or focused row.
 */
UCLASS()
class SHINOBICOOP_API UShinobiSettingsWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

public:
	/** The main menu's SETTINGS tab: no Character / Face / Outfit rows (SHINOBI has them). Set before it's built. */
	bool bMainMenu = false;

	/** Steps the Index-th row shown by Delta values (wrapping), applies, saves and updates every value shown. */
	void StepRow(int32 Index, int32 Delta);
	/** Sets the Index-th row shown to its Option-th value (ShinobiSettings::RowOptions), as picked from its dropdown. */
	void PickRow(int32 Index, int32 Option);
	/** Opens the Index-th row's dropdown list (a click on a dropdown row does this). */
	void OpenRowList(int32 Index);
	/** The Index-th row's value as shown. */
	FText GetShownValue(int32 Index) const;
	/** The help line under the rows. */
	FText GetDescriptionText() const;

	/** The category tabs shown, in order (ShinobiSettings::CategoriesFor). */
	const TArray<ShinobiSettings::ECategory>& GetCategories() const { return Categories; }
	/** Shows Category's rows (one that isn't shown: nothing). bFocusRow moves keyboard / gamepad focus to its first row. */
	void SelectCategory(ShinobiSettings::ECategory Category, bool bFocusRow = false);
	ShinobiSettings::ECategory GetCategory() const;
	/** The category tab's button: a click on it shows that category. Null when it isn't shown. */
	UShinobiIndexButton* GetCategoryButton(ShinobiSettings::ECategory Category) const;
	/** The shown category's first row: focus goes there when the panel's tab opens (null only before it's built). */
	UWidget* GetFocusTarget() const;
	/** Whether the Index-th row is on the shown category. */
	bool IsRowShown(int32 Index) const;
	/** The Index-th row's button (null out of range). */
	UShinobiIndexButton* GetRowButton(int32 Index) const { return EntryButtons.IsValidIndex(Index) ? EntryButtons[Index].Get() : nullptr; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event, const FNavigationReply& DefaultReply) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	/** In the main menu's SETTINGS tab the panel sits right of the stage's character. */
	virtual FVector2D GetPanelAnchor() const override { return bMainMenu ? FVector2D(0.72f, 0.5f) : FVector2D(0.5f, 0.5f); }
	virtual FString GetTitle() const override { return FString(); }
	virtual TArray<FString> GetEntries() const override { return {}; }
	virtual void OnEntry(int32 Index) override;
	virtual void Refresh() override;

private:
	/** The row whose button has keyboard / gamepad focus, else INDEX_NONE. */
	int32 GetFocusedRow() const;
	/** The category tab whose button has keyboard / gamepad focus (an index into Categories), else INDEX_NONE. */
	int32 GetFocusedCategoryTab() const;
	void OnCategoryButton(int32 Index);
	/** Shows the Index-th category tab (an index into Categories). */
	void ShowCategoryTab(int32 Index, bool bFocusRow);
	void RefreshValues();
	/** The help line for the Row-th row shown: its description, then why it or some of its options are locked. */
	FString DescribeRow(int32 Row) const;

	TArray<ShinobiSettings::ERow> Rows;
	/** Each row's category tab (an index into Categories). */
	TArray<int32> RowTabs;
	TArray<ShinobiSettings::ECategory> Categories;
	/** The shown category tab (an index into Categories). */
	int32 ShownTab = 0;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> CategoryButtons;
	/** A page of rows per category tab; the shown one is active. */
	UPROPERTY()
	TObjectPtr<class UWidgetSwitcher> CategoryPages;
	/** Each category page's scroll box (its rows scroll if they don't fit). */
	UPROPERTY()
	TArray<TObjectPtr<class UScrollBox>> RowsScrolls;
	int32 ScrolledToRow = INDEX_NONE;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ValueTexts;
	/** Each stepper row's < (a press over it steps back); null on dropdown rows. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> BackArrows;
	/** Each dropdown row's field (the list opens under it); null on stepper rows. */
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> Fields;
	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionText;
	/** The row the help line explains (INDEX_NONE: the general note). */
	int32 DescribedRow = -2;
	/** Set when values change (a lock may have changed): the help line is written again. */
	bool bDescriptionStale = false;
	/** Set by a press over a row's <, used by that row's click. */
	int32 PendingStep = 0;
};
