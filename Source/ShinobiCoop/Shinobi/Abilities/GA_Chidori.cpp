#include "Shinobi/Abilities/GA_Chidori.h"
#include "Animation/AnimMontage.h"
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
	WindUp = 0.5f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Chidori);
	ActivationOwnedTags.AddTag(ShinobiTags::State_Casting_Chidori); // hand orb (UShinobiCastFxComponent)
	// ponytail: the old one-clip cast stands in for the seals until the per-phase montages are imported (plan Task 8)
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Chidori.AM_Chidori")));
}

void UGA_Chidori::Strike(AShinobiCharacterBase* Victim)
{
	if (ApplyDamage(Victim, Damage * GetDamageScale()))
	{
		AShinobiBurst::Spawn(Victim->GetWorld(), EShinobiBurst::Lightning, Victim->GetActorLocation(), GetVisualScale());
		// Bonus after the hit, so it boosts the follow-up rather than this strike.
		UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
		ShinobiCombat::ApplyStun(Source, Victim);
		ShinobiCombat::ApplyStatus(Source, Victim, UGE_ShockBonus::StaticClass());
	}
}
