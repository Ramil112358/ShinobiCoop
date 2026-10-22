#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "ShinobiPlayerCharacter.generated.h"

class UCameraComponent;
class UShinobiJutsuAbility;
class UShinobiOverheadTextComponent;
class UShinobiSusanooComponent;
class UShinobiSurfaceFxComponent;
class UShinobiTraversalComponent;
class UMotionWarpingComponent;
class USpringArmComponent;
struct FInputActionValue;

/** A pickable player character: its visible body and the retargeter that copies the hidden Manny's pose onto it. */
struct FShinobiRosterEntry
{
	const TCHAR* Name;
	const TCHAR* Mesh;
	const TCHAR* Retargeter;
};

/** A wearable outfit: a garment fitted to each roster body by Scripts/character/build_outfits.ps1, one copy per body
 * (AShinobiPlayerCharacter::OutfitMeshPath), whose hem and sleeves are Chaos cloth. Entry 0, "None", is no outfit. */
struct FShinobiOutfitEntry
{
	const TCHAR* Name;
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

	/** Space may jump, vault or mantle: CanMove() and not guarding (a guard stays on the ground). */
	bool CanJumpOrTraverse() const;

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

	/** Knockback off a wall: detach first, so the launch happens with world gravity. */
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride) override;

	/** A single jump peaks at ~184 cm (JumpZVelocity 600), so only a double jump or a drop off a wall or roof rolls. */
	static constexpr float RollFallHeight = 230.f;

	/** Every machine (proxies get the replicated mode): landing from RollFallHeight or more below the fall's highest
	 * point plays the roll (AM_Land_Roll); taking off again ends it. */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	/** Every character a player can pick (Scripts/setup_hero.py builds them; heroes from photos or VRoid get added here,
	 * the generated NPC cast lives on the enemy types and AShinobiVillager instead). Entry 0 is the default hero and the
	 * fallback for a pick whose assets aren't imported. */
	static constexpr FShinobiRosterEntry Roster[] = {
		{TEXT("Hero"), TEXT("/Game/Shinobi/Hero/SK_Hero.SK_Hero"), TEXT("/Game/Shinobi/Hero/RTG_MannyToHero.RTG_MannyToHero")},
		{TEXT("Bika"), TEXT("/Game/Shinobi/Characters/Bika/SK_Bika.SK_Bika"), TEXT("/Game/Shinobi/Characters/Bika/RTG_MannyToBika.RTG_MannyToBika")},
		{TEXT("Cat"), TEXT("/Game/Shinobi/Characters/Cat/SK_Cat.SK_Cat"), TEXT("/Game/Shinobi/Characters/Cat/RTG_MannyToCat.RTG_MannyToCat")},
		{TEXT("Misha"), TEXT("/Game/Shinobi/Characters/Misha/SK_Misha.SK_Misha"), TEXT("/Game/Shinobi/Characters/Misha/RTG_MannyToMisha.RTG_MannyToMisha")},
		{TEXT("Pabu"), TEXT("/Game/Shinobi/Characters/Pabu/SK_Pabu.SK_Pabu"), TEXT("/Game/Shinobi/Characters/Pabu/RTG_MannyToPabu.RTG_MannyToPabu")},
		{TEXT("Ramil"), TEXT("/Game/Shinobi/Characters/Ramil/SK_Ramil.SK_Ramil"), TEXT("/Game/Shinobi/Characters/Ramil/RTG_MannyToRamil.RTG_MannyToRamil")},
	};

	/** Roster index of Name (case-insensitive), or INDEX_NONE. */
	static int32 FindRosterIndex(const FString& Name);

	/** Server: play as Roster[Index] (replicated; out of range or not imported = the hero). */
	void SetCharacterIndex(uint8 Index);
	uint8 GetCharacterIndex() const { return CharacterIndex; }

	/** Every outfit a player can wear over any roster character (Scripts/setup_hero.py imports them from
	 * SourceArt/Character/Outfits; docs/character/outfit-steps.md). Entry 0 is "no outfit" and the fallback. */
	static constexpr FShinobiOutfitEntry Outfits[] = {
		{TEXT("None")},
		{TEXT("Hokage")},
		{TEXT("Akatsuki")},
	};

	/** The copy of Outfits[Outfit] fitted to Roster[Body]: /Game/Shinobi/Outfits/<Outfit>/SK_<Outfit>_<Body>. */
	static FString OutfitMeshPath(int32 Outfit, int32 Body);
	/** What of Roster[Body] that copy covers (black = hidden; the body's M_Character HideMask): T_<Outfit>_<Body>_Hide. */
	static FString OutfitHidePath(int32 Outfit, int32 Body);
	/** Cloth further than this from the local camera (or unseen for a second) stops simulating. */
	static constexpr float OutfitClothRangeCm = 4000.f;

	/** Outfits index of Name (case-insensitive), or INDEX_NONE. */
	static int32 FindOutfitIndex(const FString& Name);

	/** Server: wear Outfits[Index] (replicated; out of range or not imported = no outfit). */
	void SetOutfitIndex(uint8 Index);
	uint8 GetOutfitIndex() const { return OutfitIndex; }

	/** The outfit mesh; it shows a mesh only while an outfit is worn. Null when the hero isn't in use. */
	USkeletalMeshComponent* GetOutfitMesh() const { return OutfitMesh; }

	/** The hero mesh when the hero is in use (Shinobi.Hero 1 and its assets exist), else GetMesh(). */
	virtual USkeletalMeshComponent* GetBodyMesh() const override { return HeroMesh ? HeroMesh.Get() : GetMesh(); }

	/** Outlines the hero and its outfit. */
	virtual void ApplyOutlineFor(const AShinobiCharacterBase* Viewer) override;

	/** Null when the player is the plain visible Manny (the fallback). */
	USkeletalMeshComponent* GetHeroMesh() const { return HeroMesh; }
	UShinobiOverheadTextComponent* GetNameTag() const { return NameTag; }

	/** The ribcage that stands round the player while they guard (UGA_Block). */
	UShinobiSusanooComponent* GetGuard() const { return Guard; }

	/** The jutsu in each of the 8 bar slots (ShinobiLoadout; null = empty). Slot i fires on ShinobiLoadout::SlotInputID(i). */
	const ShinobiLoadout::FSlots& GetLoadout() const { return Loadout; }

	/** Authority, no swap rules: sets the loadout and stamps every jutsu spec's input slot. Tests and cheats use it directly. */
	void ApplyLoadout(const ShinobiLoadout::FSlots& Slots);

	/** Points every jutsu spec (and each follow-up, FollowsSlotOf) at its slot's input, INDEX_NONE when unslotted. Every machine runs it. */
	void StampLoadoutInputIDs();

	/** Owner: asks for Slots (the Skills screen). The server may refuse; the replicated loadout then stays. With bSave, an
	 * accepted change becomes this machine's saved bar (a refused one never does). */
	void RequestLoadout(const ShinobiLoadout::FSlots& Slots, bool bSave = true);

	/** Authority: applies Slots if the swap rules allow it (GetLoadoutLockReason, IsSlotLocked). */
	bool TryChangeLoadout(const ShinobiLoadout::FSlots& Slots);

	/** Why the whole loadout can't change right now (casting, mounted, downed, in combat, a PvP round); empty when it can. */
	FText GetLoadoutLockReason() const;

	/** The jutsu in SlotIndex can't leave it right now (cooling down, Kamui phased, a summon out...). Empty slots never are. */
	bool IsSlotLocked(int32 SlotIndex) const;

	/** Seconds after the last hit dealt to, or taken from, a hostile that the player counts as in combat (no jutsu swaps). */
	static constexpr float CombatSeconds = 5.f;

	/** Authority: a hit was dealt or taken (ShinobiCombat::NoteCombat). */
	void NoteCombat();
	bool IsInCombat() const { return GetCombatSecondsLeft() > 0.f; }
	/** Until swaps unlock again; 0 out of combat. */
	float GetCombatSecondsLeft() const;

	/** Every jutsu this player has been granted, in grant order: the Skills screen's pool. */
	TArray<TSubclassOf<UShinobiJutsuAbility>> GetKnownJutsu() const;

	/** The pad's jutsu page: 1 = slots 1-4 on RB + A/B/X/Y, 2 = slots 5-8. Local presentation only; nothing to replicate. */
	int32 GetPadJutsuPage() const { return PadJutsuPage; }
	void FlipPadJutsuPage(int32 Direction);
	/** RB + face Face (0 A, 1 B, 2 X, 3 Y): the slot on the current pad page. The release goes to the slot the press used. */
	void PadJutsuPressed(int32 Face);
	void PadJutsuReleased(int32 Face);
	/** The slot Face is holding down, or INDEX_NONE. */
	int32 GetPadPressedSlot(int32 Face) const { return Face >= 0 && Face < 4 ? PadPressedSlot[Face] : INDEX_NONE; }

	bool IsCastingJutsu() const;
	/** Cancels a local jutsu ground-target preview; false when no target placement is active. */
	bool CancelJutsuTargetPlacement();
	bool IsJutsuTargetPlacementActive() const { return bJutsuTargetPlacementActive; }
	FVector2D GetJutsuTargetScreenPosition() const { return JutsuTargetScreenPosition; }
	FVector GetJutsuTargetAimDirection() const;
	/** Routes the primary attack press/release to the active ground-targeted jutsu while placement is open. */
	bool RouteTargetPlacementInput(EShinobiAbilityInputID InputID, bool bPressed);
	void ApplyLookInput(const FVector2D& Axis);
	void BeginJutsuTargetPlacement();
	void EndJutsuTargetPlacement();

	UFUNCTION(Server, Reliable)
	void ServerSetJutsuTargetAimDirection(FVector Direction);

	/** Server: a client's loadout request. Invalid or locked changes are ignored (Loadout replicates the truth back). */
	UFUNCTION(Server, Reliable)
	void ServerRequestLoadout(const TArray<TSubclassOf<UShinobiJutsuAbility>>& Slots);

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
	/** Keeps Montage looping while the player has Tag, whenever no other montage (a jutsu, a dodge) is playing, and stops it when
	 * the tag goes. Tag-driven, so every machine shows it. Loads the montage once; a missing asset just means no pose. */
	void UpdateLoopPose(TObjectPtr<UAnimMontage>& Montage, bool& bLoadTried, const TCHAR* Path, const FGameplayTag& Tag, float BlendOutSeconds);

	/** While mounted: the seated loop (AM_Mount_Sit). */
	UPROPERTY()
	TObjectPtr<UAnimMontage> SeatedMontage;

	/** While guarding: the guard pose (AM_Block_Guard). */
	UPROPERTY()
	TObjectPtr<UAnimMontage> GuardMontage;

	/** The current fall's highest point (world Z); Tick raises it while falling. */
	float FallApexZ = 0.f;

	bool bBoomPulledBack = false;
	bool bSteeringByCamera = false;
	bool bSeatedLoadTried = false;
	bool bGuardLoadTried = false;
	bool bFlightClimb = false;
	bool bFlightDive = false;
	bool bJutsuTargetPlacementActive = false;
	bool bCameraFreeLookActive = false;
	bool bHasServerJutsuAimDirection = false;
	int32 TargetPlacementInputID = INDEX_NONE;
	int32 PadJutsuPage = 1;
	int32 PadPressedSlot[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
	FVector2D JutsuTargetScreenPosition = FVector2D(0.5f, 0.5f);
	FVector ServerJutsuAimDirection = FVector::ForwardVector;
	FRotator CameraFreeLookRotation = FRotator::ZeroRotator;

	static constexpr float FootArmLength = 450.f;
	static constexpr float MountedArmLength = 900.f;
	static constexpr float SusanooArmLength = 1600.f; // the camera boom while the Perfect Susanoo stands (12 m tall)
	static constexpr float SusanooLookHeight = 300.f; // and how far above the character it looks
	static constexpr float MinCameraArmLength = 200.f;
	static constexpr float MaxCameraArmLength = 1200.f;
	static constexpr float CameraZoomStep = 100.f;
	static constexpr float CameraFreeLookMinPitch = -70.f;
	static constexpr float CameraFreeLookMaxPitch = 80.f;
	float CameraZoomDistance = FootArmLength;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void SetCameraFreeLook(bool bHeld);
	void ApplyCameraFreeLookInput(const FVector2D& Axis);
	void MoveJutsuTargetCursor(const FVector2D& Axis);
	void ZoomCamera(const FInputActionValue& Value);
	/** Ignores the jump input while CanMove() is false. */
	void JumpPressed();
	void JumpReleased();
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

	/** Puts the copy of the picked outfit fitted to the worn body on OutfitMesh, following HeroMesh's pose, and hides what
	 * it covers of the body (after every body change). A body without a copy wears nothing. */
	void ApplyOutfit();
	/** The outfit's cloth: along the character's gravity (wall-runs), and asleep when far from the camera or unseen. */
	void UpdateOutfitCloth();

	UFUNCTION()
	void OnRep_OutfitIndex() { ApplyOutfit(); }

	/** Roster index of the body shown; the server sets it from the owning controller's pick. */
	UPROPERTY(ReplicatedUsing = OnRep_CharacterIndex)
	uint8 CharacterIndex = 0;

	/** Outfits index worn over the body; set from the owning controller's pick like CharacterIndex. */
	UPROPERTY(ReplicatedUsing = OnRep_OutfitIndex)
	uint8 OutfitIndex = 0;
	/** Roster index of the body actually worn (the pick, or the hero when the pick isn't imported). */
	uint8 WornBodyIndex = 0;
	bool bOutfitGravityOverridden = false;

	/** Owner and server: the jutsu in each bar slot. Each machine stamps its own spec input slots from it (StampLoadoutInputIDs). */
	UPROPERTY(ReplicatedUsing = OnRep_Loadout)
	TArray<TSubclassOf<UShinobiJutsuAbility>> Loadout;

	/** Owner client: the requested loadout to save once the server's answer matches it. */
	ShinobiLoadout::FSlots PendingSave;

	/** Server world time of the last hit dealt or taken (owner and server). */
	UPROPERTY(Replicated)
	float LastCombatTime = -1000.f;

	/** The server's clock where there is a game state (the owner's copy of it), else this world's (tests). */
	float ServerNow() const;

	UFUNCTION()
	void OnRep_Loadout();

	/** "ShinobiLoadout <name> (server|client): slot1=GA_Fireball(ok) ..." -- read by Scripts/smoke_m4.ps1. */
	void LogLoadout() const;

	/** The visible hero body, pose-driven from GetMesh(). */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiBodyComponent> HeroMesh;

	/** The worn outfit: follows HeroMesh's pose through leader-pose (bones match by name), so it needs no animation of its own. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USkeletalMeshComponent> OutfitMesh;

	/** Teammates' names above their heads; hidden on your own character. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiOverheadTextComponent> NameTag;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiSusanooComponent> Guard;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiSurfaceFxComponent> SurfaceFx;

	/** Vault, hurdle and mantle on jump (the Game Animation Sample's traversal), steered onto the ledges by MotionWarping. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiTraversalComponent> Traversal;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};
