#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShinobiPlayerController.generated.h"

class UGameplayEffect;
class UShinobiHUDWidget;
class UShinobiPauseMenuWidget;
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

	/** Console: ShinobiOutfit Hokage -- wear that outfit (AShinobiPlayerCharacter::Outfits; None takes it off). The settings
	 * menu's Outfit row calls this after saving the pick; on its own it lasts for this session only. */
	UFUNCTION(Exec)
	void ShinobiOutfit(const FString& Name);

	/** Local: open or close the pause menu (Esc / Start). */
	void TogglePauseMenu();

	/** Local: open or close the big map (M / View). */
	void ToggleMap();

	/** Local: open or close the Skills screen (K, or the pause menu's Jutsu entry). */
	void ToggleSkillsScreen();

	bool IsBigMapOpen() const;

	/** Local: zoom the big map while it is open, else the minimap. Steps > 0 zooms in; with bWrap, past the closest
	 * level it goes back to the whole map. */
	void ZoomMap(int32 Steps, bool bWrap);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	void OnMapZoom(const FInputActionValue& Value);
	/** Server: the new pawn plays the character this player picked (so the pick survives respawns). */
	virtual void OnPossess(APawn* InPawn) override;
	/** Local: the pawn we control changed, so every character's outline (hostile = red) is redrawn. */
	virtual void AcknowledgePossession(APawn* P) override;

private:
	UFUNCTION(Server, Reliable)
	void ServerDamageSelf(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDrainChakra(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerPickCharacter(uint8 Index);

	UFUNCTION(Server, Reliable)
	void ServerPickOutfit(uint8 Index);

	/** Server: this player's roster pick, given to every pawn it possesses. */
	uint8 PickedCharacter = 0;

	/** Server: this player's outfit pick, likewise. */
	uint8 PickedOutfit = 0;

	/** Server only: applies an instant SetByCaller effect to this controller's own pawn. */
	void ApplyToOwnPawn(TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Magnitude);

	/** With -ShinobiSmoke on a client: damage self, throw a Fireball at the dummy ahead, dump stats, quit. Used by Scripts/smoke_m1.ps1. */
	void RunSmokeTest();

	/** With -ShinobiShot: turn the camera to face the pawn, hold chakra charge (screenshots), release, then throw a
	 * Fireball and retry it on cooldown for a HUD screenshot (cooldown + red flash), quit. Used by Scripts/shot_chakra.ps1. */
	/** -ShinobiFollowTest: summons a pack on whatever map this is, walks away from it, and logs how far each summon is (Scripts/follow_london.ps1). */
	void RunFollowTest();

	void RunShotTest();

	/** With -ShinobiJutsuShot: casts each jutsu at the dummy ahead and screenshots every effect, then quits.
	 * Used by `Scripts/shot_chakra.ps1 -Jutsu`. */
	void RunJutsuShotTest();
	void RunFireShotTest();
	/** -ShinobiSusanooShot: the Perfect Susanoo from four sides, from the gameplay camera and through a melee slash (Scripts/shot_chakra.ps1 -Susanoo). */
	void RunSusanooShotTest();

	/** With -ShinobiLookShot (arena mode): screenshots of the map, the character and the first wave closing in, fighting
	 * the nearest enemy, then quits. Used by `Scripts/shot_chakra.ps1 -Look`. */
	void RunLookShotTest();

	/** With -ShinobiWorldTour: flies the (hovering) pawn to each tour point of the open world (the PlayerStart, actors
	 * tagged Tour, camp and boss markers tagged ShinobiCampSite), waits for streaming and screenshots it, then quits.
	 * Used by `Scripts/shot_chakra.ps1 -Tour`. */
	void RunWorldTourTest();

	UPROPERTY()
	TObjectPtr<UShinobiHUDWidget> Hud;

	UPROPERTY()
	TObjectPtr<UShinobiPauseMenuWidget> PauseMenu;
};
