#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "ShinobiEnemyCharacter.generated.h"

class AShinobiEnemyCharacter;
class UShinobiOverheadTextComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FShinobiEnemyDiedDelegate, AShinobiEnemyCharacter*);

/**
 * AI-driven enemy (AShinobiAIController). Its numbers live on the subclasses. At 0 HP it dies: State.Dead
 * (ragdoll on every machine), OnDied, destroyed DeathLifeSpan seconds later.
 */
UCLASS(Abstract)
class SHINOBICOOP_API AShinobiEnemyCharacter : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	static constexpr float DeathLifeSpan = 3.f;

	/** Server: fires once, when the enemy dies. */
	FShinobiEnemyDiedDelegate OnDied;

	/** Set before BeginPlay (SpawnActorDeferred): Max/Health = BaseHealth x HealthScale. */
	float HealthScale = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float BaseHealth = 200.f;

	/** Per-hit damage of GA_EnemyMelee; for ranged enemies, the kunai's. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float AttackDamage = 25.f;

	/** The AI attacks once the target is this close (2D). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float AttackRange = 170.f;

	/** Ranged enemies back off when the target is much closer than this; 0 = melee, always close in. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float PreferredRange = 0.f;

	/** Seconds between attack starts (the Recover beat after each attack). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float AttackInterval = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	TSubclassOf<UShinobiGameplayAbility> AttackAbility;

	/** Tried whenever the target is out of attack range but within SpecialRange (the boss's fireball; its own cooldown gates it). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	TSubclassOf<UShinobiGameplayAbility> SpecialAbility;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float SpecialRange = 1500.f;

	/** The mannequin's "Paint Tint" (panels), so each enemy type reads apart from the players at a glance. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	FLinearColor BodyTint = FLinearColor(0.6f, 0.05f, 0.04f);

	/** True while any ability runs (the AI holds still). */
	bool IsBusy() const;

protected:
	virtual void BeginPlay() override;
	virtual void HandleOutOfHealth() override;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiOverheadTextComponent> HealthText;

	/** The runtime navmesh is only built around invokers (big open world); enemies carry one. */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<class UNavigationInvokerComponent> NavInvoker;
};

/** Melee grunt: 200 HP, 2-hit combo of 25. */
UCLASS()
class SHINOBICOOP_API AShinobiRogueNinja : public AShinobiEnemyCharacter
{
	GENERATED_BODY()

public:
	AShinobiRogueNinja(const FObjectInitializer& ObjectInitializer);
};

/** Ranged: 120 HP, kunai for 30 from up to 1500, keeps ~800 away. */
UCLASS()
class SHINOBICOOP_API AShinobiKunaiThrower : public AShinobiEnemyCharacter
{
	GENERATED_BODY()

public:
	AShinobiKunaiThrower(const FObjectInitializer& ObjectInitializer);
};

/** Boss: 2000 HP, 2-hit combo of 50, a 90-damage fireball every 8 s, shrugs off stuns (x0.33). */
UCLASS()
class SHINOBICOOP_API AShinobiRogueJonin : public AShinobiEnemyCharacter
{
	GENERATED_BODY()

public:
	AShinobiRogueJonin(const FObjectInitializer& ObjectInitializer);
};
