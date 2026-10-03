#include "Shinobi/Abilities/GA_IceMeteor.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMeteor.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/ShinobiTelegraph.h"
#include "Shinobi/VFX/ShinobiVfx.h"

UGA_IceMeteor::UGA_IceMeteor()
{
	InputID = EShinobiAbilityInputID::Jutsu4;
	Page = 3;
	DisplayName = INVTEXT("Ice Meteors");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_IceMeteor.T_Jutsu_IceMeteor")));
	ChakraCost = 45.f;
	Cooldown = 16.f;
	WindUp = 0.5f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_IceMeteor);
	SealMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_IceMeteor_Seal.AM_IceMeteor_Seal")));
	ReleaseMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_IceMeteor_Release.AM_IceMeteor_Release")));
}

bool UGA_IceMeteor::PrepareJutsu()
{
	return FindAimGround(Range, 30.f, Target);
}

void UGA_IceMeteor::ExecuteJutsu()
{
	FaceAim(nullptr);
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (Self && HasAuthority(&CurrentActivationInfo))
	{
		UWorld* World = Self->GetWorld();
		const float Area = AreaRadius * GetVisualScale();
		AShinobiTelegraph::Spawn(World, Target, Area, Duration + ShardFall, ShinobiVfx::Colors::Ice);

		FShinobiMeteorImpact Impact;
		Impact.Damage = ShardDamage * GetDamageScale();
		Impact.Radius = ShardRadius;
		Impact.Status = UGE_Chill::StaticClass();

		// Evenly spread points (a sunflower spiral, turned by a random amount), landing in a random order.
		const float Turn = FMath::FRandRange(0.f, 2.f * PI);
		TArray<int32> Order;
		for (int32 Index = 0; Index < ShardCount; ++Index)
		{
			Order.Add(Index);
		}
		for (int32 Index = ShardCount - 1; Index > 0; --Index)
		{
			Order.Swap(Index, FMath::RandRange(0, Index));
		}
		for (int32 Index = 0; Index < ShardCount; ++Index)
		{
			const float Angle = Turn + Index * 2.39996323f; // the golden angle
			const float Reach = Area * FMath::Sqrt((Index + 0.5f) / ShardCount);
			const FVector Where = Target + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Reach;
			const float Delay = Duration * Order[Index] / FMath::Max(ShardCount, 1);
			AShinobiMeteor::Spawn(World, Self, EShinobiMeteor::Ice, Where, ShardFall, 1.f, Impact, Delay);
		}
	}
	EndJutsu();
}
