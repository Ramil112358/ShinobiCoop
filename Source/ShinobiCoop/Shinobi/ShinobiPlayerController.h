#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShinobiPlayerController.generated.h"

class UGameplayEffect;
class UShinobiHUDWidget;
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

	/** Console: ShinobiDrainChakra 60 — server removes chakra from this player's pawn (nothing spends chakra until M2's jutsu). */
	UFUNCTION(Exec)
	void ShinobiDrainChakra(float Amount);

	/** Console: ShinobiDumpStats — logs every character's attributes as this machine sees them. */
	UFUNCTION(Exec)
	void ShinobiDumpStats();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerDamageSelf(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDrainChakra(float Amount);

	/** Server only: applies an instant SetByCaller effect to this controller's own pawn. */
	void ApplyToOwnPawn(TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Magnitude);

	/** With -ShinobiSmoke on a client: damage self, dump stats, quit. Used by Scripts/smoke_m1.ps1. */
	void RunSmokeTest();

	/** With -ShinobiShot: turn the camera to face the pawn, hold chakra charge, screenshot, release, quit. Used by Scripts/shot_chakra.ps1. */
	void RunShotTest();

	UPROPERTY()
	TObjectPtr<UShinobiHUDWidget> Hud;
};
