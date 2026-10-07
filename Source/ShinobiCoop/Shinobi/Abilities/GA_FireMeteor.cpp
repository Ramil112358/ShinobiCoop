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
	return UpdateJutsuTarget(Target);
}

FLinearColor UGA_FireMeteor::GetTargetPreviewTint() const
{
	return FLinearColor(0.02f, 0.85f, 0.75f);
}

bool UGA_FireMeteor::UpdateJutsuTarget(FVector& OutTarget)
{
	if (!FindAimGround(Range, 30.f, OutTarget))
	{
		return false;
	}
	Target = OutTarget;
	return true;
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
