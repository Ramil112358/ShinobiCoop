#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "ShinobiSummon.generated.h"

UENUM()
enum class EShinobiSummonKind : uint8
{
	Toad,  // a giant toad: slow, tough, slams the ground
	Snake, // a giant snake: long reach, strikes from afar
	Dog,   // a ninken: summoned as a pack of DogPackSize, fast and fragile
};

/**
 * A creature called by a summoning jutsu. It is an enemy-AI character on the players' team: it chases the nearest hostile
 * (AShinobiAIController), fights with the shared attack abilities, and is a target for enemies like any player. It leaves in
 * a puff of smoke when its time is up. The caster has at most one of each kind (a dog pack counts as one); calling it again
 * replaces it. Not counted by waves or camps (only FShinobiEncounter::Spawn registers kills).
 */
UCLASS()
class SHINOBICOOP_API AShinobiSummon : public AShinobiEnemyCharacter
{
	GENERATED_BODY()

public:
	AShinobiSummon(const FObjectInitializer& ObjectInitializer);

	static constexpr int32 DogPackSize = 4;

	/**
	 * Server only. Calls Kind's summon(s) to the ground point Ground, facing where Caster faces, and dismisses the ones of
	 * that kind Caster had out. Returns the new summons (empty on a client or with no caster).
	 */
	static TArray<AShinobiSummon*> Summon(AShinobiCharacterBase* Caster, EShinobiSummonKind Kind, const FVector& Ground);

	/** Seconds a summon of this kind stays. */
	static float GetLifetime(EShinobiSummonKind Kind);

	EShinobiSummonKind GetKind() const { return Kind; }
	AShinobiCharacterBase* GetSummoner() const { return Summoner.Get(); }

	/** Server: leaves in a puff of smoke now. Does nothing for one that has already fallen. */
	void Dismiss();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Applies Kind's numbers, size and look. Runs on every machine, before the enemy base reads them. */
	void Configure();

	UPROPERTY(Replicated)
	EShinobiSummonKind Kind = EShinobiSummonKind::Toad;

	TWeakObjectPtr<AShinobiCharacterBase> Summoner;
	FTimerHandle LifetimeTimer;
};
