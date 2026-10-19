#include "Shinobi/Abilities/GA_Rasengan.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Rasengan::UGA_Rasengan()
{
	DisplayName = INVTEXT("Rasengan");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_Rasengan.T_Jutsu_Rasengan")));
	ChakraCost = 35.f;
	Cooldown = 10.f;
	WindUp = 0.4f;
	DashDistance = 1200.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Rasengan);
	ActivationOwnedTags.AddTag(ShinobiTags::State_Casting_Rasengan); // hand orb (UShinobiCastFxComponent)
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Rasengan_Seal.AM_Rasengan_Seal")));
	ChargeMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Rasengan_Charge.AM_Rasengan_Charge")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Rasengan_Release.AM_Rasengan_Release")));
}

void UGA_Rasengan::Strike(AShinobiCharacterBase* Victim)
{
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	const bool bBurning = Victim->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Burning);
	if (Self && ApplyDamage(Victim, Damage * GetDamageScale()))
	{
		AShinobiBurst::Spawn(Victim->GetWorld(), EShinobiBurst::Spiral, Victim->GetActorLocation(), GetVisualScale());
		const FVector Push = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D() * Knockback * GetDamageScale() + FVector(0.f, 0.f, 300.f);
		Victim->LaunchCharacter(Push, true, true);
		if (bBurning)
		{
			Firestorm(Victim);
		}
	}
}

void UGA_Rasengan::Firestorm(AShinobiCharacterBase* Center) const
{
	AShinobiBurst::Spawn(Center->GetWorld(), EShinobiBurst::Firestorm, Center->GetActorLocation());
	UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
	for (const AShinobiCharacterBase* Caught : ShinobiCombat::FindHostilesInRadius(GetShinobiCharacter(), Center->GetActorLocation(), FirestormRadius))
	{
		ShinobiCombat::ApplyDamage(Source, Caught, FirestormDamage);
	}
	Center->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Burning_Normal));
}
