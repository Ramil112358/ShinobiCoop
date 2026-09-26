#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "ShinobiTrainingDummy.generated.h"

class UTextRenderComponent;

/**
 * Enemy-team punching bag. Shows its health (and, until M2b's cue VFX, its statuses) above its head and resets
 * 1.5 s after being defeated. With the dev cvar Shinobi.DummyAttack on, it punches the nearest player within reach
 * every 2 s, so Substitution, jutsu interrupts and the chakra-charge cancel can be tried before M3's enemies.
 */
UCLASS()
class SHINOBICOOP_API AShinobiTrainingDummy : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiTrainingDummy(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	static constexpr float PunchInterval = 2.f;
	static constexpr float PunchTelegraph = 0.4f;
	static constexpr float PunchReach = 250.f;
	static constexpr float PunchDamage = 25.f;

protected:
	virtual void HandleOutOfHealth() override;

private:
	void ResetHealth();

	/** Rewrites HealthText only when the rounded HP, max HP or status list changed. */
	void RefreshText();

	/** Server: lock the nearest hostile within PunchReach as the punch target and face it. */
	void LockPunchTarget();

	/** Server only: PunchDamage to the locked target if it's still within PunchReach, or invulnerable (a perfect
	 * dodge, which triggers Substitution). */
	void Punch();

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UTextRenderComponent> HealthText;

	FTimerHandle ResetTimer;

	/** Last values written to HealthText, so SetText (which always marks render state dirty) only runs on a change. */
	int32 DisplayedHealth = -1;
	int32 DisplayedMaxHealth = -1;
	FString DisplayedStatuses;
	bool bDisplayedTelegraph = false;

	/** Time since the last punch while Shinobi.DummyAttack is on. */
	float PunchClock = 0.f;

	/** Server: the target locked when the telegraph starts (the punch is committed to it). */
	TWeakObjectPtr<AShinobiCharacterBase> PunchTarget;
};
