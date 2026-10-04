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

/** A wearable outfit: a cloak (skeletal mesh skinned to the Mixamo skeleton) that follows the worn body's pose, so one mesh
 * fits every roster character. Mesh is null for "no outfit". */
struct FShinobiOutfitEntry
{
	const TCHAR* Name;
	const TCHAR* Mesh;
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

	/** Flight while mounted: Space climbs, Control dives (both cancel). Ignored on the ground. */
	void SetFlightClimb(bool bHeld) { bFlightClimb = bHeld; }
	void SetFlightDive(bool bHeld) { bFlightDive = bHeld; }

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
		{TEXT("Bika"), TEXT("/Game/Shinobi/Characters/Bika/SK_Bika.SK_Bika"), TEXT("/Game/Shinobi/Characters/Bika/RTG_MannyToBika.RTG_MannyToBika")},
		{TEXT("Cat"), TEXT("/Game/Shinobi/Characters/Cat/SK_Cat.SK_Cat"), TEXT("/Game/Shinobi/Characters/Cat/RTG_MannyToCat.RTG_MannyToCat")},
		{TEXT("Misha"), TEXT("/Game/Shinobi/Characters/Misha/SK_Misha.SK_Misha"), TEXT("/Game/Shinobi/Characters/Misha/RTG_MannyToMisha.RTG_MannyToMisha")},
		{TEXT("Pabu"), TEXT("/Game/Shinobi/Characters/Pabu/SK_Pabu.SK_Pabu"), TEXT("/Game/Shinobi/Characters/Pabu/RTG_MannyToPabu.RTG_MannyToPabu")},
	};

	/** Roster index of Name (case-insensitive), or INDEX_NONE. */
	static int32 FindRosterIndex(const FString& Name);

	/** Server: play as Roster[Index] (replicated; out of range or not imported = the hero). */
	void SetCharacterIndex(uint8 Index);
	uint8 GetCharacterIndex() const { return CharacterIndex; }

	/** Every outfit a player can wear over any roster character (Scripts/setup_hero.py imports them from
	 * SourceArt/Character/Outfits; docs/character/outfit-steps.md). Entry 0 is "no outfit" and the fallback. */
	static constexpr FShinobiOutfitEntry Outfits[] = {
		{TEXT("None"), nullptr},
		{TEXT("Hokage"), TEXT("/Game/Shinobi/Outfits/Hokage/SK_Hokage.SK_Hokage")},
		{TEXT("Akatsuki"), TEXT("/Game/Shinobi/Outfits/Akatsuki/SK_Akatsuki.SK_Akatsuki")},
	};

	/** Outfits index of Name (case-insensitive), or INDEX_NONE. */
	static int32 FindOutfitIndex(const FString& Name);

	/** Server: wear Outfits[Index] (replicated; out of range or not imported = no outfit). */
	void SetOutfitIndex(uint8 Index);
	uint8 GetOutfitIndex() const { return OutfitIndex; }

	/** The outfit mesh; it shows a mesh only while an outfit is worn. Null when the hero isn't in use. */
	USkeletalMeshComponent* GetOutfitMesh() const { return OutfitMesh; }

	/** The hero mesh when the hero is in use (Shinobi.Hero 1 and its assets exist), else GetMesh(). */
	virtual USkeletalMeshComponent* GetBodyMesh() const override { return HeroMesh ? HeroMesh.Get() : GetMesh(); }

	/** Null when the player is the plain visible Manny (the fallback). */
	USkeletalMeshComponent* GetHeroMesh() const { return HeroMesh; }
	UShinobiOverheadTextComponent* GetNameTag() const { return NameTag; }

	/** The jutsu page keys 1-4 fire (1..ShinobiTags::NumJutsuPages). It is a loose tag on each machine's ability system:
	 * the owning client applies a switch at once so activation predicts, and tells the server. */
	int32 GetJutsuPage() const;

	/** Owner: switch to Page (ignored while a jutsu is active or for an invalid page) and tell the server. */
	void SetJutsuPage(int32 Page);

	/** Owner: the next (Direction +1) or previous (-1) page that has a jutsu, wrapping. False if casting or no other page has one. */
	bool TryCycleJutsuPage(int32 Direction);

	/** Local tags only: makes Page the current page. SetJutsuPage and the RPCs call it. */
	void ApplyJutsuPage(int32 Page);

	bool IsCastingJutsu() const;
	bool PageHasJutsu(int32 Page) const;

	/** Server: a client's page switch. Out-of-range values are ignored; during a cast the client is sent back the real page. */
	UFUNCTION(Server, Reliable)
	void ServerSetJutsuPage(uint8 Page);

	UFUNCTION(Client, Reliable)
	void ClientSetJutsuPage(uint8 Page);

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
	/** While mounted: keeps the seated loop (AM_Mount_Sit) playing whenever no other montage (a jutsu) is, and ends it on the dismount. */
	void UpdateSeatedPose();

	UPROPERTY()
	TObjectPtr<UAnimMontage> SeatedMontage;

	bool bBoomPulledBack = false;
	bool bSteeringByCamera = false;
	bool bSeatedLoadTried = false;
	bool bFlightClimb = false;
	bool bFlightDive = false;

	static constexpr float FootArmLength = 450.f;
	static constexpr float MountedArmLength = 900.f;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	/** Ignores the jump input while CanMove() is false. */
	void JumpPressed();
	void JumpReleased();
	void AbilityInputPressed(EShinobiAbilityInputID InputID);
	void AbilityInputReleased(EShinobiAbilityInputID InputID);
	void CycleJutsuPage(int32 Direction) { TryCycleJutsuPage(Direction); }
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

	/** Puts the picked outfit on OutfitMesh, following HeroMesh's pose (after every body change). */
	void ApplyOutfit();

	UFUNCTION()
	void OnRep_OutfitIndex() { ApplyOutfit(); }

	/** Roster index of the body shown; the server sets it from the owning controller's pick. */
	UPROPERTY(ReplicatedUsing = OnRep_CharacterIndex)
	uint8 CharacterIndex = 0;

	/** Outfits index worn over the body; set from the owning controller's pick like CharacterIndex. */
	UPROPERTY(ReplicatedUsing = OnRep_OutfitIndex)
	uint8 OutfitIndex = 0;

	/** The visible hero body, pose-driven from GetMesh(). */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiBodyComponent> HeroMesh;

	/** The worn outfit: follows HeroMesh's pose through leader-pose (bones match by name), so it needs no animation of its own. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USkeletalMeshComponent> OutfitMesh;

	/** Teammates' names above their heads; hidden on your own character. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiOverheadTextComponent> NameTag;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};
