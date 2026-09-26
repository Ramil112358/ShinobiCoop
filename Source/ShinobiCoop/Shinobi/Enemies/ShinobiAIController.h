#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShinobiAIController.generated.h"

class AShinobiCharacterBase;
class AShinobiEnemyCharacter;

/**
 * Server-side enemy brain, ticked: pick the nearest living player (re-checked every RetargetInterval), focus it,
 * close in (ranged enemies hold ~PreferredRange), attack every AttackInterval once in range; hold still while an
 * ability runs or stunned. ponytail: steers straight at the target with AddMovementInput (Lvl_ThirdPerson has no
 * navmesh); switch to MoveTo once M5's arena has one.
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

	TWeakObjectPtr<AShinobiCharacterBase> Target;
	float RetargetClock = 0.f;
	float NextAttackTime = 0.f;
};
