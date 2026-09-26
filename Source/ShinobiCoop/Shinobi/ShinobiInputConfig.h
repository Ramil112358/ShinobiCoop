#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "ShinobiInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/** Priority IMC_Shinobi is added at. */
constexpr int32 ShinobiInputPriority = 1;

/** IMC_ShinobiJutsu goes above IMC_Shinobi so, while RB is held, its face buttons win over jump/dodge/melee. */
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

	UPROPERTY()
	TObjectPtr<UInputMappingContext> Context;

	UPROPERTY()
	TObjectPtr<UInputAction> Move;

	UPROPERTY()
	TObjectPtr<UInputAction> Look;

	UPROPERTY()
	TObjectPtr<UInputAction> Jump;

	/** Gamepad RB: while held, the player adds JutsuContext. */
	UPROPERTY()
	TObjectPtr<UInputAction> JutsuModifier;

	/** Tab / R3: lock the camera onto the enemy nearest the crosshair (again to release). */
	UPROPERTY()
	TObjectPtr<UInputAction> LockOn;

	/** Gamepad A/B/X/Y -> Jutsu 1-4. Only active while RB is held. */
	UPROPERTY()
	TObjectPtr<UInputMappingContext> JutsuContext;

	UPROPERTY()
	TMap<EShinobiAbilityInputID, TObjectPtr<UInputAction>> AbilityActions;
};
