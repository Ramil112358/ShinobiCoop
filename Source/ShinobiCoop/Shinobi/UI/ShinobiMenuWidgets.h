#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Shinobi/Pvp/ShinobiPvpMatch.h"
#include "Shinobi/UI/ShinobiSettings.h"
#include "ShinobiMenuWidgets.generated.h"

class UShinobiSessionSubsystem;
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
	void SetEntryLabel(int32 Index, const FString& Label);

	UPROPERTY()
	TObjectPtr<UVerticalBox> Column;

	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> EntryButtons;

	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;

	FDelegateHandle ChangedHandle;
};

/**
 * The settings panel (ShinobiSettings rows), a tab page of the main menu (SETTINGS) and the pause menu (SETTINGS): rows
 * under CHARACTER / GRAPHICS / DISPLAY / EFFECTS headings, each a selector "Name  < Value >". Clicking a row (or its >)
 * steps it on, its < steps it back; Left / Right (keys, D-pad, stick) step the focused row. Every change applies and saves
 * at once. The line under the rows explains the hovered or focused row.
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
	/** The Index-th row's value as shown. */
	FText GetShownValue(int32 Index) const;
	/** The help line under the rows. */
	FText GetDescriptionText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
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
	void RefreshValues();

	TArray<ShinobiSettings::ERow> Rows;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ValueTexts;
	/** Each row's < (a press over it steps back). */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> BackArrows;
	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionText;
	/** The row the help line explains (INDEX_NONE: the general note). */
	int32 DescribedRow = -2;
	/** Set by a press over a row's <, used by that row's click. */
	int32 PendingStep = 0;
};
