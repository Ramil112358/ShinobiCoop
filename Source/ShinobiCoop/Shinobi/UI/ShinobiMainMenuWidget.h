#pragma once

#include "CoreMinimal.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "ShinobiMainMenuWidget.generated.h"

class AShinobiMenuStage;
class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UScrollBox;
class UWidgetSwitcher;
class UShinobiMainMenuWidget;

/**
 * The open stage behind the main menu's pages (SHINOBI and its creator): a drag here turns the character, the wheel zooms, a
 * double-click resets. Presses on a page's panels never reach it, so they don't move the preview.
 */
UCLASS()
class SHINOBICOOP_API UShinobiPreviewCatcher : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UShinobiMainMenuWidget> Menu;

	/** Holding this mouse button turns the character. */
	static bool TurnsPreview(const FKey& Button) { return Button == EKeys::RightMouseButton; }
	/** Degrees the character turns for a drag of DeltaX pixels: dragging right turns its front to the right. */
	static float TurnForDrag(float DeltaX) { return -DeltaX * 0.4f; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
};

enum class EShinobiMenuTab : uint8 { Play, Shinobi, Jutsu, Settings, Controls, Num };

/**
 * The main menu (docs/superpowers/specs/2026-10-20-main-menu-polish-design.md): a top tab bar over the live stage
 * (AShinobiMenuStage). PLAY hosts on a map card (mode toggle, HOST) or lists found games; SHINOBI picks the character and
 * outfit, and its "+" card (or EDIT on a made character) opens the character creator in the same tab (OpenCreator); JUTSU
 * edits the loadout; SETTINGS is the settings panel; CONTROLS the key bindings. Click a tab, or Q/E or the bumpers, to
 * switch; Esc or B leaves the creator for the SHINOBI strip, else goes to PLAY.
 */
UCLASS()
class SHINOBICOOP_API UShinobiMainMenuWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

public:
	void SelectTab(EShinobiMenuTab Tab);
	/** The stage wears the character played next match: the made one in play (UShinobiCharacterSave::Active), else
	 * the roster pick. */
	void ShowPlayed();
	EShinobiMenuTab GetTab() const { return Tab; }
	/** SHINOBI shows the character creator on save slot SlotIndex (a free one makes a new character), in the studio. */
	void OpenCreator(int32 SlotIndex);
	/** Back from the creator to the SHINOBI strip (unsaved edits are dropped). */
	void CloseCreator();
	/** The creator is open (on the SHINOBI tab). */
	bool IsCreating() const { return bCreating; }
	/** SHINOBI's character strip, and the creator it opens. */
	class UShinobiRosterPage* GetRosterPage() const;
	class UShinobiCreatorPage* GetCreatorPage() const;
	/** The page widget shown for Tab (SHINOBI's holds the roster strip and the creator). */
	UWidget* GetTabPage(EShinobiMenuTab Tab) const;
	/** The top bar's button for Which: a click on it opens that tab. */
	UShinobiIndexButton* GetTabButton(EShinobiMenuTab Which) const;
	/** What gets keyboard focus when Tab opens (null only while the menu isn't built). */
	UWidget* GetFocusTarget(EShinobiMenuTab Tab) const;
	/** Pages built by other widgets (SHINOBI, JUTSU) say what to focus. */
	void SetFocusTarget(EShinobiMenuTab Tab, UWidget* Target);
	/** The stage in this world, if the level has one. */
	AShinobiMenuStage* GetStage() const;
	/** Number of map rows on the Host list (one per GetMaps() entry). */
	int32 GetHostRowCount() const { return HostRows.Num(); }
	/** The Host list's row button for display row Row (GetHostOrder() order). */
	UWidget* GetHostRow(int32 Row) const { return HostRowButtons.IsValidIndex(Row) ? HostRowButtons[Row].Get() : nullptr; }
	/** Screenshot tour (UShinobiMenuShotSubsystem): select the Host row of DisplayName in its Mode-th format. */
	void TourSelectHost(const FString& DisplayName, int32 Mode);
	void TourShowFind() { ShowFind(true); }
	/** The preview hints for a gamepad (right stick) or a mouse (wheel, drag): the last device used picks. */
	void UseGamepadHints(bool bGamepad);
	FText GetHintText() const;
	/** Hint Index's mouse picture (0 zoom, 1 rotate), shown instead of its key badge while the mouse leads; null if not made. */
	UWidget* GetHintIcon(int32 Index) const;
	/** The open stage behind the pages: the only place a drag turns, or the wheel zooms, the preview. */
	UWidget* GetPreviewCatcher() const { return PreviewCatcher; }
	/** The stage when this tab previews a character (SHINOBI and its creator), else null. */
	AShinobiMenuStage* GetPreviewStage() const;
	UWidget* GetPagesWidget() const;
	/** The creator's cut to the studio (NativeTick drives it): fades to black, moves the stage in or out, fades back. */
	void TickStudioFade(float DeltaSeconds);
	/** The black over the screen, 0..1. */
	float GetStudioFade() const { return StudioFade; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	// The preview (SHINOBI and its creator): the right stick turns and zooms, R3 resets (the mouse: UShinobiPreviewCatcher). A
	// mouse move anywhere puts the hints back to the mouse's.
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
	virtual FString GetTitle() const override { return FString(); }
	virtual TArray<FString> GetEntries() const override { return {}; }
	virtual void OnEntry(int32 Index) override {}
	virtual void Refresh() override;

private:
	UWidget* BuildPlayPage();
	void BuildHostDetail();
	/** Host row chips' colours for the current selection. */
	void StyleChips();
	void OnTabButton(int32 Index);
	/** Opens NewTab, with the creator showing when bCreate (SHINOBI only). */
	void ShowPage(EShinobiMenuTab NewTab, bool bCreate);
	void OnHostRow(int32 Row);
	void OnMode(int32 Index);
	void OnJoinRow(int32 Index);
	void ShowFind(bool bFind);
	UShinobiIndexButton* MakeButton(const FString& Label, int32 Size, bool bPrimary, int32 Index, FShinobiIndexClicked Handler);
	/** Background art of the selected map (Host view only). */
	void UpdateBackdrop();

	EShinobiMenuTab Tab = EShinobiMenuTab::Play;
	float FadeIn = 1.f;
	bool bFinding = false;
	/** GetMaps() index per Host row, in GetHostOrder() order. */
	TArray<int32> HostRows;
	int32 SelectedRow = 0;
	/** The Host row that had focus last tick (INDEX_NONE: none). */
	int32 LastFocusedRow = INDEX_NONE;
	int32 SelectedMode = 0;
	/** Right stick (dead zone applied), turning and zooming the preview each tick. */
	FVector2D PreviewStick = FVector2D::ZeroVector;
	bool bGamepadHints = false;
	/** SHINOBI shows the creator rather than the character strip. */
	bool bCreating = false;
	/** The stage belongs in the studio (the creator, with a studio set in the level). */
	bool bWantStudio = false;
	float StudioFade = 0.f;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> TabButtons;
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> FocusTargets;
	/** SHINOBI's page: the roster strip, or the creator. */
	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> ShinobiPages;
	UPROPERTY()
	TObjectPtr<class UShinobiRosterPage> RosterPage;
	UPROPERTY()
	TObjectPtr<class UShinobiCreatorPage> CreatorPage;
	UPROPERTY()
	TObjectPtr<class UShinobiShortcutsWidget> ControlsPage;
	UPROPERTY()
	TObjectPtr<UImage> Backdrop;
	/** Darkens the left of the screen behind the PLAY lists. */
	UPROPERTY()
	TObjectPtr<UBorder> Shade;
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> HostTab;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> FindTab;
	UPROPERTY()
	TObjectPtr<UScrollBox> HostList;
	UPROPERTY()
	TObjectPtr<UScrollBox> FindList;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> HostRowButtons;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> HostRowChips;
	UPROPERTY()
	TObjectPtr<UVerticalBox> Detail;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> ModeButtons;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> HostButton;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> RefreshButton;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> QuitButton;
	/** Bottom right: the preview's zoom and rotate hints, (label, key badge, mouse picture) twice. */
	UPROPERTY()
	TObjectPtr<UHorizontalBox> Hints;
	/** (label, key) twice. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> HintTexts;
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> HintBadges;
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> HintIcons;
	UPROPERTY()
	TObjectPtr<UBorder> Blackout;
	UPROPERTY()
	TObjectPtr<UShinobiPreviewCatcher> PreviewCatcher;
	/** The creator's light presets (STUDIO, DAY, DUSK), top right. */
	UPROPERTY()
	TObjectPtr<UHorizontalBox> LightPresets;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> LightPresetButtons;
};
