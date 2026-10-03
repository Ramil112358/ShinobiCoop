#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "ShinobiSummon.generated.h"

class UAnimSequence;

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

	/** The wolf for a dog, else the placeholder mannequin. */
	virtual USkeletalMeshComponent* GetBodyMesh() const override { return Creature ? Creature.Get() : Super::GetBodyMesh(); }

	virtual void Tick(float DeltaSeconds) override;

	/** Only enemies within LeashRange of the summoner: a summon guards its caster, it does not cross the level after a stray enemy. */
	virtual bool CanTarget(const AShinobiCharacterBase& Candidate) const override;

	static constexpr float LeashRange = 3000.f;

	/** Server: leaves in a puff of smoke now. Does nothing for one that has already fallen. */
	void Dismiss();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Applies Kind's numbers, size and look. Runs on every machine, before the enemy base reads them. */
	void Configure();

	/** Dogs and snakes: dresses the summon in its creature (the Animal Variety Pack wolf, the Mesh2Motion snake) over the hidden
	 * mannequin. Does nothing on a dedicated server or when the creature is not imported. */
	void WearCreature();

	/** Picks the creature's clip from what the summon is doing (idle, walking, running, biting, dying). */
	void UpdateCreatureClip();

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Creature;

	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> RunClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> BiteClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> DeathClip;

	UPROPERTY()
	TObjectPtr<UAnimSequence> PlayingClip;
	bool bWasBiting = false;
	bool bDeathPlayed = false;

	UPROPERTY(Replicated)
	EShinobiSummonKind Kind = EShinobiSummonKind::Toad;

	TWeakObjectPtr<AShinobiCharacterBase> Summoner;
	FTimerHandle LifetimeTimer;
};
