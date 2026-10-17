#include "Shinobi/Enemies/GA_SummonAttacks.h"
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
