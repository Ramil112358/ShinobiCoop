#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Shinobi/ShinobiProjectile.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"

namespace
{
	/** The player combo's hit frame (UGA_Melee::HitDelay): the clips are slowed so their hit meets the wind-up. */
	constexpr float ClipHitTime = 0.25f;
}

UGA_EnemyAttack::UGA_EnemyAttack()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_EnemyAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	LockedTarget = Cast<AShinobiCharacterBase>(AI ? AI->GetFocusActor() : nullptr);
	StrikeCount = 0;
	PlaySwing(WindUpMontage, WindUp);
	StrikeAfter(WindUp);
}

void UGA_EnemyAttack::PlaySwing(const TSoftObjectPtr<UAnimMontage>& Anim, float Seconds)
{
	if (UAnimMontage* Loaded = Anim.LoadSynchronous())
	{
		const float Rate = FMath::Clamp(ClipHitTime / Seconds, 0.3f, 1.f);
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Loaded, Rate)->ReadyForActivation();
	}
}

void UGA_EnemyAttack::After(float Seconds, void (UGA_EnemyAttack::*Step)())
{
	PendingStep = Step;
	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Seconds);
	Wait->OnFinish.AddDynamic(this, &UGA_EnemyAttack::RunStep);
	Wait->ReadyForActivation();
}

void UGA_EnemyAttack::RunStep()
{
	(this->*PendingStep)();
}

void UGA_EnemyAttack::EndNow()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

AShinobiEnemyCharacter* UGA_EnemyAttack::GetEnemy() const
{
	return Cast<AShinobiEnemyCharacter>(GetAvatarActorFromActorInfo());
}

bool UGA_EnemyAttack::CanReachLockedTarget(float Range) const
{
	const AShinobiCharacterBase* Target = LockedTarget.Get();
	const AActor* Self = GetAvatarActorFromActorInfo();
	return Target && Self && Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Invulnerable)
		&& FVector::Dist2D(Target->GetActorLocation(), Self->GetActorLocation()) <= Range;
}

UGA_EnemyMelee::UGA_EnemyMelee()
{
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Melee));
	WindUpMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_1.AM_Melee_1")));
	FollowUpMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_2.AM_Melee_2")));
}

void UGA_EnemyMelee::Strike()
{
	AShinobiEnemyCharacter* Self = GetEnemy();
	if (!Self)
	{
		EndNow();
		return;
	}
	const float Reach = Range * Self->GetActorScale3D().X;
	const TArray<AShinobiCharacterBase*> Targets = FindHostilesInCone(Reach, HalfAngle);
	for (AShinobiCharacterBase* Target : Targets)
	{
		ApplyDamage(Target, Self->AttackDamage);
	}
	// A dodge through the swing still meets it (double reach), so it triggers Substitution.
	if (!Targets.Contains(LockedTarget.Get()) && CanReachLockedTarget(Reach * 2.f))
	{
		ApplyDamage(LockedTarget.Get(), Self->AttackDamage);
	}

	if (++StrikeCount >= Hits)
	{
		Finish();
		return;
	}
	if (const AShinobiCharacterBase* Target = LockedTarget.Get(); Target && Target->IsAlive())
	{
		Self->SetActorRotation(FRotator(0.f, (Target->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f));
	}
	PlaySwing(FollowUpMontage, HitGap);
	StrikeAfter(HitGap);
}

UGA_EnemyKunai::UGA_EnemyKunai()
{
	WindUp = 0.6f;
	Recovery = 0.4f;
	WindUpMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_1.AM_Melee_1")));
}

void UGA_EnemyKunai::Strike()
{
	AShinobiEnemyCharacter* Self = GetEnemy();
	const AShinobiCharacterBase* Target = LockedTarget.Get();
	if (Self && Target && Target->IsAlive())
	{
		const FVector Muzzle = Self->GetActorLocation() + Self->GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);
		const FTransform Launch((Target->GetActorLocation() - Muzzle).Rotation(), Muzzle);
		if (AShinobiProjectile* Kunai = Self->GetWorld()->SpawnActorDeferred<AShinobiProjectile>(AShinobiKunai::StaticClass(), Launch, Self, Self, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Kunai->DirectDamage = Self->AttackDamage;
			Kunai->FinishSpawning(Launch);
		}
	}
	Finish();
}

UGA_EnemyFireball::UGA_EnemyFireball()
{
	InputID = EShinobiAbilityInputID::None;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ChakraCost = 0.f;
	Cooldown = 8.f;
	WindUp = 0.8f;
	DirectDamage = 90.f;
}
