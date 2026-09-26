#include "Shinobi/Abilities/GA_Rasengan.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Rasengan::UGA_Rasengan()
{
	InputID = EShinobiAbilityInputID::Jutsu3;
	DisplayName = INVTEXT("Rasengan");
	Glyph = INVTEXT("螺"); // spiral
	ChakraCost = 35.f;
	Cooldown = 10.f;
	WindUp = 0.8f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Rasengan);
	ActivationOwnedTags.AddTag(ShinobiTags::State_Casting_Rasengan); // hand orb (UShinobiCastFxComponent)
	CastMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Rasengan.AM_Rasengan")));
}

void UGA_Rasengan::ExecuteJutsu()
{
	FaceAim(nullptr); // the camera may have turned during the wind-up
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	AShinobiCharacterBase* Victim = FindNearestHostileInCone(Range, HalfAngle);
	if (Self && Victim)
	{
		FaceAim(Victim);
		if (HasAuthority(&CurrentActivationInfo))
		{
			const bool bBurning = Victim->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Burning);
			if (ApplyDamage(Victim, Damage))
			{
				AShinobiBurst::Spawn(Victim->GetWorld(), EShinobiBurst::Spiral, Victim->GetActorLocation());
				const FVector Push = (Victim->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D() * Knockback + FVector(0.f, 0.f, 300.f);
				Victim->LaunchCharacter(Push, true, true);
				if (bBurning)
				{
					Firestorm(Victim);
				}
			}
		}
	}
	EndJutsu();
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
