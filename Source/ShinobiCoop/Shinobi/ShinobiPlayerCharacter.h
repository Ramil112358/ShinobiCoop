#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "ShinobiPlayerCharacter.generated.h"

class UCameraComponent;
class UShinobiOverheadTextComponent;
class USpringArmComponent;
struct FInputActionValue;

/** Player-controlled shinobi: third-person camera, input bound to abilities. */
UCLASS()
class SHINOBICOOP_API AShinobiPlayerCharacter : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	/** False while charging chakra, shocked, staggered, reviving, downed or dead: move and jump input are ignored. */
	bool CanMove() const;

	static constexpr float ReviveHealthFraction = 0.3f;

	/** Server: back up from downed at ReviveHealthFraction of max HP (GA_Revive; the game mode at a wave's end). */
	void Revive();

	/** Seconds left before a downed player bleeds out (0 when not downed). Needs the downed effect: owner/server only. */
	float GetBleedOutRemaining() const;

	static constexpr float LockOnRange = 3000.f;
	static constexpr float LockOnHalfAngle = 35.f;

	/** Local player: the enemy the camera is locked onto, if any. */
	AShinobiCharacterBase* GetLockTarget() const { return LockTarget.Get(); }

	/** Local player: lock onto the living enemy nearest the crosshair (within LockOnHalfAngle and LockOnRange), or release. */
	void ToggleLockOn();

	virtual void Tick(float DeltaSeconds) override;

	/** The hero mesh when the hero is in use (Shinobi.Hero 1 and its assets exist), else GetMesh(). */
	virtual USkeletalMeshComponent* GetBodyMesh() const override { return HeroMesh ? HeroMesh.Get() : GetMesh(); }

	/** Null when the player is the plain visible Manny (the fallback). */
	USkeletalMeshComponent* GetHeroMesh() const { return HeroMesh; }
	UShinobiOverheadTextComponent* GetNameTag() const { return NameTag; }

	static constexpr const TCHAR* HeroMeshPath = TEXT("/Game/Shinobi/Hero/SK_Hero.SK_Hero");

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void UnPossessed() override;
	/** At 0 HP a player goes down (GE_Downed) instead of dying. */
	virtual void HandleOutOfHealth() override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	/** Ignores the jump input while CanMove() is false. */
	void JumpPressed();
	void AbilityInputPressed(EShinobiAbilityInputID InputID);
	void AbilityInputReleased(EShinobiAbilityInputID InputID);
	/** RB held: adds IMC_ShinobiJutsu (A/B/X/Y cast jutsu); released: removes it. */
	void SetJutsuModifier(bool bHeld);
	TWeakObjectPtr<AShinobiCharacterBase> LockTarget;

	/** Every machine: hold the downed pose. Server: the effect ran out with HP still 0, so bled out: dead. */
	void OnDownedTagChanged(const FGameplayTag Tag, int32 Count);

	/** Uses the hero if Shinobi.Hero is on and its mesh and retargeter exist: the Manny mesh becomes the hidden
	 * animation driver and HeroMesh copies its pose. Otherwise removes HeroMesh (plain visible Manny). Before
	 * Super::BeginPlay, so components' BeginPlay (aura, cast orb) already see the right GetBodyMesh(). */
	void ApplyHero();

	/** The visible hero body, pose-driven from GetMesh() by UShinobiRetargetAnimInstance. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USkeletalMeshComponent> HeroMesh;

	/** Teammates' names above their heads; hidden on your own character. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiOverheadTextComponent> NameTag;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};
