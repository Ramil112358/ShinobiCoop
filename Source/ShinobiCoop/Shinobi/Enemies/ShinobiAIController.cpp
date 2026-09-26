#include "Shinobi/Enemies/ShinobiAIController.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"

AShinobiAIController::AShinobiAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AShinobiAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AShinobiEnemyCharacter* Self = Cast<AShinobiEnemyCharacter>(GetPawn());
	if (!Self || !Self->IsAlive())
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	RetargetClock -= DeltaSeconds;
	if (RetargetClock <= 0.f || !Target.IsValid() || !Target->IsAlive())
	{
		RetargetClock = RetargetInterval;
		Target = FindNearestPlayer(*Self);
		if (Target.IsValid())
		{
			SetFocus(Target.Get());
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
	const AShinobiCharacterBase* Victim = Target.Get();
	UAbilitySystemComponent* ASC = Self->GetAbilitySystemComponent();
	if (!Victim || Self->IsBusy() || ASC->HasMatchingGameplayTag(ShinobiTags::State_Shocked) || ASC->HasMatchingGameplayTag(ShinobiTags::State_Staggered))
	{
		return;
	}

	const FVector ToTarget = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
	const float Distance = FVector::Dist2D(Victim->GetActorLocation(), Self->GetActorLocation());
	const bool bRanged = Self->PreferredRange > 0.f;
	const float Now = GetWorld()->GetTimeSeconds();

	if (Distance <= Self->AttackRange && (!bRanged || LineOfSightTo(Victim)))
	{
		if (Now >= NextAttackTime && ASC->TryActivateAbilityByClass(*Self->AttackAbility))
		{
			NextAttackTime = Now + Self->AttackInterval;
			return;
		}
	}
	else if (Self->SpecialAbility && Distance <= Self->SpecialRange && ASC->TryActivateAbilityByClass(*Self->SpecialAbility))
	{
		return;
	}

	if (bRanged && Distance < Self->PreferredRange * 0.75f)
	{
		Self->AddMovementInput(-ToTarget); // too close: back off while facing the target
	}
	else if (Distance > (bRanged ? Self->PreferredRange : Self->AttackRange * 0.8f))
	{
		Self->AddMovementInput(ToTarget);
	}
}

AShinobiCharacterBase* AShinobiAIController::FindNearestPlayer(const AShinobiEnemyCharacter& Self) const
{
	AShinobiCharacterBase* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::DistSquared2D(It->GetActorLocation(), Self.GetActorLocation());
		if (Self.IsHostileTo(*It) && It->IsAlive() && Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = *It;
		}
	}
	return Nearest;
}
