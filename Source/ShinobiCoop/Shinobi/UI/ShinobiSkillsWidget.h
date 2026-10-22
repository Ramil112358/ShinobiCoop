#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"
#include "ShinobiSkillsWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UShinobiSkillsWidget;
class AShinobiPlayerCharacter;

/** One square on the Skills screen: a jutsu in the pool or one of the 8 slots. Click (or A), right-click, drag and drop. */
UCLASS()
class SHINOBICOOP_API UShinobiSkillCell : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UShinobiSkillsWidget> Owner;
	bool bSlot = false;
	/** Pool: index into the known jutsu. Slot: 0-7. */
	int32 Index = 0;

	void Show(TSubclassOf<class UShinobiJutsuAbility> Jutsu, const FLinearColor& Frame, const FText& Overlay, const FText& Key, float Dim);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& Event) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& Event) override;

private:
	UPROPERTY()
	TObjectPtr<UBorder> FrameBorder;
	UPROPERTY()
	TObjectPtr<UImage> Icon;
	UPROPERTY()
	TObjectPtr<UTextBlock> OverlayText;
	UPROPERTY()
	TObjectPtr<UTextBlock> KeyText;

	TWeakObjectPtr<UClass> ShownJutsu;
	FLinearColor FrameColor = FLinearColor::Gray;
	bool bFocused = false;
};

/**
 * The Skills screen (K, or the pause menu's Jutsu): every jutsu the player knows above, their 8 bar slots below. Click a
 * jutsu then a slot (or drag it there) to equip; drag slot to slot to reorder; right-click a slot (or drag it to the
 * pool) to empty it. Swaps only out of combat and never for a jutsu on cooldown: the server decides (RequestLoadout).
 * The game keeps running; gameplay input is blocked while it is open.
 */
UCLASS()
class SHINOBICOOP_API UShinobiSkillsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The main menu's JUTSU tab: the pawn's known jutsu and the saved bar, edited and saved directly (no pawn, no combat).
	 * Esc / B / K are left to the menu. Set before the widget is built. */
	bool bMenuMode = false;

	void OnCellClicked(const UShinobiSkillCell& Cell);
	void OnCellRightClicked(const UShinobiSkillCell& Cell);
	void OnCellDropped(const UShinobiSkillCell& From, const UShinobiSkillCell& To);
	/** B (no pick), K or Esc. */
	void Close();
	/** Shows Jutsu in the detail panel (hover, focus or click). */
	void SelectJutsu(TSubclassOf<UShinobiJutsuAbility> Jutsu);
	/** The bar on screen: the pawn's, or in menu mode the saved one. */
	const ShinobiLoadout::FSlots& GetShownLoadout() const;
	/** A pool cell (index into the known jutsu) or a slot cell (0-7). */
	UShinobiSkillCell* GetCell(bool bSlot, int32 Index) const;
	/** The first pool cell. */
	UWidget* GetFocusTarget() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	AShinobiPlayerCharacter* GetPlayer() const;
	/** Cell frames, slot keys and the banner from the shown bar. */
	void RefreshCells();
	/** Sends Slots to the server if nothing obvious forbids it; otherwise says why. */
	void Request(const ShinobiLoadout::FSlots& Slots);

	UPROPERTY()
	TArray<TObjectPtr<UShinobiSkillCell>> PoolCells;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiSkillCell>> SlotCells;
	UPROPERTY()
	TObjectPtr<UTextBlock> Banner;
	UPROPERTY()
	TObjectPtr<UTextBlock> DetailGroup;
	UPROPERTY()
	TObjectPtr<UTextBlock> DetailName;
	UPROPERTY()
	TObjectPtr<UTextBlock> DetailText;
	UPROPERTY()
	TObjectPtr<UTextBlock> DetailStats;

	/** Menu mode's bar. */
	ShinobiLoadout::FSlots MenuLoadout;

	TArray<TSubclassOf<class UShinobiJutsuAbility>> Known;
	int32 PickedPool = INDEX_NONE;
	int32 PickedSlot = INDEX_NONE;
	FText Notice;
	double NoticeUntil = 0.0;
};
