#include "Shinobi/Abilities/GA_FireMeteor.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMeteor.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/ShinobiTelegraph.h"
#include "Shinobi/VFX/ShinobiVfx.h"

UGA_FireMeteor::UGA_FireMeteor()
{
	InputID = EShinobiAbilityInputID::Jutsu3;
	Page = 3;
	DisplayName = INVTEXT("Fire Meteor");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_FireMeteor.T_Jutsu_FireMeteor")));
	ChakraCost = 50.f;
	Cooldown = 18.f;
	WindUp = 0.6f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_FireMeteor);
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_FireMeteor_Seal.AM_FireMeteor_Seal")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_FireMeteor_Release.AM_FireMeteor_Release")));
}

bool UGA_FireMeteor::PrepareJutsu()
{
	return FindAimGround(Range, 30.f, Target);
}

void UGA_FireMeteor::ExecuteJutsu()
{
	FaceAim(nullptr);
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (Self && HasAuthority(&CurrentActivationInfo))
	{
		const float Radius_ = Radius * GetVisualScale();
		AShinobiTelegraph::Spawn(Self->GetWorld(), Target, Radius_, FallSeconds, ShinobiVfx::Colors::Fire);
		FShinobiMeteorImpact Impact;
		Impact.Damage = Damage * GetDamageScale();
		Impact.Radius = Radius_;
		Impact.Status = UGE_Burn::StaticClass();
		Impact.StatusDamagePerTick = UGE_Burn::DamagePerTick * GetDamageScale();
		Impact.FirePatchScale = 2.f * GetVisualScale();
		AShinobiMeteor::Spawn(Self->GetWorld(), Self, EShinobiMeteor::Fire, Target, FallSeconds, GetVisualScale(), Impact);
	}
	EndJutsu();
}
