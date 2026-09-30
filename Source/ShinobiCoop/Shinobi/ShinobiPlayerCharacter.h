#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "ShinobiPlayerCharacter.generated.h"

class UCameraComponent;
class UShinobiOverheadTextComponent;
class USpringArmComponent;
struct FInputActionValue;

/** A pickable player character: its visible body and the retargeter that copies the hidden Manny's pose onto it. */
struct FShinobiRosterEntry
{
	const TCHAR* Name;
	const TCHAR* Mesh;
	const TCHAR* Retargeter;
};

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

	/** Every character a player can pick (Scripts/setup_hero.py builds them; heroes from photos or VRoid get added here,
	 * the generated NPC cast lives on the enemy types and AShinobiVillager instead). Entry 0 is the default hero and the
	 * fallback for a pick whose assets aren't imported. */
	static constexpr FShinobiRosterEntry Roster[] = {
		{TEXT("Hero"), TEXT("/Game/Shinobi/Hero/SK_Hero.SK_Hero"), TEXT("/Game/Shinobi/Hero/RTG_MannyToHero.RTG_MannyToHero")},
	};

	/** Roster index of Name (case-insensitive), or INDEX_NONE. */
	static int32 FindRosterIndex(const FString& Name);

	/** Server: play as Roster[Index] (replicated; out of range or not imported = the hero). */
	void SetCharacterIndex(uint8 Index);
	uint8 GetCharacterIndex() const { return CharacterIndex; }

	/** The hero mesh when the hero is in use (Shinobi.Hero 1 and its assets exist), else GetMesh(). */
	virtual USkeletalMeshComponent* GetBodyMesh() const override { return HeroMesh ? HeroMesh.Get() : GetMesh(); }

	/** Null when the player is the plain visible Manny (the fallback). */
	USkeletalMeshComponent* GetHeroMesh() const { return HeroMesh; }
	UShinobiOverheadTextComponent* GetNameTag() const { return NameTag; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void UnPossessed() override;
	/** At 0 HP a player goes down (GE_Downed) instead of dying. */
	virtual void HandleOutOfHealth() override;
	/** Below the world's kill-Z: back to a player start instead of being destroyed. */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

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

	/** Puts the picked character (else the hero) on HeroMesh; false when neither's assets load or HeroMesh is gone. */
	bool ApplyCharacter();

	UFUNCTION()
	void OnRep_CharacterIndex() { ApplyCharacter(); }

	/** Roster index of the body shown; the server sets it from the owning controller's pick. */
	UPROPERTY(ReplicatedUsing = OnRep_CharacterIndex)
	uint8 CharacterIndex = 0;

	/** The visible hero body, pose-driven from GetMesh(). */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiBodyComponent> HeroMesh;

	/** Teammates' names above their heads; hidden on your own character. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiOverheadTextComponent> NameTag;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};
