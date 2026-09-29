#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Animation/AnimMontage.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Amaterasu::UGA_Amaterasu()
{
	InputID = EShinobiAbilityInputID::Jutsu4;
	DisplayName = INVTEXT("Amaterasu");
	Glyph = INVTEXT("天"); // heaven
	ChakraCost = 50.f;
	Cooldown = 20.f;
	WindUp = 0.4f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Amaterasu);
	ActivationOwnedTags.AddTag(ShinobiTags::State_Casting_Amaterasu); // hand orb (UShinobiCastFxComponent)
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Amaterasu_Seal.AM_Amaterasu_Seal")));
	ChargeMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Amaterasu_Charge.AM_Amaterasu_Charge")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Amaterasu_Release.AM_Amaterasu_Release")));
}

bool UGA_Amaterasu::PrepareJutsu()
{
	Target = nullptr;
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	FHitResult Hit;
	if (Self && TraceAim(Range, AimRadius, Hit))
	{
		AShinobiCharacterBase* Candidate = Cast<AShinobiCharacterBase>(Hit.GetActor());
		if (Self->IsHostileTo(Candidate) && FVector::Dist(Self->GetActorLocation(), Candidate->GetActorLocation()) <= Range)
		{
			Target = Candidate;
		}
	}
	return Target.IsValid();
}

void UGA_Amaterasu::ExecuteJutsu()
{
	if (Target.IsValid() && HasAuthority(&CurrentActivationInfo))
	{
		if (ShinobiCombat::ApplyStatus(GetAbilitySystemComponentFromActorInfo(), Target.Get(), UGE_BlackFlame::StaticClass(), UGE_BlackFlame::DamagePerTick * GetDamageScale()))
		{
			AShinobiBurst::Spawn(Target->GetWorld(), EShinobiBurst::BlackFlameIgnite, Target->GetActorLocation(), GetVisualScale());
		}
	}
	EndJutsu();
}
