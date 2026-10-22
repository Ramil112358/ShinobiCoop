#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Shinobi/Pvp/ShinobiPvpMatch.h"
#include "ShinobiMenuWidgets.generated.h"

class UShinobiSessionSubsystem;
class UTextBlock;
class UVerticalBox;

DECLARE_DELEGATE_OneParam(FShinobiIndexClicked, int32);

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
	/** Opens the settings panel over this menu (hidden until the panel's Back). */
	void OpenSettings();

	UPROPERTY()
	TObjectPtr<UVerticalBox> Column;

	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> EntryButtons;

	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;

	FDelegateHandle ChangedHandle;
};

/** Resume, Jutsu (the Skills screen), Invite Friend (Steam overlay), Settings, Leave. The game keeps running (it's online). */
UCLASS()
class SHINOBICOOP_API UShinobiPauseMenuWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

protected:
	virtual FString GetTitle() const override { return TEXT("PAUSED"); }
	virtual TArray<FString> GetEntries() const override { return { TEXT("Resume"), TEXT("Jutsu"), TEXT("Invite Friend"), TEXT("Settings"), TEXT("Leave") }; }
	virtual void OnEntry(int32 Index) override;
	virtual void Refresh() override;
};

/** Graphics settings (ShinobiSettings rows): each click cycles a row, applies and saves; Shortcuts opens the key bindings; Back returns to the opener. */
UCLASS()
class SHINOBICOOP_API UShinobiSettingsWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

public:
	/** The menu hidden underneath, shown again on Back. */
	TWeakObjectPtr<UUserWidget> Opener;
	/** The main menu's SETTINGS tab: no Character / Outfit rows (SHINOBI has them) and no Back. Set before it's built. */
	bool bMainMenu = false;

protected:
	virtual FString GetTitle() const override { return TEXT("SETTINGS"); }
	/** In the main menu's SETTINGS tab the panel sits right of the stage's character. */
	virtual FVector2D GetPanelAnchor() const override { return bMainMenu ? FVector2D(0.72f, 0.5f) : FVector2D(0.5f, 0.5f); }
	virtual TArray<FString> GetEntries() const override;
	virtual void OnEntry(int32 Index) override;
	virtual void Refresh() override;
};
