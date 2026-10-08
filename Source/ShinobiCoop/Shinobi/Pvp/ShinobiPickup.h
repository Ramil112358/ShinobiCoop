#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiPickup.generated.h"

class AShinobiCharacterBase;
class UPrimitiveComponent;
class UShinobiOverheadTextComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FHitResult;

UENUM()
enum class EShinobiPickupType : uint8
{
	Health, // +40% of max HP
	Attack, // UGE_AttackBoost
	Speed,  // UGE_SpeedBoost
	Shield, // chakra to max and UGE_Shield
};

/**
 * A PvP arena power-up: a glowing orb with its name over it. The first living fighter to touch it (summons can't) takes it;
 * it hides and comes back 25 s later, or at the next round's reset (AShinobiPvpGameMode). Server-spawned and replicated:
 * the server decides who takes it, clients only see bTaken.
 */
UCLASS()
class SHINOBICOOP_API AShinobiPickup : public AActor
{
	GENERATED_BODY()

public:
	AShinobiPickup();

	static constexpr float RespawnSeconds = 25.f;
	static constexpr float HealFraction = 0.4f; // of max HP

	/** Server only: a pickup of Type at Location (on the ground; the orb floats above it). */
	static AShinobiPickup* Spawn(UWorld* World, EShinobiPickupType Type, const FVector& Location);

	/** Server only: what taking a Type pickup does to Target. False, and nothing done, for a target that is not alive. */
	static bool Apply(AShinobiCharacterBase& Target, EShinobiPickupType Type);

	/** The overlap handler (public for tests): Taker takes it if it is available and Taker is a living fighter. One winner:
	 * the first call takes it, later ones (the same frame included) find it taken. Server only. */
	bool TryTake(AActor* Taker);

	/** Hide (and start the respawn timer) or show it again. */
	void SetTaken(bool bNowTaken);

	/** Available again now (the round reset). */
	void ResetNow();

	bool IsTaken() const { return bTaken; }
	EShinobiPickupType GetType() const { return Type; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnRep_Taken();

	/** Hidden and untouchable while taken. */
	void ApplyTakenLook();

	UPROPERTY(Replicated)
	EShinobiPickupType Type = EShinobiPickupType::Health;

	UPROPERTY(ReplicatedUsing = OnRep_Taken)
	bool bTaken = false;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UStaticMeshComponent> Orb;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiOverheadTextComponent> Label;

	FTimerHandle RespawnTimer;
	float Age = 0.f;
};
