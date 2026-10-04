#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShinobiAIController.generated.h"

class AShinobiCharacterBase;
class AShinobiEnemyCharacter;

/**
 * Server-side enemy brain, ticked: pick the nearest living player (re-checked every RetargetInterval), focus it,
 * close in (ranged enemies hold ~PreferredRange), attack every AttackInterval once in range; hold still while an
 * ability runs or stunned. Paths with MoveToActor where there's a navmesh (Training Grounds, generated at runtime);
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

private:
	AShinobiCharacterBase* FindNearestPlayer(const AShinobiEnemyCharacter& Self) const;

	/** With nothing to fight: walk back to the Self's follow target (a summon's caster) and wait beside it. */
	void FollowLeader(AShinobiEnemyCharacter& Self);

	TWeakObjectPtr<AShinobiCharacterBase> Target;
	float RetargetClock = 0.f;
	bool bHadTarget = false;
	float NextAttackTime = 0.f;
	/** The last MoveToActor failed (no navmesh or no path): steer directly until the next retarget. */
	bool bSteerDirectly = false;
};
