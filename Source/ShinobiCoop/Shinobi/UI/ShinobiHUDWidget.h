#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiHUDWidget.generated.h"

class UAbilitySystemComponent;
class UBorder;
class UCanvasPanel;
class UGameplayAbility;
class UImage;
class UOverlay;
class UProgressBar;
class UShinobiJutsuAbility;
class UShinobiMiniMapData;
class USizeBox;
class UTextBlock;
class UVerticalBox;
struct FGameplayTagContainer;

/**
 * Bottom-left unit frame (portrait, name, health / chakra / stamina bars, Substitution pips) and the four jutsu
 * slots (painted icon, key, chakra cost, name); the arena's wave line, teammates,
 * banners (downed, dead, wipe, victory), revive progress and lock-on; in a PvP arena the score strip, round banners,
 * opponents and active boosts take their place. Built in code; polled each tick.
 */
UCLASS()
class SHINOBICOOP_API UShinobiHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 NumJutsuSlots = 4;

	/** The width the jutsu slot Index takes up (its icon and its name); every slot is the same, whatever the name. For tests. */
	float GetJutsuSlotWidth(int32 Index) const;

	/** Whether the dragon crosshair is showing, and where (viewport pixels). For tests. */
	bool IsDragonCrosshairShown() const;

	/** M: the level's map large in the middle of the screen, in place of the minimap. Stays shut on a level without a map. */
	void ToggleBigMap();
	bool IsBigMapOpen() const { return bBigMap; }

	/** Zooms the open map (big or mini) one level: Steps > 0 in. With bWrap, zooming in past the closest level goes back
	 * to the whole map. Zoomed views follow the player. */
	void ZoomMap(int32 Steps, bool bWrap);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void BuildUnitFrame(UCanvasPanel* Root);
	/** A fixed-size row in Bars for one bar and its overlays (the trail, the numbers). */
	UOverlay* AddBarRow(UVerticalBox* Bars, float Width, float Height);
	UProgressBar* AddBar(UOverlay* Row, FName Name, const FLinearColor& Color, bool bBacked);
	void BuildSubstitutionPips(UVerticalBox* Bars);
	void BuildStatusIcons(UVerticalBox* Bars);
	void BuildJutsuSlots(UCanvasPanel* Root);
	UTextBlock* MakeText(int32 FontSize, const FLinearColor& Tint, bool bBold = false);
	UTextBlock* AddLabel(UOverlay* Layers, int32 FontSize, EHorizontalAlignment HorizontalAlign, EVerticalAlignment VerticalAlign, const FLinearColor& Tint);
	/** The slots show the jutsu on Page (the player's current jutsu page); slots with none on it are hidden. */
	void UpdateJutsuSlots(const UAbilitySystemComponent& ASC, int32 Page, const class AShinobiPlayerCharacter* Player);
	/** One small diamond per jutsu page under the slots; the current page is lit. */
	void BuildPagePips(UCanvasPanel* Root);

	/** While riding a dragon: a ring at the point where its fire will end (straight ahead of the dragon, or the first thing in the way). */
	void BuildDragonCrosshair(UCanvasPanel* Root);
	void UpdateDragonCrosshair(const class AShinobiPlayerCharacter* Player);
	void BuildAimCrosshair(UCanvasPanel* Root);
	void UpdateAimCrosshair(const class AShinobiPlayerCharacter* Player, const FVector2D& ViewSize);
	UTextBlock* AddCanvasText(UCanvasPanel* Root, FName Name, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position, int32 FontSize, ETextJustify::Type Justify);
	void UpdateArena(const class AShinobiPlayerCharacter* Player);
	/** PvP: the opposing side, one "name  hp / max" line each (players and bots, never summons). */
	FString OpponentList(const class AShinobiPlayerCharacter* Player) const;
	/** PvP: "Attack boost 12s" per active pickup boost on Player. */
	FString BoostList(const class AShinobiPlayerCharacter* Player) const;
	/** Open world: the compass strip (cardinal points, village, camps, boss) around the camera's yaw. */
	void UpdateCompass();
	UTextBlock* CompassLabel(int32 Index);

	/** Bottom-right: the level's baked map (UShinobiMiniMapData::ForLevel), north-up; hidden when none is baked. */
	void BuildMiniMap(UCanvasPanel* Root);
	/** Lays the map out (corner or big), pans it to the player at the current zoom and moves the arrow, teammates,
	 * village and camps onto it. ScreenSize: the HUD's size, for the big map. */
	void UpdateMiniMap(const class AShinobiCharacterBase* Pawn, const FVector2D& ScreenSize);
	/** The closest zoom level (index into the zoom steps) for a view ViewLong across: the view still spans a useful
	 * stretch of world and the art is not blown up too far. */
	int32 MaxMapZoom(float ViewLong) const;
	/** Pooled map dot Index, shown at Position (map pixels) with this size and colour. */
	void MiniMapDot(int32 Index, const FVector2D& Position, float Size, const FLinearColor& Colour);

	/** Binds the red "can't cast" flash to the current pawn's ability system; rebinds when the pawn changes. */
	void BindFailureFlash(UAbilitySystemComponent* ASC);
	void OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Reasons);

	/** Top-centre: wave, phase countdown, enemies left. */
	UPROPERTY()
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UBorder> CompassBar;

	UPROPERTY()
	TObjectPtr<const UShinobiMiniMapData> MiniMapData;

	/** The corner map's size on screen: the long edge is fixed, the short one follows the map's aspect. */
	FVector2D MiniMapSize = FVector2D::ZeroVector;

	/** The size the map is laid out at now (corner or big), and whether that is the big layout. */
	FVector2D MapViewSize = FVector2D::ZeroVector;
	bool bMapLaidOutBig = false;

	/** The tile layout last applied (offset and whole-map size, in view units); tiles move only when it changes. */
	FVector2D TileOffset = FVector2D(-1.f);
	FVector2D TileFull = FVector2D::ZeroVector;

	bool bBigMap = false;
	int32 MiniZoom = 1;
	int32 BigZoom = 0;

	UPROPERTY()
	TObjectPtr<UBorder> MiniMapFrame;

	/** Behind the big map: dims the game. */
	UPROPERTY()
	TObjectPtr<UBorder> BigMapShade;

	UPROPERTY()
	TObjectPtr<UTextBlock> BigMapHint;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> MiniMapCanvas;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> MiniMapTiles;

	UPROPERTY()
	TObjectPtr<UImage> MiniMapArrow;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> MiniMapDots;

	/** Pooled compass labels, positioned every tick; unused ones are hidden. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CompassLabels;

	/** Top-left: teammates' names and health. */
	UPROPERTY()
	TObjectPtr<UTextBlock> TeamText;

	/** Top-right, PvP: the opposing side's names and health. */
	UPROPERTY()
	TObjectPtr<UTextBlock> OpponentText;

	/** Below the hint, PvP: the active pickup boosts and their seconds left. */
	UPROPERTY()
	TObjectPtr<UTextBlock> BoostText;

	/** Centre: downed / dead / wipe / victory. */
	UPROPERTY()
	TObjectPtr<UTextBlock> BannerText;

	/** Below the centre: revive progress, lock-on. */
	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar;

	/** Behind the health bar: holds the pre-damage level for a moment, then drains down to the health. */
	UPROPERTY()
	TObjectPtr<UProgressBar> HealthTrailBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChakraText;

	UPROPERTY()
	TObjectPtr<UProgressBar> ChakraBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SubstitutionPips;

	/** The SpriteCook pip art (T_Hud_PipOn / PipOff); the pips fall back to tinted diamonds while it's missing. */
	UPROPERTY()
	FSlateBrush PipOnBrush;

	UPROPERTY()
	FSlateBrush PipOffBrush;

	/** Status medallions under the pips (burn, black flame, shock, stagger), shown while the player carries that state. */
	UPROPERTY()
	TArray<TObjectPtr<UImage>> StatusIcons;

	/** True once the SpriteCook slot frame texture is loaded: the frames then flash by tint instead of a redrawn outline. */
	bool bFrameArt = false;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotFrames;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> SlotIcons;

	/** The jutsu's name inside the slot while its icon texture is missing. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotFallbacks;

	/** Under each slot. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotNames;

	/** Each slot's column (frame + name): hidden when the current page has no jutsu there. */
	UPROPERTY()
	TArray<TObjectPtr<UVerticalBox>> SlotColumns;

	UPROPERTY()
	TObjectPtr<class USizeBox> DragonCrosshair;
	UPROPERTY()
	TArray<TObjectPtr<class UBorder>> DragonCrosshairParts;

	UPROPERTY()
	TObjectPtr<class USizeBox> AimCrosshair;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> PagePips;

	/** The jutsu each slot shows; its icon, name and cost are set when it changes. */
	TArray<TWeakObjectPtr<const UShinobiJutsuAbility>> SlotJutsu;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCosts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotCooldownTexts;

	/** Dark overlay whose height is the remaining share of the cooldown. */
	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> SlotShades;

	float HealthTrail = 1.f;
	float LastHealthRatio = 1.f;
	double TrailHoldUntil = 0.0;

	/** World time until which each slot shows the red flash. */
	TArray<double> SlotFlashUntil;

	/** For the wave-start gong and the victory chime. */
	uint8 LastPhase = 0;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle FailedHandle;
};
