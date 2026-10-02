#include "Shinobi/Abilities/GA_WaterWall.h"
#include "Shinobi/ShinobiBarrier.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"

UGA_WaterWall::UGA_WaterWall()
{
	InputID = EShinobiAbilityInputID::Jutsu1;
	Page = 3;
	DisplayName = INVTEXT("Water Wall");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_WaterWall.T_Jutsu_WaterWall")));
	ChakraCost = 25.f;
	Cooldown = 9.f;
	WindUp = 0.4f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_WaterWall);
}

void UGA_WaterWall::ExecuteJutsu()
{
	FaceAim(nullptr);
	if (HasAuthority(&CurrentActivationInfo))
	{
		AShinobiBarrier::SpawnInFront(GetShinobiCharacter(), EShinobiBarrierKind::Water);
	}
	EndJutsu();
}
