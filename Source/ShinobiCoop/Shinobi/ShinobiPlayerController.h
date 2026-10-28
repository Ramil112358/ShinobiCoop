#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "ShinobiPlayerController.generated.h"

class UGameplayEffect;
class UShinobiHUDWidget;
class UShinobiPauseMenuWidget;
class UShinobiSkillsWidget;
struct FGameplayTag;
struct FInputActionValue;

/** Creates the HUD for local players; hosts developer console commands and the smoke-test hook. */
UCLASS()
class SHINOBICOOP_API AShinobiPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Console: ShinobiDamageSelf 50 — server applies damage to this player's pawn. */
	UFUNCTION(Exec)
	void ShinobiDamageSelf(float Amount);

	/** Console: ShinobiDrainChakra 60 — server removes chakra from this player's pawn. */
	UFUNCTION(Exec)
	void ShinobiDrainChakra(float Amount);

	/** Console: ShinobiCast 1..4 -- taps that jutsu's input, same as pressing its key. */
	UFUNCTION(Exec)
	void ShinobiCast(int32 JutsuSlot);

	/** Console: ShinobiCastHold 1..4 Seconds -- holds that jutsu's key for Seconds (seals, then charge), then releases. */
	UFUNCTION(Exec)
	void ShinobiCastHold(int32 JutsuSlot, float Seconds);

	/** Console: ShinobiEquipPage 1..5 -- puts one of the old five jutsu pages into slots 1-4 (used by the screenshot scripts). */
	UFUNCTION(Exec)
	void ShinobiEquipPage(int32 Page);

	/** Console: ShinobiDumpStats -- logs every character's attributes (and whether it's burning) as this machine sees them. */
	UFUNCTION(Exec)
	void ShinobiDumpStats();

	/** Console: ShinobiCharacter Hero -- play as that roster character (AShinobiPlayerCharacter::Roster). The settings
	 * menu's Character row calls this after saving the pick; on its own it lasts for this session only. */
	UFUNCTION(Exec)
	void ShinobiCharacter(const FString& Name);

	/** Console: ShinobiFace RealityScan -- the played roster character's face of that name (AShinobiPlayerCharacter::Faces).
	 * The settings menu's Face row calls this after saving the pick; on its own it lasts for this session only. */
	UFUNCTION(Exec)
	void ShinobiFace(const FString& Name);

	/** Console: ShinobiOutfit Hokage -- wear that outfit (AShinobiPlayerCharacter::Outfits; None takes it off). The settings
	 * menu's Outfit row calls this after saving the pick; on its own it lasts for this session only. */
	UFUNCTION(Exec)
	void ShinobiOutfit(const FString& Name);

	/** Console (local live edit, the creator's preview): ShinobiBodyShape <Slider> <-1..1> -- one body slider on the worn
	 * creator look (Height, Shoulders, ...). */
	UFUNCTION(Exec)
	void ShinobiBodyShape(const FString& Slider, float Value);

	/** Console (local live edit): ShinobiFaceBlend <face> <0..1> [<face> <0..1>] -- blend the worn creator face toward
	 * those faces (catalog indices within the sex; -1 = off). */
	UFUNCTION(Exec)
	void ShinobiFaceBlend(int32 FaceA, float WeightA, int32 FaceB = -1, float WeightB = 0.f);

	/** Console: ShinobiLook <sex 0/1> <build 0-2> <face> <hair> <beard> <mustache> <top> <bottom> <shoes> -- play a
	 * character-creator look (UShinobiCreatorCatalog indices within the sex; 255 = none), replicated, for this session. */
	UFUNCTION(Exec)
	void ShinobiLook(int32 Sex, int32 Build, int32 Face, int32 Hair, int32 Beard, int32 Mustache, int32 Top, int32 Bottom, int32 Shoes);

	/** Console: ShinobiLookCode <code> -- play the look a preset code holds (FShinobiAppearance::ToCode); with no code,
	 * prints the worn look's (live edits included). */
	UFUNCTION(Exec)
	void ShinobiLookCode(const FString& Code);

	/** Server: play as a creator-made character (sanitized: a client's value); given to every pawn it possesses. */
	UFUNCTION(Server, Reliable)
	void ServerPickAppearance(const FShinobiAppearance& Look);

	/** Local: open or close the pause menu (Esc / Start). */
	void TogglePauseMenu();

	/** Local: open or close the big map (M / View). */
	void ToggleMap();

	/** Local: the last button pressed was a gamepad's (the HUD shows A/B/X/Y labels and the pad page). */
	bool IsUsingGamepad() const { return bUsingGamepad; }

	/** Local: open or close the Skills screen (K, or the pause menu's Jutsu entry). */
	void ToggleSkillsScreen();

	bool IsBigMapOpen() const;

	/** Local: zoom the big map while it is open, else the minimap. Steps > 0 zooms in; with bWrap, past the closest
	 * level it goes back to the whole map. */
	void ZoomMap(int32 Steps, bool bWrap);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	/** Notes whether the last button came from a gamepad (IsUsingGamepad). */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	void OnMapZoom(const FInputActionValue& Value);
	/** Server: the new pawn plays the character this player picked (so the pick survives respawns). */
	virtual void OnPossess(APawn* InPawn) override;
	/** Local: the pawn we control changed, so every character's outline (hostile = red) is redrawn. */
	virtual void AcknowledgePossession(APawn* P) override;

private:
	bool bUsingGamepad = false;

	UFUNCTION(Server, Reliable)
	void ServerDamageSelf(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDrainChakra(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerPickCharacter(uint8 Index, uint8 Face);

	UFUNCTION(Server, Reliable)
	void ServerPickOutfit(uint8 Index);

	/** Server: this player's roster pick, given to every pawn it possesses. */
	uint8 PickedCharacter = 0;
	/** Server: which of that character's faces. */
	uint8 PickedFace = 0;

	/** Server: this player's outfit pick, likewise. */
	uint8 PickedOutfit = 0;

	/** Server: the creator-made look played while PickedCharacter is AShinobiPlayerCharacter::CustomCharacter. */
	FShinobiAppearance PickedAppearance;

	/** Server only: applies an instant SetByCaller effect to this controller's own pawn. */
	void ApplyToOwnPawn(TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Magnitude);

	/** With -ShinobiSmoke on a client: damage self, throw a Fireball at the dummy ahead, dump stats, quit. Used by Scripts/smoke_m1.ps1. */
	void RunSmokeTest();

	/** With -ShinobiShot: turn the camera to face the pawn, hold chakra charge (screenshots), release, then throw a
	 * Fireball and retry it on cooldown for a HUD screenshot (cooldown + red flash), quit. Used by Scripts/shot_chakra.ps1. */
	/** -ShinobiFollowTest: summons a pack on whatever map this is, walks away from it, and logs how far each summon is (Scripts/follow_london.ps1). */
	void RunFollowTest();

	void RunShotTest();

	/** -ShinobiHitProbe: a hit reaction at several times after launch, logging how far the head moves on the driver and on
	 * the worn body ("ShinobiHitProbe" lines), then quits. */
	void RunHitProbe();

	/** With -ShinobiJutsuShot: casts each jutsu at the dummy ahead and screenshots every effect, then quits.
	 * Used by `Scripts/shot_chakra.ps1 -Jutsu`. */
	void RunJutsuShotTest();
	void RunFireShotTest();
	/** -ShinobiSusanooShot: the Perfect Susanoo from four sides, from the gameplay camera and through a melee slash (Scripts/shot_chakra.ps1 -Susanoo). */
	void RunSusanooShotTest();

	/** With -ShinobiLookShot (arena mode): screenshots of the map, the character and the first wave closing in, fighting
	 * the nearest enemy, then quits. Used by `Scripts/shot_chakra.ps1 -Look`. */
	void RunLookShotTest();
	/** -ShinobiOutfitShot=<Outfit>: that outfit on every roster body, idle, walking, running, jumping (ShinobiOutfitShot.cpp). */
	void RunOutfitShotTest(const FString& Outfit);
	/** -ShinobiPortraitShot=<Character>: that roster character's face up close from the front, three-quarter and side, and
	 * its whole body from the front (ShinobiPortraitShot.cpp; Scripts/shot_chakra.ps1 -Portrait <Character>). */
	void RunPortraitShotTest(const FString& Who);
	/** -ShinobiCreatorSpike: the character creator's phase-0 checks on film (ShinobiCreatorSpikeShot.cpp). */
	void RunCreatorSpikeShot();
	/** -ShinobiFaceSliderShot: every face slider at -1 and +1 on a creator look (ShinobiCreatorSpikeShot.cpp). */
	void RunFaceSliderShot();
	/** -ShinobiFacePresetShot: every catalog face, front and three-quarter (ShinobiCreatorSpikeShot.cpp). */
	void RunFacePresetShot();
	/** -ShinobiFaceBlendShot: preset blending steps between two faces (ShinobiCreatorSpikeShot.cpp). */
	void RunFaceBlendShot();
	/** -ShinobiBodySliderShot: every body slider at -1 and +1 (ShinobiCreatorSpikeShot.cpp). */
	void RunBodySliderShot();
	/** -ShinobiCreatorOutfitShot: each cloak over every creator frame, front and side (ShinobiCreatorSpikeShot.cpp). */
	void RunCreatorOutfitShot();
	/** What RunOutfitShotTest preloads, kept loaded for the run. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> OutfitShotAssets;

	/** With -ShinobiWorldTour: flies the (hovering) pawn to each tour point of the open world (the PlayerStart, actors
	 * tagged Tour, camp and boss markers tagged ShinobiCampSite), waits for streaming and screenshots it, then quits.
	 * Used by `Scripts/shot_chakra.ps1 -Tour`. */
	void RunWorldTourTest();

	UPROPERTY()
	TObjectPtr<UShinobiHUDWidget> Hud;

	UPROPERTY()
	TObjectPtr<UShinobiPauseMenuWidget> PauseMenu;

	UPROPERTY()
	TObjectPtr<UShinobiSkillsWidget> SkillsWidget;
};
