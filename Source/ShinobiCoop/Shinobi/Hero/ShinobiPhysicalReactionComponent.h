#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShinobiPhysicalReactionComponent.generated.h"

class UChooserTable;
class UPhysicsControlComponent;

/**
 * Physical hit reactions from the Game Animation Sample (UE 5.8), on the hidden Manny that every humanoid animates (the
 * hero mesh copies its pose, physics included, through the retarget).
 *
 * The sample's physics control asset (PCA_SandboxCharacter) gives three profiles: "PhysicalAnimation" (the default here,
 * as in the sample: the body simulates while controls pull it onto the animation, so impulses read as hits),
 * "Kinematic" (pure animation: used while wall running, where world gravity would pull the limbs off the wall) and
 * "Ragdoll". On top of them:
 * - Hit: an impulse on the chest, scaled by the damage, from the attacker's side.
 * - Knockdown (any launch from LaunchVelocity >= KnockdownSpeed): a powered ragdoll tumble, then a getup montage picked by
 *   the sample's CHT_GetUpMontages (pose-matched against the ragdoll's pose). State.Staggered holds the character for
 *   KnockdownHoldSeconds on every machine.
 * - Death: a ragdoll that keeps some strength at first, so the body sags instead of dropping like a sack.
 *
 * Cosmetic: each machine simulates its own copy, the server decides when (unreliable hits, reliable knockdowns). Nothing
 * simulates on a dedicated server. Getups ignore root motion, so the owner's capsule never disagrees with the server.
 */
UCLASS(ClassGroup = (Shinobi))
class SHINOBICOOP_API UShinobiPhysicalReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShinobiPhysicalReactionComponent();

	/** Server: hit from Direction (attacker to victim) for Damage. */
	void Hit(const FVector& Direction, float Damage);

	/** Server: thrown at Velocity. */
	void Knockdown(const FVector& Velocity);

	/** Every machine (State.Dead replicates). False when there's no physics control here, so the caller ragdolls itself. */
	bool Die();

	/** Whether the body is physically controlled on this machine (never on a dedicated server). */
	bool IsPhysical() const { return Physics != nullptr; }

	bool IsKnockedDown() const { return bKnockedDown; }

	/** Launches at least this fast knock the character down (the melee finisher is ~630, Rasengan ~1500). */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float KnockdownSpeed = 600.f;

	/** How long a knockdown holds the character (State.Staggered): tumble plus getup. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float KnockdownHoldSeconds = 2.2f;

	/** Velocity change on the chest per point of damage, and its cap (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float HitImpulsePerDamage = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float MaxHitImpulse = 2000.f;

	/** Control strength right after a hit, eased back to full over HitRecoverySeconds: how far the body gives. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float HitStrength = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float HitRecoverySeconds = 0.4f;

	/** Control strength while tumbling (1 = the profile's own): the body braces a little instead of going limp. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float TumbleStrength = 0.3f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHit(FVector_NetQuantizeNormal Direction, float Damage);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastKnockdown(FVector_NetQuantize Velocity);

	void SetProfile(FName Profile);
	void SetStrength(float Strength);
	void GetUp();
	void EndKnockdown();

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsControlComponent> Physics;

	UPROPERTY(Transient)
	TObjectPtr<UChooserTable> GetUpChooser;

	FName Profile;
	TArray<FName> PelvisModifiers;
	bool bKnockedDown = false;
	bool bGettingUp = false;
	bool bDead = false;
	double KnockdownTime = 0.0;
	double HitTime = -1.e9;
	double DeathTime = 0.0;
	FTimerHandle HoldTimer;
};
