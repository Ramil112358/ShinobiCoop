#include "Shinobi/Abilities/GA_Dodge.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Dodge::UGA_Dodge()
{
	InputID = EShinobiAbilityInputID::Dodge;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Dodge));
	ActivationOwnedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Mounted); // Control dives on a dragon
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Jutsu);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Block); // dodging out of a guard
	CostGameplayEffectClass = UGE_DodgeCost::StaticClass();
	Montage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Dodge.AM_Dodge")));
	IFramesEffect = UGE_DodgeIFrames::StaticClass();
	WindowEffect = UGE_BlinkWindow::StaticClass();
}

void UGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AShinobiCharacterBase* Character = GetShinobiCharacter();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		ApplyEffectToSelf(IFramesEffect);
		ApplyEffectToSelf(UGE_StaminaRegenDelay::StaticClass());
	}
	if (WindowEffect)
	{
		ApplyEffectToSelf(WindowEffect);
	}
	DashStart = Character->GetActorLocation();

	// Along the surface the character stands on: the ground, or the wall it runs on.
	const FVector Up = Character->GetActorUpVector();
	FVector Direction = FVector::VectorPlaneProject(Character->GetCharacterMovement()->GetCurrentAcceleration(), Up).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = -FVector::VectorPlaneProject(Character->GetActorForwardVector(), Up).GetSafeNormal(); // backstep
	}
	// "Gravity" on a root motion force keeps world Z, which would cancel a dodge up or down a wall.
	const UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(Character->GetCharacterMovement());
	const bool bKeepWorldZ = !(Movement && Movement->IsWallTraversing());

	if (UAnimMontage* Anim = Montage.IsNull() ? nullptr : Montage.LoadSynchronous())
	{
		// bStopWhenAbilityEnds = false: the 0.25 s dash ends the ability before AM_Dodge finishes (~0.32 s
		// at setup_m1.py's 3x rate), and the animation is cosmetic only (root motion is disabled on the
		// duplicated sequence in Task 12), so let it play out instead of cutting it off.
		UAbilityTask_PlayMontageAndWait* Play = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Anim, 1.f, NAME_None, false);
		Play->ReadyForActivation();
	}

	UAbilityTask_ApplyRootMotionConstantForce* Push = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Dodge"), Direction, Distance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 200.f, bKeepWorldZ);
	Push->OnFinish.AddDynamic(this, &UGA_Dodge::OnDodgeFinished);
	Push->ReadyForActivation();
}

void UGA_Dodge::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

UGA_Blink::UGA_Blink()
{
	InputID = EShinobiAbilityInputID::None; // routed ahead of the dodge by the player character
	ActivationOwnedTags.AddTag(ShinobiTags::State_Blinking);
	ActivationBlockedTags.RemoveTag(ShinobiTags::State_Dodging); // blinks out of the dodge it follows
	ActivationRequiredTags.AddTag(ShinobiTags::State_BlinkReady);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Dodge);
	CostGameplayEffectClass = UGE_BlinkCost::StaticClass();
	CooldownGameplayEffectClass = UGE_BlinkCooldown::StaticClass();
	IFramesEffect = UGE_BlinkIFrames::StaticClass();
	WindowEffect = nullptr;
	Distance = 800.f;
	Duration = 0.1f;
	Montage.Reset(); // the body is hidden throughout
}

void UGA_Blink::OnDodgeFinished()
{
	const AShinobiCharacterBase* Character = GetShinobiCharacter();
	if (Character && HasAuthority(&CurrentActivationInfo))
	{
		// From feet to feet: the streak is drawn at chest height above it, the crackle on the ground.
		const FVector Feet = Character->GetActorUpVector() * Character->GetSimpleCollisionHalfHeight();
		const FVector Path = Character->GetActorLocation() - DashStart;
		AShinobiBurst::Spawn(Character->GetWorld(), EShinobiBurst::BlinkStreak, DashStart - Feet, Path.Size() / AShinobiBurst::BlinkLength, Path.Rotation());
	}
	Super::OnDodgeFinished();
}
