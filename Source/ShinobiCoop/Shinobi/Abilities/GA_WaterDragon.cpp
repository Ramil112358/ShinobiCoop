#include "Shinobi/Abilities/GA_WaterDragon.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/ShinobiWaterDragon.h"

UGA_WaterDragon::UGA_WaterDragon()
{
	InputID = EShinobiAbilityInputID::Jutsu2;
	Page = 3;
	DisplayName = INVTEXT("Water Dragon");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_WaterDragon.T_Jutsu_WaterDragon")));
	ChakraCost = 40.f;
	Cooldown = 12.f;
	WindUp = 0.6f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_WaterDragon);
}

void UGA_WaterDragon::ExecuteJutsu()
{
	FaceAim(nullptr); // the camera may have turned during the seals
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (Self && HasAuthority(&CurrentActivationInfo))
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetAimViewPoint(ViewLocation, ViewRotation);
		FHitResult Hit;
		const FVector AimPoint = TraceAim(AimRange, 10.f, Hit) ? Hit.ImpactPoint : ViewLocation + ViewRotation.Vector() * AimRange;
		const FVector Mouth = Self->GetActorLocation() + Self->GetActorForwardVector() * 90.f + FVector(0.f, 0.f, 40.f);
		FShinobiDragonHit Catch;
		Catch.Damage = Damage * GetDamageScale();
		AShinobiWaterDragon::Spawn(Self->GetWorld(), Self, Mouth, AimPoint - Mouth, FlightSpeed, FlightSeconds, GetVisualScale(), Catch);
	}
	EndJutsu();
}
