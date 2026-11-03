#pragma once

#include "CoreMinimal.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "ShinobiPauseMenuWidget.generated.h"

class UHorizontalBox;
class UWidgetSwitcher;
class UShinobiShortcutsWidget;
class UShinobiSkillsWidget;

enum class EShinobiPauseTab : uint8 { Game, Jutsu, Settings, Controls, Num };

/**
 * The in-game menu (Esc / Start; K opens it on JUTSU): one screen with a tab bar, like the main menu, so nothing opens
 * over anything else. GAME: Resume, Invite Friend, Leave (asks twice) and who's in the game; JUTSU: the Skills screen;
 * SETTINGS: the settings panel; CONTROLS: the key bindings. Q / E or the bumpers switch tabs; Esc or Start resumes from
 * any tab, B goes back to GAME first. The world keeps running (it's online) but gameplay input waits until it closes.
 */
UCLASS()
class SHINOBICOOP_API UShinobiPauseMenuWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

public:
	/** The tab it opens on. Set before it's built. */
	EShinobiPauseTab InitialTab = EShinobiPauseTab::Game;

	void SelectTab(EShinobiPauseTab NewTab);
	EShinobiPauseTab GetTab() const { return Tab; }
	/** The page widget shown for Which. */
	UWidget* GetTabPage(EShinobiPauseTab Which) const;
	/** What gets keyboard focus when Which opens (null only while the menu isn't built). */
	UWidget* GetFocusTarget(EShinobiPauseTab Which) const;
	/** Keyboard / gamepad focus to the current tab's first control. */
	void FocusTab();
	/** Leave's first press arms it ("Confirm leave"); a second press within a few seconds leaves. */
	bool IsLeaveArmed() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FString GetTitle() const override { return FString(); }
	virtual TArray<FString> GetEntries() const override { return {}; }
	virtual void OnEntry(int32 Index) override {}
	virtual void Refresh() override;

private:
	UWidget* BuildGamePage();
	void OnTabButton(int32 Index);
	void OnGameButton(int32 Index);
	/** Closes the menu (the controller puts the game's input back). */
	void Close();
	/** Footer key hints for a gamepad or a keyboard. */
	void UseGamepadHints(bool bGamepad);
	/** The GAME page's player list. */
	void RefreshSquad();
	void SetLeaveArmed(bool bArmed);

	EShinobiPauseTab Tab = EShinobiPauseTab::Game;
	float FadeIn = 1.f;
	bool bGamepadHints = false;
	bool bHintsBuilt = false;
	double LeaveArmedUntil = 0.0;
	float SquadTimer = 0.f;
	/** Names shown in the player list, to rebuild it only when someone joins, leaves or renames. */
	TArray<FString> ShownSquad;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> TabButtons;
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> FocusTargets;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> ResumeButton;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> InviteButton;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> LeaveButton;
	UPROPERTY()
	TObjectPtr<UTextBlock> InviteNote;
	UPROPERTY()
	TObjectPtr<UTextBlock> SquadCount;
	UPROPERTY()
	TObjectPtr<UVerticalBox> SquadList;
	UPROPERTY()
	TObjectPtr<UHorizontalBox> Hints;
	UPROPERTY()
	TObjectPtr<UShinobiSkillsWidget> JutsuPage;
	UPROPERTY()
	TObjectPtr<UShinobiSettingsWidget> SettingsPage;
	UPROPERTY()
	TObjectPtr<UShinobiShortcutsWidget> ControlsPage;
};
