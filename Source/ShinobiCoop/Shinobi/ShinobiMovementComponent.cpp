#include "Shinobi/ShinobiMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	class FShinobiSavedMove : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		uint8 bSavedWantsToSprint : 1;

		virtual void Clear() override
		{
			Super::Clear();
			bSavedWantsToSprint = false;
		}

		virtual uint8 GetCompressedFlags() const override
		{
			uint8 Result = Super::GetCompressedFlags();
			if (bSavedWantsToSprint)
			{
				Result |= FLAG_Custom_0;
			}
			return Result;
		}

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
		{
			if (bSavedWantsToSprint != static_cast<FShinobiSavedMove*>(NewMove.Get())->bSavedWantsToSprint)
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
			}
		}

		virtual void PrepMoveFor(ACharacter* C) override
		{
			Super::PrepMoveFor(C);
			if (UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(C->GetCharacterMovement()))
			{
				Movement->bWantsToSprint = bSavedWantsToSprint;
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
	MaxWalkSpeed = 600.f;
	BrakingDecelerationFlying = 2500.f; // a dragon stops and turns quickly
	// Uncontrolled characters (dummies, knocked-back pawns, test actors) still need physics and root motion.
	bRunPhysicsWithNoController = true;
}

void UShinobiMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
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
	if (bMounted && MovementMode == MOVE_Flying && CharacterOwner)
	{
		// Keep the dragon off the ground: from the capsule's floor, look down; too close pushes up, harder the closer it is (so a
		// mount from the ground is a quick lift-off, and a dive eases out above the floor).
		const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		const FVector Feet = CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MountClearance), false, CharacterOwner);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Feet, Feet - FVector(0.f, 0.f, MountedGroundClearance * 2.f), ECC_WorldStatic, Params) && Hit.Distance < MountedGroundClearance)
		{
			Velocity.Z = FMath::Max(Velocity.Z, (MountedGroundClearance - Hit.Distance) * 8.f);
		}
	}
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

float UShinobiMovementComponent::GetMaxSpeed() const
{
	float Speed = (bWantsToSprint && MovementMode == MOVE_Walking) ? SprintSpeed : Super::GetMaxSpeed();
	const AShinobiCharacterBase* Character = Cast<AShinobiCharacterBase>(GetOwner());
	if (MovementMode == MOVE_Flying && Character && Character->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Mounted))
	{
		Speed = FlightSpeed;
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
	// bWantsToSprint with each replayed move's flag in turn. The engine does the same
	// save/restore for bWantsToCrouch around this call (CharacterMovementComponent.cpp), so we
	// mirror that here: the last replayed move would otherwise win instead of the current input.
	const bool bRealWantsToSprint = bWantsToSprint;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealWantsToSprint;
	return bResult;
}

void UShinobiMovementComponent::ForceRotationCorrectionFor(float Seconds)
{
	ForceRotationCorrectionUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

bool UShinobiMovementComponent::ShouldCorrectRotation() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < ForceRotationCorrectionUntil;
}
