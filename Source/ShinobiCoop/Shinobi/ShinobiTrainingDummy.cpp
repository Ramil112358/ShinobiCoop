#include "Shinobi/ShinobiTrainingDummy.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/UI/ShinobiOverheadTextComponent.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDummyAttack(
		TEXT("Shinobi.DummyAttack"),
		false,
		TEXT("Dev aid until M3's enemies: training dummies punch the nearest player within reach every 2 s for 25 damage; ")
		TEXT("their text turns yellow just before. For trying Substitution, jutsu interrupts and the chakra-charge cancel."));
}

AShinobiTrainingDummy::AShinobiTrainingDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Team = EShinobiTeam::Enemies;
	AutoPossessAI = EAutoPossessAI::Disabled;

	HealthText = CreateDefaultSubobject<UShinobiOverheadTextComponent>(TEXT("HealthText"));
	HealthText->SetupAttachment(RootComponent);

	UseMannequinMesh();
}

void AShinobiTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The clock runs on every machine so clients see the telegraph too (PIE windows share the cvar); only the server punches.
	PunchClock = CVarDummyAttack.GetValueOnGameThread() ? PunchClock + DeltaSeconds : 0.f;
	if (PunchClock >= PunchInterval)
	{
		PunchClock = 0.f;
		if (HasAuthority())
		{
			Punch();
		}
	}
	const bool bTelegraph = PunchClock >= PunchInterval - PunchTelegraph;
	if (bTelegraph != bDisplayedTelegraph)
	{
		bDisplayedTelegraph = bTelegraph;
		HealthText->SetTextRenderColor(bTelegraph ? FColor::Yellow : FColor::Red);
		if (bTelegraph && HasAuthority())
		{
			LockPunchTarget();
		}
	}
}

void AShinobiTrainingDummy::LockPunchTarget()
{
	AShinobiCharacterBase* Nearest = nullptr;
	float NearestDistance = PunchReach;
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (IsHostileTo(*It) && Distance <= NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = *It;
		}
	}
	PunchTarget = Nearest;
	if (Nearest)
	{
		SetActorRotation(FRotator(0.f, (Nearest->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
	}
}

void AShinobiTrainingDummy::Punch()
{
	// Committed at the telegraph: a dodge that carries the target out of reach still "meets" the punch while
	// invulnerable, so a well-timed dodge triggers Substitution instead of just escaping it.
	const AShinobiCharacterBase* Target = PunchTarget.Get();
	PunchTarget = nullptr;
	if (Target && (FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()) <= PunchReach
		|| Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Invulnerable)))
	{
		ShinobiCombat::ApplyDamage(AbilitySystem, Target, PunchDamage);
	}
}

void AShinobiTrainingDummy::HandleOutOfHealth()
{
	Super::HandleOutOfHealth();
	GetWorldTimerManager().SetTimer(ResetTimer, this, &AShinobiTrainingDummy::ResetHealth, 1.5f, false);
}

void AShinobiTrainingDummy::ResetHealth()
{
	Attributes->SetHealth(Attributes->GetMaxHealth());
}
