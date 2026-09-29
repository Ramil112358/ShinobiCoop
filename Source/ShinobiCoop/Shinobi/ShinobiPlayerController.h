#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShinobiPlayerController.generated.h"

class UGameplayEffect;
class UShinobiHUDWidget;
class UShinobiPauseMenuWidget;
struct FGameplayTag;

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

	/** Console: ShinobiDumpStats -- logs every character's attributes (and whether it's burning) as this machine sees them. */
	UFUNCTION(Exec)
	void ShinobiDumpStats();

	/** Local: open or close the pause menu (Esc / Start). */
	void TogglePauseMenu();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerDamageSelf(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDrainChakra(float Amount);

	/** Server only: applies an instant SetByCaller effect to this controller's own pawn. */
	void ApplyToOwnPawn(TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Magnitude);

	/** With -ShinobiSmoke on a client: damage self, throw a Fireball at the dummy ahead, dump stats, quit. Used by Scripts/smoke_m1.ps1. */
	void RunSmokeTest();

	/** With -ShinobiShot: turn the camera to face the pawn, hold chakra charge (screenshots), release, then throw a
	 * Fireball and retry it on cooldown for a HUD screenshot (cooldown + red flash), quit. Used by Scripts/shot_chakra.ps1. */
	void RunShotTest();

	/** With -ShinobiJutsuShot: casts each jutsu at the dummy ahead and screenshots every effect, then quits.
	 * Used by `Scripts/shot_chakra.ps1 -Jutsu`. */
	void RunJutsuShotTest();

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
