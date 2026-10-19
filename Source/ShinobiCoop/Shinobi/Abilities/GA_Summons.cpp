#include "Shinobi/Abilities/GA_Summons.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Enemies/ShinobiOwl.h"

UGA_SummonBase::UGA_SummonBase()
{
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

UGA_SummonOwl::UGA_SummonOwl()
{
	DisplayName = INVTEXT("Summon Owl");
	Description = INVTEXT("A snowy owl perches on your shoulder and calls lightning down on enemies.");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonOwl.T_Jutsu_SummonOwl")));
	ChakraCost = 60.f;
	Cooldown = 45.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonOwl);
}

void UGA_SummonOwl::ExecuteJutsu()
{
	FaceAim(nullptr);
	if (HasAuthority(&CurrentActivationInfo))
	{
		AShinobiOwl::Summon(GetShinobiCharacter());
	}
	EndJutsu();
}

UGA_SummonMisha::UGA_SummonMisha()
{
	DisplayName = INVTEXT("Summon Misha");
	Description = INVTEXT("Misha, a giant plush puppy, barks a shockwave from a distance.");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonMisha.T_Jutsu_SummonMisha")));
	ChakraCost = 50.f;
	Cooldown = 40.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonMisha);
	SummonKind = EShinobiSummonKind::Misha;
	Distance = 500.f; // he is a big plush: room for him clear of the caster
	Clearance = 350.f;
}

UGA_SummonPabu::UGA_SummonPabu()
{
	DisplayName = INVTEXT("Summon Pabu");
	Description = INVTEXT("Pabu the corgi ninken fights at your side.");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonPabu.T_Jutsu_SummonPabu")));
	ChakraCost = 45.f;
	Cooldown = 30.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonPabu);
	SummonKind = EShinobiSummonKind::Pabu;
	Distance = 350.f;
	Clearance = 250.f;
}

UGA_SummonPusho::UGA_SummonPusho()
{
	DisplayName = INVTEXT("Summon Pusho");
	Description = INVTEXT("Pusho the long-haired cat pounces and claws at your foes.");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_SummonPusho.T_Jutsu_SummonPusho")));
	ChakraCost = 45.f;
	Cooldown = 35.f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_SummonPusho);
	SummonKind = EShinobiSummonKind::Pusho;
	Distance = 350.f;
	Clearance = 250.f;
}

bool UGA_SummonBase::IsSlotLocked(const UAbilitySystemComponent& ASC, const FGameplayAbilitySpec& Spec) const
{
	if (Super::IsSlotLocked(ASC, Spec))
	{
		return true;
	}
	const AActor* Caster = ASC.GetAvatarActor();
	const UWorld* World = Caster ? Caster->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AShinobiSummon> It(World); It; ++It)
	{
		if (It->GetSummoner() == Caster && It->GetKind() == SummonKind && It->IsAlive())
		{
			return true; // the summon is still out
		}
	}
	return false;
}

bool UGA_SummonOwl::IsSlotLocked(const UAbilitySystemComponent& ASC, const FGameplayAbilitySpec& Spec) const
{
	if (UGA_SummonBase::IsSlotLocked(ASC, Spec))
	{
		return true;
	}
	const AActor* Caster = ASC.GetAvatarActor();
	const UWorld* World = Caster ? Caster->GetWorld() : nullptr;
	for (TActorIterator<AShinobiOwl> It(World); World && It; ++It)
	{
		if (It->GetSummoner() == Caster && !It->IsActorBeingDestroyed())
		{
			return true;
		}
	}
	return false;
}
