#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShinobiMovementComponent.generated.h"

/** Adds a sprint flag that travels in saved moves, so the server agrees with the client (no rubber-banding), and wall/water
 * traversal for players: custom gravity into a wall while running on it, decided inside each move on both machines. */
UCLASS()
class SHINOBICOOP_API UShinobiMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UShinobiMovementComponent();

	void SetSprinting(bool bNewSprinting) { bWantsToSprint = bNewSprinting; }
	bool IsSprinting() const { return bWantsToSprint; }

	/** Guarding (UGA_Block sets it on the owning client and the server): roots the player. A saved-move flag like sprint, so the
	 * owning client roots and frees itself at once instead of a round trip later, when State.Blocking replicates. */
	void SetBlocking(bool bNewBlocking) { bWantsToBlock = bNewBlocking; }
	bool IsBlocking() const { return bWantsToBlock; }

	virtual float GetMaxSpeed() const override;

	/** Rides State.Mounted: flies while it is on and falls once it ends. Runs in every move, on the client and the server alike,
	 * so the tag (which replicates) is all the two need to agree on. */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	static constexpr float FlightSpeed = 1500.f;

	/** The lowest a mounted rider's capsule floor may be above the ground: the dragon's legs hang below the rider, so it can lift off
	 * from the ground and never lowers into it. */
	static constexpr float MountedGroundClearance = 310.f;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

	/** Server: position corrections also carry rotation for the next Seconds. Corrections are location-only by
	 * default, so a server-side teleport that turns the character (Substitution) would otherwise never turn the
	 * owning client. */
	void ForceRotationCorrectionFor(float Seconds);
	virtual bool ShouldCorrectRotation() const override;

	/** Corrections carry the server's gravity, but the engine never applies it on the owning client (UE 5.8): take it here,
	 * so a wall detach the server made (a hit, a teleport) also takes this client off the wall before its moves replay. */
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;
	virtual void SetGravityDirection(const FVector& GravityDir) override;

	// ---- Wall and water traversal (players only; docs/superpowers/specs/2026-10-05-surface-traversal-design.md) ----

	/** On a wall: gravity points into it and the character walks on it. The engine replicates gravity to every machine. */
	bool IsWallTraversing() const { return HasCustomGravity(); }

	/** Walking on an actor tagged WaterSurfaceTag. Traces under the feet, so it also works for other players' characters. */
	bool IsWaterTraversing() const;
	bool IsSurfaceTraversing() const { return IsWallTraversing() || IsWaterTraversing(); }

	/** A player shinobi that is not mounted, downed, dead or phased. */
	bool CanSurfaceTraverse() const;

	/** Leaves the wall: world gravity, upright yaw-only rotation facing Facing (default: the wall) at the nearest spot the
	 * capsule fits, falling. No-op when not on a wall. */
	void DetachFromWall(const FVector& Facing = FVector::ZeroVector);

	/** Maps a horizontal camera-relative world intent onto the wall: into the wall = up, out of it = down, along it = sideways. */
	FVector WallInputDirection(const FVector& Intent) const;

	/** Turns Actor toward WorldDir about its gravity up axis: yaw only when upright, along the wall when on one. A WorldDir
	 * along the up axis leaves the facing as it is. */
	static void FaceDirection(AActor* Actor, const FVector& WorldDir);

	/** On a wall, Jump kicks off it instead of jumping along its normal. */
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

	/** Walking off a wall's edge: onto the roof at the top, round an outer corner at the sides, otherwise fall. */
	virtual bool CheckFall(const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float remainingTime, float timeTick, int32 Iterations, bool bMustJump) override;

	static const FName WaterSurfaceTag;  // walkable water actors (map builders: WATER_SURFACE_TAG)
	static const FName NoTraversalTag;   // never climbed, e.g. map-boundary walls (map builders: NO_TRAVERSAL_TAG)
	static constexpr float WallMaxNormalZ = 0.4226f; // sin 25 deg: steeper faces are walls
	static constexpr float WallReach = 30.f;         // how far ahead of the capsule a face is noticed
	static constexpr float KickOffSpeed = 700.f;
	static constexpr float KickOffUp = 450.f;
	static constexpr float LedgeHopIn = 300.f;  // running off a wall's top with no roof to land on: over it, ~60 cm up
	static constexpr float LedgeHopUp = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SprintSpeed = 900.f;

	uint8 bWantsToSprint : 1;
	uint8 bWantsToBlock : 1;

private:
	void TryEnterWall();
	void UpdateWallTraversal();
	bool SweepCapsule(const FVector& From, const FVector& To, const FQuat& Rotation, FHitResult& OutHit) const;
	bool Fits(const FVector& Location, const FQuat& Rotation) const;
	bool IsClimbable(const FHitResult& Hit) const;
	/** Turns onto the wall Hit found, at Anchor projected onto it. False (and nothing changes) when the turned capsule would not fit. */
	bool AttachToWall(const FHitResult& Hit, const FVector& Anchor);
	bool TryVault();
	bool TryWrapCorner(const FVector& Move);

	double ForceRotationCorrectionUntil = 0.0;
	/** When gravity last changed (world seconds): corrections carry rotation for a while after, so the owning client turns
	 * with a server-side attach or detach. */
	double GravityChangedAt = -1.0;
	/** Other machines' copies of a character follow the replicated gravity only; they never decide wall state. */
	bool IsSimulatedProxy() const;
	bool bWasMounted = false;
};
