#include "Shinobi/Enemies/GA_SummonAttacks.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_ToadSlam::UGA_ToadSlam()
{
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

UGA_SnakeStrike::UGA_SnakeStrike()
{
	WindUp = 0.5f;
	Recovery = 0.4f;
}

void UGA_SnakeStrike::Strike()
{
	const AShinobiEnemyCharacter* Self = GetEnemy();
	AShinobiCharacterBase* Target = LockedTarget.Get();
	if (Self && Target && Target->IsAlive() && FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation()) <= Self->AttackRange * 1.3f)
	{
		ShinobiCombat::ApplyDamage(Self->GetAbilitySystemComponent(), Target, Self->AttackDamage);
	}
	Finish();
}
