#include "Shinobi/Abilities/GA_Summons.h"
#include "Engine/World.h"
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
		const FVector Forward = Self->GetActorForwardVector().GetSafeNormal2D();
		const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetSimpleCollisionHalfHeight());
		UWorld* World = Self->GetWorld();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SummonSpot), false, Self);
		FHitResult Hit;
		// Not into a wall: stop short of anything in the way.
		float Ahead = Distance;
		if (World->LineTraceSingleByChannel(Hit, Self->GetActorLocation(), Self->GetActorLocation() + Forward * (Distance + Clearance), ECC_WorldStatic, Params))
		{
			Ahead = FMath::Clamp(Hit.Distance - Clearance, 0.f, Distance);
		}
		// And on the ground there: down from above, so a ledge or a slope puts it on the floor rather than in the air or in the hill.
		FVector Spot = Feet + Forward * Ahead;
		if (World->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 200.f), Spot - FVector(0.f, 0.f, 500.f), ECC_WorldStatic, Params))
		{
			Spot = Hit.ImpactPoint;
		}
		AShinobiSummon::Summon(Self, SummonKind, Spot);
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
	Clearance = 300.f;
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
	Clearance = 350.f;
}

UGA_SummonPabu::UGA_SummonPabu()
{
	InputID = EShinobiAbilityInputID::Jutsu4;
	DisplayName = INVTEXT("Summon Pabu");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonPabu.T_Jutsu_SummonPabu")));
	ChakraCost = 45.f;
	Cooldown = 30.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonPabu);
	SummonKind = EShinobiSummonKind::Pabu;
	Distance = 350.f;
	Clearance = 250.f;
}
