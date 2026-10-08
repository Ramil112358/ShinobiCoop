#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShinobiAIController.generated.h"

class AShinobiCharacterBase;
class AShinobiEnemyCharacter;

/**
 * Server-side enemy brain, ticked: pick the nearest living player (re-checked every RetargetInterval), focus it,
 * close in (ranged enemies hold ~PreferredRange), attack every AttackInterval once in range; hold still while an
 * ability runs or stunned, and target nobody while held for a PvP round's countdown (State.PvpHold). Paths with MoveToActor where there's a navmesh (Training Grounds, generated at runtime);
 * steers straight at the target with AddMovementInput where there isn't (Lvl_ThirdPerson, tests).
 */
UCLASS()
class SHINOBICOOP_API AShinobiAIController : public AAIController
{
	GENERATED_BODY()

public:
	AShinobiAIController();

	virtual void Tick(float DeltaSeconds) override;

	static constexpr float RetargetInterval = 0.5f;

	/** Whether it is going after (or fighting) someone, as opposed to idling. */
	bool HasTarget() const { return Target.IsValid(); }

private:
	AShinobiCharacterBase* FindNearestPlayer(const AShinobiEnemyCharacter& Self) const;

	/** With nothing to fight: walks to Post (paths where there is a navmesh, else steers straight), standing once there. */
	void GoToPost(AShinobiEnemyCharacter& Self, const FVector& Post);

	TWeakObjectPtr<AShinobiCharacterBase> Target;
	float RetargetClock = 0.f;
	float NextAttackTime = 0.f;
	float NextSpecialTime = 0.f;
	/** The last MoveToActor failed (no navmesh or no path): steer directly until the next retarget. */
	bool bSteerDirectly = false;
	/** Where the last path to an idle post was asked for; asked again when the post has moved on (its caster walks). */
	FVector PostGoal = FVector::ZeroVector;
};
