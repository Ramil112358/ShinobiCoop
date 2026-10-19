#include "Shinobi/Abilities/GA_Fireball.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiProjectile.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Fireball::UGA_Fireball()
{
	DisplayName = INVTEXT("Fireball");
	Description = INVTEXT("A fireball thrown at the crosshair that sets its target burning.");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_Fireball.T_Jutsu_Fireball")));
	ChakraCost = 20.f;
	Cooldown = 4.f;
	WindUp = 0.6f;
	ReleaseDelay = 0.1f; // the ball leaves on the blow, not at the start of the release clip
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Fireball);
	ActivationOwnedTags.AddTag(ShinobiTags::State_Casting_Fireball); // hand orb (UShinobiCastFxComponent)
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Fireball_Seal.AM_Fireball_Seal")));
	ChargeMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Fireball_Charge.AM_Fireball_Charge")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Fireball_Release.AM_Fireball_Release")));
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

		// Breathed out of the mouth: just in front of the head bone (the old chest point if the body has no head bone).
		const USkeletalMeshComponent* Body = Self->GetBodyMesh();
		const FName Head = Self->GetHeadBone();
		const FVector Muzzle = Body->GetBoneIndex(Head) != INDEX_NONE
			? Body->GetSocketLocation(Head) + Self->GetActorForwardVector() * 25.f
			: Self->GetActorLocation() + Self->GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);
		const FTransform Launch((AimPoint - Muzzle).Rotation(), Muzzle);
		AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::FireBreath, Muzzle, GetVisualScale(), Launch.Rotator());
		if (AShinobiProjectile* Ball = Self->GetWorld()->SpawnActorDeferred<AShinobiProjectile>(AShinobiProjectile::StaticClass(), Launch, Self, Self, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Ball->DirectDamage = DirectDamage * GetDamageScale();
			Ball->SplashDamage = SplashDamage * GetDamageScale();
			Ball->SplashRadius = SplashRadius * GetVisualScale();
			Ball->StatusEffect = UGE_Burn::StaticClass();
			Ball->StatusDamagePerTick = UGE_Burn::DamagePerTick * GetDamageScale();
			Ball->Scale = GetVisualScale();
			Ball->FinishSpawning(Launch);
		}
	}
	EndJutsu();
}
