#include "Shinobi/Abilities/GA_Summons.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"

UGA_SummonBase::UGA_SummonBase()
{
	Page = 4;
	WindUp = 0.7f;
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Summon_Seal.AM_Summon_Seal")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Summon_Release.AM_Summon_Release")));
}

void UGA_SummonBase::ExecuteJutsu()
{
	FaceAim(nullptr);
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (Self && HasAuthority(&CurrentActivationInfo))
	{
		const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight());
		AShinobiSummon::Summon(Self, SummonKind, Feet + Self->GetActorForwardVector().GetSafeNormal2D() * Distance);
	}
	EndJutsu();
}

UGA_SummonToad::UGA_SummonToad()
{
	InputID = EShinobiAbilityInputID::Jutsu2;
	DisplayName = INVTEXT("Summon Toad");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonToad.T_Jutsu_SummonToad")));
	ChakraCost = 60.f;
	Cooldown = 45.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonToad);
	SummonKind = EShinobiSummonKind::Toad;
	Distance = 500.f;
}

UGA_SummonSnake::UGA_SummonSnake()
{
	InputID = EShinobiAbilityInputID::Jutsu3;
	DisplayName = INVTEXT("Summon Snake");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonSnake.T_Jutsu_SummonSnake")));
	ChakraCost = 50.f;
	Cooldown = 40.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonSnake);
	SummonKind = EShinobiSummonKind::Snake;
	Distance = 400.f;
}

UGA_SummonDogs::UGA_SummonDogs()
{
	InputID = EShinobiAbilityInputID::Jutsu4;
	DisplayName = INVTEXT("Summon Dogs");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonDogs.T_Jutsu_SummonDogs")));
	ChakraCost = 45.f;
	Cooldown = 30.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonDogs);
	SummonKind = EShinobiSummonKind::Dog;
	Distance = 350.f;
}
