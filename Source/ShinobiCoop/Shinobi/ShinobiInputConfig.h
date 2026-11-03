#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "ShinobiInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/** Priority IMC_Shinobi is added at. */
constexpr int32 ShinobiInputPriority = 1;

/** IMC_ShinobiJutsu goes above IMC_Shinobi so, while RB is held, the jutsu slots' buttons win over jump/dodge/melee. */
constexpr int32 ShinobiJutsuInputPriority = ShinobiInputPriority + 1;

/** All input actions and the mapping context, constructed in code so no input assets are needed. */
UCLASS()
class SHINOBICOOP_API UShinobiInputConfig : public UObject
{
	GENERATED_BODY()

public:
	/** Builds a fresh config; each call creates its own new UInputAction/UInputMappingContext objects. */
	static UShinobiInputConfig* Build(UObject* Outer);

	/** Process-wide singleton, rooted so it behaves like a static input asset. Re-adding the same
	 * UInputMappingContext object to a controller's subsystem replaces its existing entry instead of
	 * layering a duplicate, so every (re)possession must go through the same instance -- a fresh
	 * Build() per possession would let a stale, unbound context win priority ties after a re-possess. */
	static const UShinobiInputConfig& Get();

	/** Puts ShinobiKeyBindings' keyboard/mouse keys on the rebindable actions in Context (their gamepad keys stay), Alt
	 * bindings chorded on AltModifier, and each jutsu slot's pad button in JutsuContext, then has every local player rebuild
	 * its mappings. Build() and every rebind call it. */
	void ApplyKeyBindings() const;

	/** The action a ShinobiKeyBindings::EBindable drives (cast from uint8 so this header needn't include it). */
	UInputAction* ActionForBindable(uint8 Bindable) const;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> Context;

	UPROPERTY()
	TObjectPtr<UInputAction> Move;

	UPROPERTY()
	TObjectPtr<UInputAction> Look;

	/** Right mouse: temporarily look around without changing gameplay aim. */
	UPROPERTY()
	TObjectPtr<UInputAction> CameraFreeLook;

	UPROPERTY()
	TObjectPtr<UInputAction> Jump;

	/** Gamepad RB: while held, the player adds JutsuContext. */
	UPROPERTY()
	TObjectPtr<UInputAction> JutsuModifier;

	/** Tab / R3: lock the camera onto the enemy nearest the crosshair (again to release). */
	UPROPERTY()
	TObjectPtr<UInputAction> LockOn;

	/** Esc / Start: pause menu (bound by AShinobiPlayerController, so it works while downed or dead). */
	UPROPERTY()
	TObjectPtr<UInputAction> Pause;

	/** Mouse wheel: adjust the player's camera distance (zooms the big map instead while it is open). */
	UPROPERTY()
	TObjectPtr<UInputAction> CameraZoom;

	/** M (View button): open or close the big map. */
	UPROPERTY()
	TObjectPtr<UInputAction> MapToggle;

	/** = / - (numpad + / -, D-pad up): zoom the open map, +1 in, -1 out. */
	UPROPERTY()
	TObjectPtr<UInputAction> MapZoom;

	/** Each jutsu slot's pad button (ShinobiKeyBindings::GetPad) on that slot's action. Only active while RB is held. */
	UPROPERTY()
	TObjectPtr<UInputMappingContext> JutsuContext;

	/** Left/Right Alt: the chord for Alt + key bindings. */
	UPROPERTY()
	TObjectPtr<UInputAction> AltModifier;

	/** K: the Skills screen (bound by AShinobiPlayerController). */
	UPROPERTY()
	TObjectPtr<UInputAction> SkillsScreen;

	UPROPERTY()
	TMap<EShinobiAbilityInputID, TObjectPtr<UInputAction>> AbilityActions;
};
