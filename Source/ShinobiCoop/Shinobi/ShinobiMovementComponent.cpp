#include "Shinobi/ShinobiMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	class FShinobiSavedMove : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		uint8 bSavedWantsToSprint : 1;
		uint8 bSavedWantsToBlock : 1;
		/** Gravity when the move started: moves across a wall attach or detach must not be combined, or the replay would run
		 * the combined move entirely under one gravity. */
		FVector SavedGravityDirection = UCharacterMovementComponent::DefaultGravityDirection;

		virtual void Clear() override
		{
			Super::Clear();
			bSavedWantsToSprint = false;
			bSavedWantsToBlock = false;
			SavedGravityDirection = UCharacterMovementComponent::DefaultGravityDirection;
		}

		virtual uint8 GetCompressedFlags() const override
		{
			uint8 Result = Super::GetCompressedFlags();
			if (bSavedWantsToSprint)
			{
				Result |= FLAG_Custom_0;
			}
			if (bSavedWantsToBlock)
			{
				Result |= FLAG_Custom_1;
			}
			return Result;
		}

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
		{
			const FShinobiSavedMove* Other = static_cast<FShinobiSavedMove*>(NewMove.Get());
			if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedWantsToBlock != Other->bSavedWantsToBlock || !SavedGravityDirection.Equals(Other->SavedGravityDirection))
			{
				return false;
			}
			return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
		}

		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
		{
			Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
			if (const UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(C->GetCharacterMovement()))
			{
				bSavedWantsToSprint = Movement->bWantsToSprint;
				bSavedWantsToBlock = Movement->bWantsToBlock;
				SavedGravityDirection = Movement->GetGravityDirection();
			}
		}

		virtual void PrepMoveFor(ACharacter* C) override
		{
			Super::PrepMoveFor(C);
			if (UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(C->GetCharacterMovement()))
			{
				Movement->bWantsToSprint = bSavedWantsToSprint;
				Movement->bWantsToBlock = bSavedWantsToBlock;
			}
		}
	};

	class FShinobiPredictionData : public FNetworkPredictionData_Client_Character
	{
	public:
		explicit FShinobiPredictionData(const UCharacterMovementComponent& ClientMovement)
			: FNetworkPredictionData_Client_Character(ClientMovement)
		{
		}

		virtual FSavedMovePtr AllocateNewMove() override
		{
			return FSavedMovePtr(new FShinobiSavedMove());
		}
	};
}

UShinobiMovementComponent::UShinobiMovementComponent()
{
	bWantsToSprint = false;
	bWantsToBlock = false;
	MaxWalkSpeed = 600.f;
	BrakingDecelerationFlying = 2500.f; // a dragon stops and turns quickly
	// Uncontrolled characters (dummies, knocked-back pawns, test actors) still need physics and root motion.
	bRunPhysicsWithNoController = true;
}

void UShinobiMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (IsSimulatedProxy())
	{
		// Another machine's copy: it follows the replicated gravity (and its extrapolation can briefly lose the floor).
	}
	else if (!CanSurfaceTraverse())
	{
		DetachFromWall(); // mounted, downed, dead, phased, or not a player: never on a wall
	}
	else if (IsWallTraversing())
	{
		UpdateWallTraversal();
	}
	else
	{
		TryEnterWall();
	}
	const AShinobiCharacterBase* Character = Cast<AShinobiCharacterBase>(GetOwner());
	const bool bMounted = Character && Character->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Mounted);
	if (bMounted && MovementMode != MOVE_Flying)
	{
		SetMovementMode(MOVE_Flying);
	}
	else if (!bMounted && bWasMounted && MovementMode == MOVE_Flying)
	{
		SetMovementMode(MOVE_Falling);
	}
	bWasMounted = bMounted;
	bMountedGrounded = false;
	MountedGroundBelow = UE_BIG_NUMBER;
	if (bMounted && MovementMode == MOVE_Flying && CharacterOwner)
	{
		// Near the ground (or a roof) and not climbing, the dragon lands: PhysFlying settles the seat to its standing height. Climbing
		// close to it pushes up, harder the closer it is (a quick lift-off). The push is the speed that the flying brakes stop right
		// at the clearance: a springier push launched Tairn's lift-off far past it.
		MountedGroundBelow = GroundBelowSeat(MountedGroundClearance * 2.f);
		bMountedGrounded = MountedStandHeight > 0.f && Acceleration.Z <= UE_KINDA_SMALL_NUMBER && MountedGroundBelow < MountedGroundClearance;
		MountedSteer = bMountedGrounded ? FVector(Acceleration.X, Acceleration.Y, 0.f) : FVector::ZeroVector;
		if (!bMountedGrounded && MountedGroundBelow < MountedGroundClearance)
		{
			Velocity.Z = FMath::Max(Velocity.Z, FMath::Sqrt(2.f * BrakingDecelerationFlying * (MountedGroundClearance - MountedGroundBelow)));
		}
	}
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

float UShinobiMovementComponent::GroundBelowSeat(float Reach) const
{
	const FVector Seat = CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MountClearance), false, CharacterOwner);
	return GetWorld()->LineTraceSingleByChannel(Hit, Seat, Seat - FVector(0.f, 0.f, Reach), ECC_WorldStatic, Params) ? Hit.Distance : UE_BIG_NUMBER;
}

void UShinobiMovementComponent::PhysFlying(float DeltaTime, int32 Iterations)
{
	if (!bMountedGrounded)
	{
		Super::PhysFlying(DeltaTime, Iterations);
		return;
	}
	// On the ground: walk level (no climbing or diving) and only the way it faces (PhysicsRotation turns it toward the steer; steered
	// behind, it turns on the spot), then settle the seat to its standing height over whatever is below now.
	Acceleration.Z = 0.f;
	Velocity.Z = 0.f;
	if (MountedTurnRate > 0.f && UpdatedComponent)
	{
		const FVector Heading = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
		Acceleration = Heading * Acceleration.Size2D() * FMath::Max(0.f, Acceleration.GetSafeNormal2D() | Heading);
	}
	Super::PhysFlying(DeltaTime, Iterations);
	const float Below = GroundBelowSeat(MountedGroundClearance * 2.f);
	if (Below < UE_BIG_NUMBER && DeltaTime > 0.f && UpdatedComponent)
	{
		const float Rise = FMath::Clamp(MountedStandHeight - Below, -LandingSpeed * DeltaTime, StandingRiseSpeed * DeltaTime);
		FHitResult Hit;
		SafeMoveUpdatedComponent(FVector(0.f, 0.f, Rise), UpdatedComponent->GetComponentQuat(), true, Hit);
		Velocity.Z = Rise / DeltaTime; // honest for the dragon's clips on other machines (sinking: landing)
	}
}

void UShinobiMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (!bMountedGrounded || MountedTurnRate <= 0.f || !UpdatedComponent || MovementMode != MOVE_Flying)
	{
		Super::PhysicsRotation(DeltaTime); // flying: it faces the camera (bUseControllerDesiredRotation)
		return;
	}
	const FRotator Current = UpdatedComponent->GetComponentRotation();
	if (MountedSteer.IsNearlyZero())
	{
		return; // no steer: it keeps its heading
	}
	const FRotator Turned(0.f, FMath::FixedTurn(Current.Yaw, MountedSteer.Rotation().Yaw, MountedTurnRate * DeltaTime), 0.f);
	// Up against something (its snout at a wall), the turn would swing it in: it steps back (or aside) first, as far as the turn needs.
	const FVector Seat = UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FHitResult Hit;
	if (FindRoomHit(*GetWorld(), MountedRoom(), Seat, Seat, Turned.Quaternion(), MountedGroundBelow, CharacterOwner, Hit))
	{
		const FVector Ahead = Current.Vector().GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
		bool bRoom = false;
		for (const float Step : { 25.f, 50.f, 100.f })
		{
			for (const FVector& Away : { -Ahead, Side, -Side, Ahead })
			{
				if (!bRoom && !FindRoomHit(*GetWorld(), MountedRoom(), Seat + Away * Step, Seat + Away * Step, Turned.Quaternion(), MountedGroundBelow, CharacterOwner, Hit))
				{
					SafeMoveUpdatedComponent(Away * Step, Current, true, Hit);
					bRoom = true;
				}
			}
		}
	}
	MoveUpdatedComponent(FVector::ZeroVector, Turned, false); // its room boxes still hold the turn if there was no room to be made
}

bool UShinobiMovementComponent::FindRoomHit(const UWorld& World, TConstArrayView<FBox> Room, const FVector& From, const FVector& To, const FQuat& Rotation, float GroundBelow, const AActor* Ignore, FHitResult& OutHit)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonRoom), false, Ignore);
	if (Ignore)
	{
		TArray<AActor*> Attached;
		Ignore->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
	}
	// What a character bumps into (buildings, props, the ground), but not characters or loose debris.
	FCollisionResponseParams Responses;
	Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Responses.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);
	Responses.CollisionResponse.SetResponse(ECC_Vehicle, ECR_Ignore);
	Responses.CollisionResponse.SetResponse(ECC_Destructible, ECR_Ignore);
	OutHit = FHitResult();
	for (const FBox& Part : Room)
	{
		FVector Min = Part.Min;
		Min.Z = FMath::Max(Min.Z, MountedStepClearance - GroundBelow);
		if (!Part.IsValid || Min.Z >= Part.Max.Z)
		{
			continue;
		}
		const FVector Centre = Rotation.RotateVector((Min + Part.Max) * 0.5f);
		const FCollisionShape Box = FCollisionShape::MakeBox((Part.Max - Min) * 0.5f);
		if (From.Equals(To))
		{
			if (World.OverlapBlockingTestByChannel(From + Centre, Rotation, ECC_Pawn, Box, Params, Responses))
			{
				OutHit.bBlockingHit = OutHit.bStartPenetrating = true;
				return true;
			}
			continue;
		}
		FHitResult Hit;
		if (World.SweepSingleByChannel(Hit, From + Centre, To + Centre, Rotation, ECC_Pawn, Box, Params, Responses)
			&& (Hit.bStartPenetrating || !OutHit.bBlockingHit || Hit.Time < OutHit.Time))
		{
			OutHit = Hit; // the first box to meet something (a box already in something: that one, at once)
			if (Hit.bStartPenetrating)
			{
				return true;
			}
		}
	}
	return OutHit.bBlockingHit;
}

bool UShinobiMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	const TArray<FBox>& Room = MountedRoom();
	if (!bWasMounted || Room.IsEmpty() || MovementMode != MOVE_Flying || !UpdatedComponent || !CharacterOwner || !GetWorld())
	{
		return Super::MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, OutHit, Teleport);
	}
	// The rider's capsule is small; the dragon under it is not. Its boxes go first: a turn that would swing it into something is held,
	// and a move stops (and slides) where the box meets something, as the capsule's own would.
	const FVector Seat = UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FQuat Current = UpdatedComponent->GetComponentQuat();
	FQuat Rotation = NewRotation;
	FHitResult Probe;
	if (!Rotation.Equals(Current, 1e-5f) && FindRoomHit(*GetWorld(), Room, Seat, Seat, Rotation, MountedGroundBelow, CharacterOwner, Probe)
		&& !FindRoomHit(*GetWorld(), Room, Seat, Seat, Current, MountedGroundBelow, CharacterOwner, Probe))
	{
		Rotation = Current;
	}
	FVector Move = Delta;
	float Fraction = 1.f;
	FHitResult RoomHit;
	if (bSweep && !Move.IsNearlyZero() && FindRoomHit(*GetWorld(), Room, Seat, Seat + Move, Rotation, MountedGroundBelow, CharacterOwner, RoomHit))
	{
		if (RoomHit.bStartPenetrating)
		{
			// Already in something (it moved into the dragon, or the bigger flying box on take-off): any move that doesn't go deeper.
			if ((Move | RoomHit.Normal) < 0.f)
			{
				Move = FVector::VectorPlaneProject(Move, RoomHit.Normal);
			}
			RoomHit = FHitResult();
		}
		else
		{
			Fraction = FMath::Max(0.f, RoomHit.Time - 1.f / Move.Size()); // a centimetre short
			Move *= Fraction;
		}
	}
	const bool bMoved = Super::MoveUpdatedComponentImpl(Move, Rotation, bSweep, OutHit, Teleport);
	if (OutHit && RoomHit.bBlockingHit)
	{
		if (OutHit->bBlockingHit)
		{
			OutHit->Time *= Fraction; // the capsule met something sooner: its time, of the whole move
		}
		else
		{
			*OutHit = RoomHit;
			OutHit->Time = Fraction;
		}
	}
	return bMoved;
}

float UShinobiMovementComponent::GetMaxSpeed() const
{
	float Speed = (bWantsToSprint && MovementMode == MOVE_Walking) ? SprintSpeed : Super::GetMaxSpeed();
	const AShinobiCharacterBase* Character = Cast<AShinobiCharacterBase>(GetOwner());
	if (bWantsToBlock)
	{
		return 0.f; // guarding roots the player (the guard pose is full-body); they still turn to face their input
	}
	if (MovementMode == MOVE_Flying && Character && Character->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Mounted))
	{
		// Shift (UGA_Sprint, stamina and all) runs on the ground and flies faster, as it sprints on foot.
		Speed = !bMountedGrounded || MountedWalkSpeed <= 0.f ? (bWantsToSprint ? FlightSprintSpeed : FlightSpeed)
			: bWantsToSprint && MountedRunSpeed > 0.f ? MountedRunSpeed : MountedWalkSpeed;
	}
	if (Character && Character->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Chilled))
	{
		Speed *= UGE_Chill::SpeedScale;
	}
	if (Character && Character->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_SpeedBoost))
	{
		Speed *= UGE_SpeedBoost::SpeedScale;
	}
	return Speed;
}

void UShinobiMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	// For a remote client's pawn, the server derives bWantsToSprint from that client's own moves here,
	// so GA_Sprint's server-side SetSprinting(false) isn't authoritative against a client that keeps
	// sending the flag; acceptable for friends-only co-op, not for a competitive/anti-cheat setting.
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToBlock = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0; // the same caveat as sprint
}

FNetworkPredictionData_Client* UShinobiMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UShinobiMovementComponent* MutableThis = const_cast<UShinobiMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FShinobiPredictionData(*this);
	}
	return ClientPredictionData;
}

bool UShinobiMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	// Super replays unacked moves via PrepMoveFor/UpdateFromCompressedFlags, which overwrites
	// bWantsToSprint (and bWantsToBlock) with each replayed move's flag in turn. The engine does the same
	// save/restore for bWantsToCrouch around this call (CharacterMovementComponent.cpp), so we
	// mirror that here: the last replayed move would otherwise win instead of the current input.
	const bool bRealWantsToSprint = bWantsToSprint;
	const bool bRealWantsToBlock = bWantsToBlock;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealWantsToSprint;
	bWantsToBlock = bRealWantsToBlock;
	return bResult;
}

void UShinobiMovementComponent::ForceRotationCorrectionFor(float Seconds)
{
	ForceRotationCorrectionUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

bool UShinobiMovementComponent::ShouldCorrectRotation() const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// On a wall, and just after leaving one, the facing is part of the wall state: corrections carry it too.
	return Now < ForceRotationCorrectionUntil || HasCustomGravity() || (GravityChangedAt >= 0.0 && Now < GravityChangedAt + 0.5);
}

void UShinobiMovementComponent::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	if (!MoveResponse.IsGoodMove())
	{
		SetGravityDirection(MoveResponse.ClientAdjustment.GravityDirection);
	}
	Super::ClientHandleMoveResponse(MoveResponse);
}

void UShinobiMovementComponent::SetGravityDirection(const FVector& GravityDir)
{
	if (!GravityDir.GetSafeNormal().Equals(GetGravityDirection()) && GetWorld())
	{
		GravityChangedAt = GetWorld()->GetTimeSeconds();
	}
	Super::SetGravityDirection(GravityDir);
}

// ---- Wall and water traversal ----

const FName UShinobiMovementComponent::WaterSurfaceTag(TEXT("ShinobiWaterSurface"));
const FName UShinobiMovementComponent::NoTraversalTag(TEXT("ShinobiNoSurfaceTraversal"));

bool UShinobiMovementComponent::IsSimulatedProxy() const
{
	return CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy;
}

bool UShinobiMovementComponent::CanSurfaceTraverse() const
{
	const AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(GetOwner());
	if (!Player || !Player->GetAbilitySystemComponent())
	{
		return false;
	}
	static const FGameplayTagContainer Blockers = []
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(ShinobiTags::State_Mounted);
		Tags.AddTag(ShinobiTags::State_Downed);
		Tags.AddTag(ShinobiTags::State_Dead);
		Tags.AddTag(ShinobiTags::State_Phased);
		return Tags;
	}();
	return !Player->GetAbilitySystemComponent()->HasAnyMatchingGameplayTags(Blockers);
}

bool UShinobiMovementComponent::IsWaterTraversing() const
{
	if (!CharacterOwner || IsWallTraversing() || !IsMovingOnGround())
	{
		return false;
	}
	const FVector Feet = GetActorFeetLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiWaterCheck), false, CharacterOwner);
	return GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, 5.f), Feet - FVector(0.f, 0.f, 20.f), ECC_Pawn, Params)
		&& Hit.GetActor() && Hit.GetActor()->ActorHasTag(WaterSurfaceTag);
}

bool UShinobiMovementComponent::SweepCapsule(const FVector& From, const FVector& To, const FQuat& Rotation, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiTraversalSweep), false, CharacterOwner);
	FCollisionResponseParams Response;
	InitCollisionParams(Params, Response);
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore); // other characters are where each machine last saw them
	return GetWorld()->SweepSingleByChannel(OutHit, From, To, Rotation, UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), Params, Response);
}

bool UShinobiMovementComponent::Fits(const FVector& Location, const FQuat& Rotation) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiTraversalFit), false, CharacterOwner);
	FCollisionResponseParams Response;
	InitCollisionParams(Params, Response);
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore); // other characters move out of the way
	return !GetWorld()->OverlapBlockingTestByChannel(Location, Rotation, UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), Params, Response);
}

bool UShinobiMovementComponent::IsClimbable(const FHitResult& Hit) const
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	return Hit.IsValidBlockingHit() && Component && Component->GetCollisionObjectType() == ECC_WorldStatic
		&& FMath::Abs(Hit.ImpactNormal.Z) <= WallMaxNormalZ
		&& !(Hit.GetActor() && Hit.GetActor()->ActorHasTag(NoTraversalTag));
}

void UShinobiMovementComponent::TryEnterWall()
{
	const FVector Push = Acceleration.GetSafeNormal2D();
	if (Push.IsZero() || !(IsMovingOnGround() || IsFalling()))
	{
		return; // standing still beside a wall never attaches
	}
	const FVector Location = UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	if (!SweepCapsule(Location, Location + Push * WallReach, UpdatedComponent->GetComponentQuat(), Hit) || !IsClimbable(Hit) || (Push | -Hit.ImpactNormal) < 0.5f)
	{
		return;
	}
	// Kerbs, benches and car sides have vertical faces too: the face must also reach head height.
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const FVector Head = Location + FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() - 10.f);
	FHitResult HeadHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiWallHead), false, CharacterOwner);
	if (GetWorld()->LineTraceSingleByChannel(HeadHit, Head, Head - Hit.ImpactNormal * (Capsule->GetScaledCapsuleRadius() + WallReach + 10.f), UpdatedComponent->GetCollisionObjectType(), Params)
		&& IsClimbable(HeadHit))
	{
		AttachToWall(Hit, Location);
	}
}

bool UShinobiMovementComponent::AttachToWall(const FHitResult& Hit, const FVector& Anchor)
{
	const FVector Normal = Hit.ImpactNormal;
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Center = FVector::PointPlaneProject(Anchor, Hit.ImpactPoint, Normal) + Normal * (HalfHeight + MIN_FLOOR_DIST);
	// Turn the run with the surface (ground to wall, or round a corner) so it carries on along the new face.
	const FVector Turned = FVector::VectorPlaneProject(FQuat::FindBetweenNormals(-GetGravityDirection(), Normal).RotateVector(Velocity), Normal);
	FVector Facing = Turned;
	if (Facing.SizeSquared() < 1.f)
	{
		Facing = FVector::VectorPlaneProject(FVector::UpVector, Normal); // face up the wall
	}
	const FQuat Rotation = FRotationMatrix::MakeFromZX(Normal, Facing).ToQuat();
	if (!Fits(Center, Rotation))
	{
		return false; // no room to turn onto the wall: never rotate the capsule into geometry
	}
	SetGravityDirection(-Normal);
	UpdatedComponent->SetWorldLocationAndRotation(Center, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = Turned;
	if (MovementMode == MOVE_Walking)
	{
		FindFloor(Center, CurrentFloor, false); // already walking (a corner): SetMovementMode would not look for the new floor
	}
	else
	{
		SetMovementMode(MOVE_Walking);
	}
	return true;
}

void UShinobiMovementComponent::UpdateWallTraversal()
{
	if (!IsMovingOnGround())
	{
		DetachFromWall(); // left the wall some other way: fall the normal way
		return;
	}
	const FVector Move = Acceleration.GetSafeNormal();
	if (Move.IsZero())
	{
		return;
	}
	const FVector Location = UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	if (!SweepCapsule(Location, Location + Move * WallReach, UpdatedComponent->GetComponentQuat(), Hit) || !Hit.IsValidBlockingHit())
	{
		return;
	}
	if (Hit.ImpactNormal.Z >= GetWalkableFloorZ())
	{
		DetachFromWall(); // walked down onto the ground: stand up on it
	}
	else if (IsClimbable(Hit) && (Move | -Hit.ImpactNormal) >= 0.5f)
	{
		AttachToWall(Hit, Location); // inner corner: onto the next face
	}
}

void UShinobiMovementComponent::DetachFromWall(const FVector& Facing)
{
	if (!IsWallTraversing() || IsSimulatedProxy())
	{
		return;
	}
	const FVector Normal = -GetGravityDirection();
	SetGravityDirection(DefaultGravityDirection);
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Lift = Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius();
	const FVector Look = FVector::VectorPlaneProject(Facing.IsNearlyZero() ? -Normal : Facing, FVector::UpVector);
	const FQuat Upright = FRotationMatrix::MakeFromXZ(Look.IsNearlyZero() ? UpdatedComponent->GetForwardVector() : Look, FVector::UpVector).ToQuat();
	// Standing up swings the capsule's long axis from the wall's normal to vertical: take the first nearby spot it fits,
	// closest to the wall first. With none free, stay put and let the movement component push out of the overlap.
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Near = Location - Normal * (Lift - MIN_FLOOR_DIST);
	const FVector Candidates[] = { Near, Near + FVector::UpVector * Lift, Location, Location + FVector::UpVector * Lift };
	FVector Spot = Location;
	for (const FVector& Candidate : Candidates)
	{
		if (Fits(Candidate, Upright))
		{
			Spot = Candidate;
			break;
		}
	}
	UpdatedComponent->SetWorldLocationAndRotation(Spot, Upright, false, nullptr, ETeleportType::TeleportPhysics);
	SetMovementMode(MOVE_Falling);
}

FVector UShinobiMovementComponent::WallInputDirection(const FVector& Intent) const
{
	const FVector Normal = -GetGravityDirection();
	const FVector Into = -Normal.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Into).GetSafeNormal();
	const FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Normal).GetSafeNormal();
	return Up * (Intent | Into) + Side * (Intent | Side);
}

bool UShinobiMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	if (!IsWallTraversing())
	{
		return Super::DoJump(bReplayingMoves, DeltaTime);
	}
	const FVector Normal = -GetGravityDirection();
	DetachFromWall(Normal); // facing away from the wall
	Velocity = Normal * KickOffSpeed + FVector::UpVector * KickOffUp;
	return true; // counts as the first jump, so the double jump still works in the air
}

bool UShinobiMovementComponent::CheckFall(const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float remainingTime, float timeTick, int32 Iterations, bool bMustJump)
{
	if (!IsWallTraversing())
	{
		return Super::CheckFall(OldFloor, Hit, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump);
	}
	if (IsSimulatedProxy())
	{
		return true; // the owner's machine decides; the replicated state follows
	}
	const FVector Move = Delta.GetSafeNormal();
	const bool bUp = (Move | FVector::UpVector) > 0.5f;
	if (!(bUp && TryVault()) && !TryWrapCorner(Move))
	{
		const FVector Inward = GetGravityDirection().GetSafeNormal2D();
		DetachFromWall();
		if (bUp)
		{
			// Over a top with no roof to stand on (a ridge, a parapet): hop over it. Kept, the run up the wall flung the
			// character metres straight up, and the held input then caught the wall again on the way down.
			Velocity = Inward * LedgeHopIn + FVector::UpVector * FMath::Min(Velocity.Z, LedgeHopUp);
		}
	}
	return true; // handled: PhysWalking must not start a fall with the wall's gravity
}

bool UShinobiMovementComponent::TryVault()
{
	const FVector Inward = GetGravityDirection().GetSafeNormal2D(); // gravity points into the wall
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Over = Location + Inward * (HalfHeight + Capsule->GetScaledCapsuleRadius() + 20.f);
	FHitResult Roof;
	if (!SweepCapsule(Over + FVector::UpVector * HalfHeight * 2.f, Over - FVector::UpVector * HalfHeight * 2.f, FQuat::Identity, Roof)
		|| !Roof.IsValidBlockingHit() || Roof.ImpactNormal.Z < GetWalkableFloorZ() || Roof.ImpactPoint.Z < Location.Z - HalfHeight)
	{
		return false; // no roof within reach: a parapet in the way, or only the ground far below
	}
	SetGravityDirection(DefaultGravityDirection);
	UpdatedComponent->SetWorldLocationAndRotation(Roof.Location, FRotationMatrix::MakeFromX(Inward).ToQuat(), false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = Inward * Velocity.Size(); // keep running, now across the roof
	FindFloor(Roof.Location, CurrentFloor, false);
	return true;
}

bool UShinobiMovementComponent::TryWrapCorner(const FVector& Move)
{
	const FVector Normal = -GetGravityDirection();
	const FVector Along = FVector::VectorPlaneProject(Move, Normal).GetSafeNormal();
	if (Along.IsZero())
	{
		return false;
	}
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	// Past an outer corner the next face looks along the move: sweep back onto it from beyond the corner, then stand a
	// little way in from the corner so the whole capsule is over the new face.
	const FVector From = UpdatedComponent->GetComponentLocation() - Normal * (Capsule->GetScaledCapsuleHalfHeight() - Radius) + Along * (Radius + WallReach);
	FHitResult Hit;
	if (!SweepCapsule(From, From - Along * (Radius + 2.f * WallReach), UpdatedComponent->GetComponentQuat(), Hit)
		|| !IsClimbable(Hit) || !AttachToWall(Hit, Hit.ImpactPoint - Normal * (Radius + 5.f)))
	{
		return false;
	}
	// The camera turns with the wall, so the held key keeps running round the building instead of meaning "down" on the
	// new face. Only where the input comes from (the owning machine), and not again while replaying moves.
	AController* Controller = CharacterOwner->GetController();
	if (Controller && CharacterOwner->IsLocallyControlled() && !CharacterOwner->bClientUpdating)
	{
		const float Turn = FRotator::NormalizeAxis(Hit.ImpactNormal.Rotation().Yaw - Normal.Rotation().Yaw);
		Controller->SetControlRotation(Controller->GetControlRotation() + FRotator(0.f, Turn, 0.f));
	}
	return true;
}

void UShinobiMovementComponent::FaceDirection(AActor* Actor, const FVector& WorldDir)
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	const FVector Up = Character && Character->GetCharacterMovement() ? -Character->GetCharacterMovement()->GetGravityDirection() : FVector::UpVector;
	const FVector Forward = FVector::VectorPlaneProject(WorldDir, Up);
	if (Actor && !Forward.IsNearlyZero())
	{
		Actor->SetActorRotation(FRotationMatrix::MakeFromXZ(Forward, Up).Rotator());
	}
}
