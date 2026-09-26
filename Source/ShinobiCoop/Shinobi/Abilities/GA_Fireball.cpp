#include "Shinobi/Abilities/GA_Fireball.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiProjectile.h"
#include "Shinobi/ShinobiTags.h"

UGA_Fireball::UGA_Fireball()
{
	InputID = EShinobiAbilityInputID::Jutsu1;
	DisplayName = INVTEXT("Fireball");
	Glyph = INVTEXT("火"); // fire
	ChakraCost = 20.f;
	Cooldown = 4.f;
	WindUp = 0.4f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Fireball);
	ActivationOwnedTags.AddTag(ShinobiTags::State_Casting_Fireball); // hand orb (UShinobiCastFxComponent)
	CastMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Fireball.AM_Fireball")));
}

void UGA_Fireball::ExecuteJutsu()
{
	FaceAim(nullptr); // the camera may have turned during the wind-up
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (Self && HasAuthority(&CurrentActivationInfo))
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetAimViewPoint(ViewLocation, ViewRotation);
		FHitResult Hit;
		const FVector AimPoint = TraceAim(AimRange, 10.f, Hit) ? Hit.ImpactPoint : ViewLocation + ViewRotation.Vector() * AimRange;

		// ponytail: fixed chest-height launch point in front of the caster; a hand socket arrives with M2b's animations
		const FVector Muzzle = Self->GetActorLocation() + Self->GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);
		const FTransform Launch((AimPoint - Muzzle).Rotation(), Muzzle);
		if (AShinobiProjectile* Ball = Self->GetWorld()->SpawnActorDeferred<AShinobiProjectile>(AShinobiProjectile::StaticClass(), Launch, Self, Self, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Ball->DirectDamage = DirectDamage;
			Ball->SplashDamage = SplashDamage;
			Ball->SplashRadius = SplashRadius;
			Ball->StatusEffect = UGE_Burn::StaticClass();
			Ball->StatusDamagePerTick = UGE_Burn::DamagePerTick;
			Ball->FinishSpawning(Launch);
		}
	}
	EndJutsu();
}
