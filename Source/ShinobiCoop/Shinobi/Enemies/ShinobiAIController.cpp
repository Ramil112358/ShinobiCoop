#include "Shinobi/Enemies/ShinobiAIController.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Navigation/PathFollowingComponent.h"
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
	// Look again every interval, and at once when the target is lost (not while there simply is nobody: that waits for the interval).
	if (RetargetClock <= 0.f || (bHadTarget && (!Target.IsValid() || !Target->IsTargetable())))
	{
		RetargetClock = RetargetInterval;
		Target = FindNearestPlayer(*Self);
		bHadTarget = Target.IsValid();
		bSteerDirectly = false;
		if (Target.IsValid() || !Self->GetFollowTarget())
		{
			StopMovement(); // re-path toward the (possibly new) target below; a summon walking back to its caster keeps walking
		}
		if (Target.IsValid())
		{
			SetFocus(Target.Get());
		}
		else if (!Self->GetFollowTarget())
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
	const AShinobiCharacterBase* Victim = Target.Get();
	UAbilitySystemComponent* ASC = Self->GetAbilitySystemComponent();
	const bool bHeld = Self->IsBusy() || ASC->HasMatchingGameplayTag(ShinobiTags::State_Shocked) || ASC->HasMatchingGameplayTag(ShinobiTags::State_Staggered);
	if (!Victim && !bHeld && Self->GetFollowTarget())
	{
		FollowLeader(*Self);
		return;
	}
	if (!Victim || bHeld)
	{
		StopMovement();
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

	const float StopDistance = bRanged ? Self->PreferredRange : Self->AttackRange * 0.8f;
	if (bRanged && Distance < Self->PreferredRange * 0.75f)
	{
		StopMovement();
		Self->AddMovementInput(-ToTarget); // too close: back off while facing the target
	}
	else if (Distance > StopDistance)
	{
		if (!bSteerDirectly && GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			// bCanStrafe: keep the focus on the target instead of turning to face the path.
			bSteerDirectly = MoveToActor(Target.Get(), StopDistance * 0.9f, true, true, true) == EPathFollowingRequestResult::Failed;
		}
		if (bSteerDirectly)
		{
			Self->AddMovementInput(ToTarget);
		}
	}
	else
	{
		StopMovement();
	}
}

void AShinobiAIController::FollowLeader(AShinobiEnemyCharacter& Self)
{
	const AShinobiCharacterBase* Leader = Self.GetFollowTarget();
	const float Distance = FVector::Dist2D(Leader->GetActorLocation(), Self.GetActorLocation());
	if (Distance <= Self.GetFollowDistance())
	{
		StopMovement();
		bSteerDirectly = false;
		return;
	}
	SetFocus(const_cast<AShinobiCharacterBase*>(Leader)); // faces them as it comes
	if (!bSteerDirectly && GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		bSteerDirectly = MoveToActor(const_cast<AShinobiCharacterBase*>(Leader), Self.GetFollowDistance() * 0.6f, true, true, true) == EPathFollowingRequestResult::Failed;
	}
	if (bSteerDirectly)
	{
		Self.AddMovementInput((Leader->GetActorLocation() - Self.GetActorLocation()).GetSafeNormal2D());
	}
}

AShinobiCharacterBase* AShinobiAIController::FindNearestPlayer(const AShinobiEnemyCharacter& Self) const
{
	AShinobiCharacterBase* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::DistSquared2D(It->GetActorLocation(), Self.GetActorLocation());
		if (Self.IsHostileTo(*It) && It->IsTargetable() && Distance < NearestDistance && Self.CanTarget(**It))
		{
			NearestDistance = Distance;
			Nearest = *It;
		}
	}
	return Nearest;
}
