#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Animation/AnimMontage.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiFirePatch.h"

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
	if (!Self)
	{
		return false;
	}
	FHitResult Hit;
	if (TraceAim(Range, AimRadius, Hit))
	{
		AShinobiCharacterBase* Candidate = Cast<AShinobiCharacterBase>(Hit.GetActor());
		if (Self->IsHostileTo(Candidate) && FVector::Dist(Self->GetActorLocation(), Candidate->GetActorLocation()) <= Range)
		{
			Target = Candidate;
			return true;
		}
		AimPoint = Hit.ImpactPoint; // a wall, the ground, a building
		return true;
	}
	// Aimed above everything: burn the ground below the end of the aim, if there is ground within reach.
	FVector ViewLocation;
	FRotator ViewRotation;
	GetAimViewPoint(ViewLocation, ViewRotation);
	const FVector End = Self->GetActorLocation() + ViewRotation.Vector() * Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiAmaterasuGround), false, Self);
	if (Self->GetWorld()->LineTraceSingleByChannel(Hit, End, End - FVector(0.f, 0.f, Range), ECC_WorldStatic, Params))
	{
		AimPoint = Hit.ImpactPoint;
		return true;
	}
	return false;
}

void UGA_Amaterasu::ExecuteJutsu()
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		AShinobiCharacterBase* Victim = Target.Get();
		if (Victim && Victim->IsAlive() && ShinobiCombat::ApplyStatus(GetAbilitySystemComponentFromActorInfo(), Victim, UGE_BlackFlame::StaticClass(), UGE_BlackFlame::DamagePerTick * GetDamageScale()))
		{
			AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::BlackFlameIgnite, Victim->GetActorLocation(), GetVisualScale());
		}
		else
		{
			// No enemy, or it died (or dodged) during the charge: the flames catch on the spot instead.
			const FVector Spot = Victim ? Victim->GetActorLocation() - FVector(0.f, 0.f, Victim->GetSimpleCollisionHalfHeight()) : AimPoint;
			AShinobiFirePatch::Spawn(GetWorld(), EShinobiFirePatch::BlackFlame, Spot, GetShinobiCharacter(), GetVisualScale(), UGE_BlackFlame::DamagePerTick * GetDamageScale());
			AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::BlackFlameIgnite, Spot, GetVisualScale());
		}
	}
	EndJutsu();
}
