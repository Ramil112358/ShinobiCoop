#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "ShinobiSummon.generated.h"

class USkeletalMeshComponent;

UENUM()
enum class EShinobiSummonKind : uint8
{
	Toad,  // a giant toad: slow, tough, slams the ground
	Misha, // a giant plush puppy: hops, barks a shockwave from afar
	Pabu,  // a corgi ninken: one, quick, bites
};

/**
 * A creature called by a summoning jutsu. It is an enemy-AI character on the players' team: it chases the nearest hostile
 * (AShinobiAIController), fights with the shared attack abilities, and is a target for enemies like any player. It leaves in
 * a puff of smoke when its time is up. The caster has at most one of each kind; calling it again
 * replaces it. Not counted by waves or camps (only FShinobiEncounter::Spawn registers kills).
 */
UCLASS()
class SHINOBICOOP_API AShinobiSummon : public AShinobiEnemyCharacter
{
	GENERATED_BODY()

public:
	AShinobiSummon(const FObjectInitializer& ObjectInitializer);

	/**
	 * Server only. Calls Kind's summon(s) to the ground point Ground, facing where Caster faces, and dismisses the ones of
	 * that kind Caster had out. Returns the new summons (empty on a client or with no caster).
	 */
	static TArray<AShinobiSummon*> Summon(AShinobiCharacterBase* Caster, EShinobiSummonKind Kind, const FVector& Ground);

	/** Seconds a summon of this kind stays. */
	static float GetLifetime(EShinobiSummonKind Kind);

	EShinobiSummonKind GetKind() const { return Kind; }
	AShinobiCharacterBase* GetSummoner() const { return Summoner.Get(); }

	/** The creature model when it is imported, else the placeholder mannequin. */
	virtual USkeletalMeshComponent* GetBodyMesh() const override { return Creature ? Creature.Get() : Super::GetBodyMesh(); }

	virtual void Tick(float DeltaSeconds) override;

	/** Only enemies within LeashRange of the summoner: a summon guards its caster, it does not cross the level after a stray enemy. */
	virtual bool CanTarget(const AShinobiCharacterBase& Candidate) const override;

	static constexpr float LeashRange = 3000.f;

	/** Nothing to fight: it walks to its post beside the caster, which is about this far from them (a pack fans out round it). */
	static constexpr float FollowDistance = 500.f;

	/** Left this far behind (a teleport, a dragon ride; farther than it will chase, LeashRange plus its reach), it reappears beside the caster instead of walking. */
	static constexpr float CatchUpDistance = 4000.f;

	/** Idle (nothing to fight) and this far off, it reappears too: a street of buildings can block the straight walk back. */
	static constexpr float IdleCatchUpDistance = 1800.f;

	/** Idle and this far from its post, it hurries (a summon is slower than a running player): its speed times CatchUpSpeedScale. */
	static constexpr float HurryDistance = 600.f;
	static constexpr float CatchUpSpeedScale = 2.f;

	virtual bool GetIdlePost(FVector& OutPost) const override;

	/** Server: leaves in a puff of smoke now. Does nothing for one that has already fallen. */
	void Dismiss();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Applies Kind's numbers, size and look. Runs on every machine, before the enemy base reads them. */
	void Configure();

	/** Dresses the summon in its creature (Pabu, Misha, the Rodin toad) over the hidden
	 * mannequin. Does nothing on a dedicated server or when the creature is not imported. */
	void WearCreature();

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Creature;

	/** Server: with the caster far off, appears at its post beside them in a puff of smoke. */
	void CatchUpWithCaster();

	/** Server: the summon's post in the world: its place beside the caster, on the ground under it when there is ground. */
	FVector GetPostBesideCaster() const;

	/** Server: runs at CatchUpSpeedScale while idle and far from the post, at its own speed otherwise. */
	void HurryToPost();

	/** Direction (radians, world) from the caster to this summon's post, and how far out it stands; a pack fans out round them. */
	float FollowAngle = 0.f;
	float FollowRadius = 400.f;

	UPROPERTY(Replicated)
	EShinobiSummonKind Kind = EShinobiSummonKind::Toad;

	TWeakObjectPtr<AShinobiCharacterBase> Summoner;
	FTimerHandle LifetimeTimer;
};
