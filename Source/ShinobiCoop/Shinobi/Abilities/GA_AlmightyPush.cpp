#include "Shinobi/Abilities/GA_AlmightyPush.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_AlmightyPush::UGA_AlmightyPush()
{
	DisplayName = INVTEXT("Almighty Push");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_AlmightyPush.T_Jutsu_AlmightyPush")));
	ChakraCost = 30.f;
	Cooldown = 8.f;
	WindUp = 0.5f;
	ReleaseDelay = 0.15f; // the wave leaves on the palm thrust, not at the start of the release clip
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_AlmightyPush);
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_AlmightyPush_Seal.AM_AlmightyPush_Seal")));
	ChargeMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_AlmightyPush_Charge.AM_AlmightyPush_Charge")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_AlmightyPush_Release.AM_AlmightyPush_Release")));
}

void UGA_AlmightyPush::ExecuteJutsu()
{
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (Self && HasAuthority(&CurrentActivationInfo))
	{
		UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
		for (AShinobiCharacterBase* Victim : ShinobiCombat::FindHostilesInRadius(Self, Self->GetActorLocation(), Radius * GetVisualScale()))
		{
			if (!ShinobiCombat::ApplyDamage(Source, Victim, Damage * GetDamageScale()))
			{
				continue; // dodged (invulnerable) or phased: the wave passes through
			}
			ShinobiCombat::ApplyStatus(Source, Victim, UGE_Stagger::StaticClass());
			FVector Away = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = Self->GetActorForwardVector().GetSafeNormal2D();
			}
			Victim->LaunchCharacter(Away * LaunchSpeed * GetVisualScale() + FVector(0.f, 0.f, LaunchUp), true, true);
		}
		AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::AlmightyPush, Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()), GetVisualScale());
	}
	EndJutsu();
}
