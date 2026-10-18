#include "Shinobi/Abilities/ShinobiDashJutsu.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiMovementComponent.h"

void UShinobiDashJutsu::ExecuteJutsu()
{
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self)
	{
		EndJutsu();
		return;
	}
	FaceAim(nullptr); // the camera may have turned during the seals and the charge
	float Distance = DashDistance;
	if (const AShinobiCharacterBase* Target = FindNearestHostileInCone(DashDistance, SnapHalfAngle))
	{
		FaceAim(Target);
		Distance = FMath::Max(0.f, FVector::VectorPlaneProject(Target->GetActorLocation() - Self->GetActorLocation(), Self->GetActorUpVector()).Size() - StopShort);
	}
	DashRotation = Self->GetActorQuat();
	DashStart = Self->GetActorLocation();
	if (Distance < 1.f)
	{
		OnDashFinished();
		return;
	}
	// "Gravity" on a root motion force keeps world Z, which would cancel a dash up or down a wall. Off walls it's additive:
	// the normal movement (move input) adds to it, so the player can steer a little; on a wall it stays a straight push.
	const UShinobiMovementComponent* Movement = GetShinobiMovement();
	const bool bWall = Movement && Movement->IsWallTraversing();
	UAbilityTask_ApplyRootMotionConstantForce* Dash = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("JutsuDash"), Self->GetActorForwardVector(), DashSpeed, Distance / DashSpeed, !bWall, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 200.f, !bWall);
	Dash->OnFinish.AddDynamic(this, &UShinobiDashJutsu::OnDashFinished);
	Dash->ReadyForActivation();
}

void UShinobiDashJutsu::OnDashFinished()
{
	// Strike along the dash, not the current facing: movement input held during the dash turns the character
	// (bOrientRotationToMovement), which would swing the strike cone off the target.
	if (AActor* Self = GetAvatarActorFromActorInfo())
	{
		// The way the dash went (it may have been steered), else the starting facing; it may have left or reached a wall.
		const FVector Up = DashRotation.GetUpVector();
		const FVector Went = FVector::VectorPlaneProject(Self->GetActorLocation() - DashStart, Up);
		UShinobiMovementComponent::FaceDirection(Self, Went.Size() > 100.f ? Went.GetSafeNormal() : DashRotation.GetForwardVector());
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		if (AShinobiCharacterBase* Victim = FindNearestHostileInCone(StrikeRange, 60.f))
		{
			Strike(Victim);
		}
	}
	EndJutsu();
}
