#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Susanoo::UGA_Susanoo()
{
	InputID = EShinobiAbilityInputID::Jutsu1;
	Page = 4;
	DisplayName = INVTEXT("Susanoo");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_Susanoo.T_Jutsu_Susanoo")));
	ChakraCost = 40.f;
	Cooldown = 60.f;
	WindUp = 0.6f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Susanoo);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Susanoo); // one armour at a time
}

void UGA_Susanoo::ExecuteJutsu()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		ApplyEffectToSelf(UGE_Susanoo::StaticClass());
		if (const AShinobiCharacterBase* Self = GetShinobiCharacter())
		{
			AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::SusanooRise, Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight()));
		}
	}
	EndJutsu();
}
