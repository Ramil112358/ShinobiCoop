#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
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

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Title and button labels, in order. */
	virtual FString GetTitle() const PURE_VIRTUAL(UShinobiMenuWidget::GetTitle, return FString(););
	virtual TArray<FString> GetEntries() const PURE_VIRTUAL(UShinobiMenuWidget::GetEntries, return {};);
	virtual void OnEntry(int32 Index) PURE_VIRTUAL(UShinobiMenuWidget::OnEntry, );
	/** Session state changed: refresh the message line (and anything else). */
	virtual void Refresh();

	UShinobiSessionSubsystem* GetSessions() const;
	UShinobiIndexButton* AddButton(UVerticalBox* Box, const FString& Label, int32 Index, FShinobiIndexClicked Handler);
	UTextBlock* AddText(UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color);
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

/** Host Game, Find Games (a list of games to join), Quit; online status and the last failure. */
UCLASS()
class SHINOBICOOP_API UShinobiMainMenuWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FString GetTitle() const override { return TEXT("SHINOBI CO-OP"); }
	virtual TArray<FString> GetEntries() const override { return { TEXT("Host Game"), TEXT("Find Games"), TEXT("Settings"), TEXT("Quit") }; }
	virtual void OnEntry(int32 Index) override;
	virtual void Refresh() override;

private:
	void OnJoinRow(int32 Index);
	void OnMapRow(int32 Index);

	/** Host Game was pressed: the list shows maps to host on instead of found games. */
	bool bPickingMap = false;

	UPROPERTY()
	TObjectPtr<UVerticalBox> GameList;
};

/** Resume, Invite Friend (Steam overlay), Leave. The game keeps running (it's online). */
UCLASS()
class SHINOBICOOP_API UShinobiPauseMenuWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

protected:
	virtual FString GetTitle() const override { return TEXT("PAUSED"); }
	virtual TArray<FString> GetEntries() const override { return { TEXT("Resume"), TEXT("Invite Friend"), TEXT("Settings"), TEXT("Leave") }; }
	virtual void OnEntry(int32 Index) override;
	virtual void Refresh() override;
};

/** Graphics settings (ShinobiSettings rows): each click cycles a row, applies and saves; Back returns to the opener. */
UCLASS()
class SHINOBICOOP_API UShinobiSettingsWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

public:
	/** The menu hidden underneath, shown again on Back. */
	TWeakObjectPtr<UUserWidget> Opener;

protected:
	virtual FString GetTitle() const override { return TEXT("SETTINGS"); }
	virtual TArray<FString> GetEntries() const override;
	virtual void OnEntry(int32 Index) override;
	virtual void Refresh() override;
};
