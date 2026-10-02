#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiDamageable.h"
#include "ShinobiBarrier.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UENUM()
enum class EShinobiBarrierKind : uint8
{
	Earth, // Earth Wall: 400 HP, 10 s
	Water, // Water Wall: 250 HP, 6 s
};

/**
 * A jutsu wall: 450 wide, 60 thick, 300 tall, standing on the ground at its origin. Blocks pawns and anything but its
 * owner's team's projectiles (those fly through, enemy ones are stopped and hurt it). Server-spawned and replicated;
 * health and lifetime run on the server, and the rise and look are cosmetic on each machine.
 */
UCLASS()
class SHINOBICOOP_API AShinobiBarrier : public AActor, public IShinobiDamageable
{
	GENERATED_BODY()

public:
	AShinobiBarrier();

	/** Server. A wall of Kind at BaseTransform (ground level, centred), belonging to Caster's team; breaks Caster's
	 * existing wall first (one per caster). */
	static AShinobiBarrier* Spawn(UWorld* World, const FTransform& BaseTransform, AShinobiCharacterBase* Caster, EShinobiBarrierKind Kind);

	/** Server: a wall of Kind 350 units ahead of Caster, at their feet, facing their way. */
	static AShinobiBarrier* SpawnInFront(AShinobiCharacterBase* Caster, EShinobiBarrierKind Kind);

	float GetHealth() const { return Health; }
	EShinobiBarrierKind GetKind() const { return Kind; }

	virtual void ReceiveShinobiDamage(float Amount, const AActor* Source) override;
	virtual EShinobiTeam GetDamageableTeam() const override { return Team; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Replicated)
	EShinobiBarrierKind Kind = EShinobiBarrierKind::Earth;

	UPROPERTY(Replicated)
	EShinobiTeam Team = EShinobiTeam::Players;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Server only. */
	float Health = 0.f;

	/** Seconds since this machine saw it: drives the rise from the ground. */
	float Age = 0.f;
};
