#include "Shinobi/Enemies/GA_SummonAttacks.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_PabuBite::UGA_PabuBite()
{
	Hits = 1;
	WindUp = 0.4f;
	Recovery = 0.35f;
	Range = 190.f;
	HalfAngle = 70.f;
	WindUpMontage = nullptr; // Pabu plays its own clip, on State.SummonAttacking
	FollowUpMontage = nullptr;
	ActivationOwnedTags.AddTag(ShinobiTags::State_SummonAttacking);
}

UGA_PushoPounce::UGA_PushoPounce()
{
	Hits = 1;
	WindUp = 0.45f;
	Recovery = 0.45f;
	Range = 200.f;
	HalfAngle = 70.f;
	WindUpMontage = nullptr; // Pusho plays his own clip, on State.SummonAttacking
	FollowUpMontage = nullptr;
	ActivationOwnedTags.AddTag(ShinobiTags::State_SummonAttacking);
}

void UGA_PushoPounce::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bLeapt = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGA_PushoPounce::Strike()
{
	if (bLeapt)
	{
		Super::Strike(); // landed: the claw
		return;
	}
	bLeapt = true;
	AShinobiEnemyCharacter* Self = GetEnemy();
	if (!Self)
	{
		EndNow();
		return;
	}
	// A ballistic arc that comes down LeapSeconds from now just short of where the target is (or on the spot, with no target). Never
	// aimed downward: a target lower down is reached by falling further, so there is always a hop.
	const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight());
	FVector Land = Feet;
	if (const AShinobiCharacterBase* Target = LockedTarget.Get(); Target && Target->IsAlive())
	{
		const FVector TargetFeet = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
		const FVector To = TargetFeet - Feet;
		const float Gap = Self->GetSimpleCollisionRadius() + Target->GetSimpleCollisionRadius() + StopShort;
		Land = Feet + To.GetSafeNormal2D() * FMath::Max(To.Size2D() - Gap, 0.f) + FVector(0.f, 0.f, FMath::Max(To.Z, 0.f));
		Self->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	}
	const FVector Delta = Land - Feet;
	FVector Velocity = Delta / LeapSeconds;
	Velocity.Z -= 0.5f * Self->GetCharacterMovement()->GetGravityZ() * LeapSeconds;
	Self->LaunchCharacter(Velocity, true, true);
	StrikeAfter(LeapSeconds);
}

UGA_ToadSlam::UGA_ToadSlam()
{
	ActivationOwnedTags.AddTag(ShinobiTags::State_SummonAttacking);
	WindUp = 0.7f;
	Recovery = 0.5f;
}

void UGA_ToadSlam::Strike()
{
	AShinobiEnemyCharacter* Self = GetEnemy();
	if (Self)
	{
		const float Reach = Radius * Self->GetActorScale3D().X;
		const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight());
		for (AShinobiCharacterBase* Target : ShinobiCombat::FindHostilesInRadius(Self, Feet, Reach))
		{
			ShinobiCombat::ApplyDamage(Self->GetAbilitySystemComponent(), Target, Self->AttackDamage);
		}
		AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::Shockwave, Feet, Reach / 650.f);
	}
	Finish();
}

UGA_MishaBark::UGA_MishaBark()
{
	ActivationOwnedTags.AddTag(ShinobiTags::State_SummonAttacking);
	WindUp = 0.5f;
	Recovery = 0.5f;
}

void UGA_MishaBark::Strike()
{
	AShinobiEnemyCharacter* Self = GetEnemy();
	if (!Self)
	{
		EndNow();
		return;
	}
	AShinobiCharacterBase* Locked = LockedTarget.Get();
	if (Locked && Locked->IsAlive())
	{
		Self->SetActorRotation(FRotator(0.f, (Locked->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f));
	}
	TArray<AShinobiCharacterBase*> Targets = FindHostilesInCone(Range, HalfAngle);
	if (Locked && Locked->IsAlive() && FVector::Dist2D(Locked->GetActorLocation(), Self->GetActorLocation()) <= Range)
	{
		Targets.AddUnique(Locked);
	}
	for (AShinobiCharacterBase* Target : Targets)
	{
		if (!ApplyDamage(Target, Self->AttackDamage))
		{
			continue; // dodged (invulnerable) or phased: the bark passes through
		}
		ShinobiCombat::ApplyStatus(Self->GetAbilitySystemComponent(), Target, UGE_Stagger::StaticClass());
		const FVector Away = (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
		Target->LaunchCharacter((Away.IsNearlyZero() ? Self->GetActorForwardVector() : Away) * LaunchSpeed + FVector(0.f, 0.f, LaunchUp), true, true);
	}
	// The bark's ring of force rolls out over the ground in front of him.
	const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight());
	AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::Shockwave, Feet + Self->GetActorForwardVector() * Range * 0.45f, Range / 1300.f);
	Finish();
}
