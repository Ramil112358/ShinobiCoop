#include "Shinobi/Abilities/ShinobiDashJutsu.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Shinobi/ShinobiCharacterBase.h"

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
	if (Distance < 1.f)
	{
		OnDashFinished();
		return;
	}
	UAbilityTask_ApplyRootMotionConstantForce* Dash = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("JutsuDash"), Self->GetActorForwardVector(), DashSpeed, Distance / DashSpeed, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 200.f, true);
	Dash->OnFinish.AddDynamic(this, &UShinobiDashJutsu::OnDashFinished);
	Dash->ReadyForActivation();
}

void UShinobiDashJutsu::OnDashFinished()
{
	// Strike along the dash, not the current facing: movement input held during the dash turns the character
	// (bOrientRotationToMovement), which would swing the strike cone off the target.
	if (AActor* Self = GetAvatarActorFromActorInfo())
	{
		Self->SetActorRotation(DashRotation);
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
