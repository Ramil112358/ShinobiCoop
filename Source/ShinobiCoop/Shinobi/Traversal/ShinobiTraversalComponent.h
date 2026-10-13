#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShinobiTraversalComponent.generated.h"

class UAnimMontage;
class UChooserTable;

/** One vault, hurdle or mantle: what the owning machine found and every machine plays. */
USTRUCT()
struct FShinobiTraversalAction
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY()
	float StartTime = 0.f;

	UPROPERTY()
	float PlayRate = 1.f;

	UPROPERTY()
	FVector FrontLedgeLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector FrontLedgeNormal = FVector::ZeroVector;

	UPROPERTY()
	FVector BackLedgeLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector BackLedgeNormal = FVector::ZeroVector;

	UPROPERTY()
	FVector BackFloorLocation = FVector::ZeroVector;

	/** Hurdles and vaults warp over the far edge; only hurdles land on the far floor (vaults end falling). */
	UPROPERTY()
	bool bWarpBackLedge = false;

	UPROPERTY()
	bool bWarpBackFloor = false;

	/** The obstacle: the capsule ignores it while the animation carries the character over or onto it. */
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Obstacle = nullptr;
};

/**
 * Vault, hurdle and mantle from the Game Animation Sample, on any geometry. The sample's own logic (AC_TraversalLogic)
 * only works on its level blocks, which carry hand-placed ledge splines; this finds the ledges with traces instead, then
 * does what the sample does: picks a montage with its chooser table (CHT_TraversalMontages_CMC, which also pose-matches
 * against the AnimBP's pose history), points the montage's motion-warping windows at the ledges, and plays it with the
 * capsule flying through the obstacle. The owning machine decides; the server and the other clients replay its choice,
 * and the server accepts the owner's position for the duration, as the sample does.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiTraversalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShinobiTraversalComponent();

	/** Owning machine (jump input): starts a traversal over or onto what is ahead. False when there is none (jump instead). */
	bool TryTraversal();

	bool IsTraversing() const { return bTraversing; }

	/** Highest ledge the traces look for, above the feet (the sample's tallest mantle). */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float MaxLedgeHeight = 275.f;

	/** Deepest obstacle that still counts as one to go over; deeper ones are mantled onto. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float MaxObstacleDepth = 150.f;

protected:
	virtual void BeginPlay() override;

private:
	bool FindLedges(FShinobiTraversalAction& Action, float& OutObstacleHeight, float& OutObstacleDepth, float& OutBackLedgeHeight,
		bool& bOutHasBackLedge, bool& bOutHasBackFloor) const;
	bool ChooseMontage(FShinobiTraversalAction& Action, float ObstacleHeight, float ObstacleDepth, float BackLedgeHeight,
		bool bHasBackLedge, bool bHasBackFloor) const;
	void Perform(const FShinobiTraversalAction& Action);
	void SetWarpTargets(const FShinobiTraversalAction& Action) const;
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void SetClientAuthoritative(bool bClientAuthoritative) const;

	UFUNCTION(Server, Reliable)
	void ServerPerform(const FShinobiTraversalAction& Action);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPerform(const FShinobiTraversalAction& Action);

	UPROPERTY()
	TObjectPtr<UChooserTable> Chooser;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> IgnoredObstacle;

	TWeakObjectPtr<UAnimMontage> PlayingMontage;
	FTimerHandle ReplicationTimer;
	bool bTraversing = false;
};
