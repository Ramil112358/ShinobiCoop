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
	if (RetargetClock <= 0.f || !Target.IsValid() || !Target->IsTargetable())
	{
		RetargetClock = RetargetInterval;
		Target = FindNearestPlayer(*Self);
		bSteerDirectly = false;
		StopMovement(); // re-path toward the (possibly new) target below
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
	const bool bHeld = Self->IsBusy() || ASC->HasMatchingGameplayTag(ShinobiTags::State_Shocked) || ASC->HasMatchingGameplayTag(ShinobiTags::State_Staggered);
	FVector Post;
	if (!Victim && !bHeld && Self->GetIdlePost(Post))
	{
		GoToPost(*Self, Post);
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

void AShinobiAIController::GoToPost(AShinobiEnemyCharacter& Self, const FVector& Post)
{
	constexpr float ArriveWithin = 120.f;
	const FVector ToPost = (Post - Self.GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (ToPost.Size() <= ArriveWithin)
	{
		StopMovement();
		bSteerDirectly = false;
		return;
	}
	if (!bSteerDirectly && (GetMoveStatus() == EPathFollowingStatus::Idle || FVector::Dist2D(PostGoal, Post) > 200.f))
	{
		PostGoal = Post;
		bSteerDirectly = MoveToLocation(Post, ArriveWithin * 0.5f, true, true, false, false) == EPathFollowingRequestResult::Failed;
	}
	if (bSteerDirectly)
	{
		SetFocalPoint(Post, EAIFocusPriority::Move);
		Self.AddMovementInput(ToPost.GetSafeNormal());
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
