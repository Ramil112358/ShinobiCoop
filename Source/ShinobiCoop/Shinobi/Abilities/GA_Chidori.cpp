#include "Shinobi/Abilities/GA_Chidori.h"
#include "Animation/AnimMontage.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Chidori::UGA_Chidori()
{
	InputID = EShinobiAbilityInputID::Jutsu2;
	DisplayName = INVTEXT("Chidori");
	Glyph = INVTEXT("雷"); // lightning
	ChakraCost = 30.f;
	Cooldown = 8.f;
	WindUp = 0.6f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Chidori);
	ActivationOwnedTags.AddTag(ShinobiTags::State_Casting_Chidori); // hand orb (UShinobiCastFxComponent)
	CastMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Chidori.AM_Chidori")));
}

void UGA_Chidori::ExecuteJutsu()
{
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self)
	{
		EndJutsu();
		return;
	}
	FaceAim(nullptr); // the camera may have turned during the wind-up
	float Distance = DashDistance;
	if (const AShinobiCharacterBase* Target = FindNearestHostileInCone(DashDistance, SnapHalfAngle))
	{
		FaceAim(Target);
		Distance = FMath::Max(0.f, FVector::Dist2D(Self->GetActorLocation(), Target->GetActorLocation()) - StopShort);
	}
	DashYaw = Self->GetActorRotation().Yaw;
	if (Distance < 1.f)
	{
		OnDashFinished();
		return;
	}
	UAbilityTask_ApplyRootMotionConstantForce* Dash = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Chidori"), Self->GetActorForwardVector().GetSafeNormal2D(), DashSpeed, Distance / DashSpeed, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 200.f, true);
	Dash->OnFinish.AddDynamic(this, &UGA_Chidori::OnDashFinished);
	Dash->ReadyForActivation();
}

void UGA_Chidori::OnDashFinished()
{
	// Strike along the dash, not the current facing: movement input held during the dash turns the character
	// (bOrientRotationToMovement), which would swing the strike cone off the target.
	if (AActor* Self = GetAvatarActorFromActorInfo())
	{
		Self->SetActorRotation(FRotator(0.f, DashYaw, 0.f));
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		AShinobiCharacterBase* Victim = FindNearestHostileInCone(StrikeRange, 60.f);
		if (Victim && ApplyDamage(Victim, Damage))
		{
			AShinobiBurst::Spawn(Victim->GetWorld(), EShinobiBurst::Lightning, Victim->GetActorLocation());
			// Bonus after the hit, so it boosts the follow-up rather than this strike.
			UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
			ShinobiCombat::ApplyStatus(Source, Victim, UGE_Shock::StaticClass());
			ShinobiCombat::ApplyStatus(Source, Victim, UGE_ShockBonus::StaticClass());
		}
	}
	EndJutsu();
}
