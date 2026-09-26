#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Amaterasu::UGA_Amaterasu()
{
	InputID = EShinobiAbilityInputID::Jutsu4;
	DisplayName = INVTEXT("Amaterasu");
	Glyph = INVTEXT("天"); // heaven
	ChakraCost = 50.f;
	Cooldown = 20.f;
	WindUp = 0.5f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Amaterasu);
	ActivationOwnedTags.AddTag(ShinobiTags::State_Casting_Amaterasu); // hand orb (UShinobiCastFxComponent)
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
		ShinobiCombat::ApplyStatus(GetAbilitySystemComponentFromActorInfo(), Target.Get(), UGE_BlackFlame::StaticClass(), UGE_BlackFlame::DamagePerTick);
	}
	EndJutsu();
}
