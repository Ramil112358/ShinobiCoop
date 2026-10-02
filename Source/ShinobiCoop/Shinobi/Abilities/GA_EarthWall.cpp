#include "Shinobi/Abilities/GA_EarthWall.h"
#include "Shinobi/ShinobiBarrier.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"

UGA_EarthWall::UGA_EarthWall()
{
	InputID = EShinobiAbilityInputID::Jutsu4;
	Page = 2;
	DisplayName = INVTEXT("Earth Wall");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_EarthWall.T_Jutsu_EarthWall")));
	ChakraCost = 35.f;
	Cooldown = 14.f;
	WindUp = 0.5f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_EarthWall);
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_EarthWall_Seal.AM_EarthWall_Seal")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_EarthWall_Release.AM_EarthWall_Release")));
}

void UGA_EarthWall::ExecuteJutsu()
{
	FaceAim(nullptr); // the camera may have turned during the seals
	if (HasAuthority(&CurrentActivationInfo))
	{
		AShinobiBarrier::SpawnInFront(GetShinobiCharacter(), EShinobiBarrierKind::Earth);
	}
	EndJutsu();
}
