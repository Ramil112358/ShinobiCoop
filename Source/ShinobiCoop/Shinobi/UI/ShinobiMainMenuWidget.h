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

enum class EShinobiMenuTab : uint8 { Play, Shinobi, Jutsu, Settings, Num };

/**
 * The main menu (docs/superpowers/specs/2026-10-20-main-menu-polish-design.md): a top tab bar over the live stage
 * (AShinobiMenuStage). PLAY hosts on a map card (mode toggle, HOST) or lists found games; SHINOBI picks the character and
 * outfit; JUTSU edits the loadout; SETTINGS is the settings panel. Q/E or the bumpers switch tabs, Esc or B goes to PLAY.
 */
UCLASS()
class SHINOBICOOP_API UShinobiMainMenuWidget : public UShinobiMenuWidget
{
	GENERATED_BODY()

public:
	void SelectTab(EShinobiMenuTab Tab);
	EShinobiMenuTab GetTab() const { return Tab; }
	/** The page widget shown for Tab. */
	UWidget* GetTabPage(EShinobiMenuTab Tab) const;
	/** What gets keyboard focus when Tab opens (null only while the menu isn't built). */
	UWidget* GetFocusTarget(EShinobiMenuTab Tab) const;
	/** Pages built by other widgets (SHINOBI, JUTSU) say what to focus. */
	void SetFocusTarget(EShinobiMenuTab Tab, UWidget* Target);
	/** The stage in this world, if the level has one. */
	AShinobiMenuStage* GetStage() const;
	/** Number of map rows on the Host list (one per GetMaps() entry). */
	int32 GetHostRowCount() const { return HostRows.Num(); }
	/** Screenshot tour (UShinobiMenuShotSubsystem): select the Host row of DisplayName in its Mode-th format. */
	void TourSelectHost(const FString& DisplayName, int32 Mode);
	void TourShowFind() { ShowFind(true); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FString GetTitle() const override { return FString(); }
	virtual TArray<FString> GetEntries() const override { return {}; }
	virtual void OnEntry(int32 Index) override {}
	virtual void Refresh() override;

private:
	UWidget* BuildPlayPage();
	void BuildHostDetail();
	void OnTabButton(int32 Index);
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

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> TabButtons;
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> FocusTargets;
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
	TObjectPtr<UVerticalBox> Detail;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> ModeButtons;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> HostButton;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> RefreshButton;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> QuitButton;
};
