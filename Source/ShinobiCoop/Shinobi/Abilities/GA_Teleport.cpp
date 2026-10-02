#include "Shinobi/Abilities/GA_Teleport.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMarkerKunai.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiBurst.h"

UGA_Teleport::UGA_Teleport()
{
	InputID = EShinobiAbilityInputID::Jutsu1;
	Page = 2;
	DisplayName = INVTEXT("Teleport");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Shinobi/UI/T_Jutsu_Teleport.T_Jutsu_Teleport")));
	ChakraCost = 25.f;
	Cooldown = 10.f;
	WindUp = 0.4f;
	CooldownTags.AddTag(ShinobiTags::Cooldown_Jutsu_Teleport);
	ActivationBlockedTags.AddTag(ShinobiTags::State_TeleportMarked); // the key belongs to UGA_TeleportJump while a window is open
}

void UGA_Teleport::ExecuteJutsu()
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
		const FVector Muzzle = Self->GetActorLocation() + Self->GetActorForwardVector() * 40.f + FVector(0.f, 0.f, 30.f);
		const FRotator Middle = (AimPoint - Muzzle).Rotation();
		for (const float Offset : { -FanDegrees, 0.f, FanDegrees })
		{
			AShinobiMarkerKunai::Spawn(Self->GetWorld(), Self, Muzzle, Middle + FRotator(0.f, Offset, 0.f));
		}
		ApplyEffectToSelf(UGE_TeleportMarked::StaticClass());
	}
	EndJutsu();
}

UGA_TeleportJump::UGA_TeleportJump()
{
	InputID = EShinobiAbilityInputID::Jutsu1;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationRequiredTags.AddTag(ShinobiTags::State_TeleportMarked);
	ActivationRequiredTags.AddTag(ShinobiTags::JutsuPage(2));
}

void UGA_TeleportJump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self || !HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetAimViewPoint(ViewLocation, ViewRotation);
	const FVector Aim = ViewRotation.Vector();
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(PickDegrees));
	float BestCos = MinCos;
	AShinobiMarkerKunai* NearestToCrosshair = nullptr;
	AShinobiMarkerKunai* Oldest = nullptr;
	int32 Owned = 0;
	for (TActorIterator<AShinobiMarkerKunai> It(Self->GetWorld()); It; ++It)
	{
		if (It->GetOwner() != Self)
		{
			continue;
		}
		++Owned;
		if (!It->IsStuck())
		{
			continue;
		}
		if (!Oldest || It->GetThrownAt() < Oldest->GetThrownAt())
		{
			Oldest = *It;
		}
		const float Cos = FVector::DotProduct(Aim, (It->GetActorLocation() - ViewLocation).GetSafeNormal());
		if (Cos >= BestCos)
		{
			BestCos = Cos;
			NearestToCrosshair = *It;
		}
	}
	AShinobiMarkerKunai* Chosen = NearestToCrosshair ? NearestToCrosshair : Oldest;

	if (!Chosen)
	{
		if (Owned == 0)
		{
			// Every kunai missed or timed out: the window has nothing in it.
			GetAbilitySystemComponentFromActorInfo()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_TeleportMarked));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const UCapsuleComponent* Capsule = Self->GetCapsuleComponent();
	const FVector Arrival = Chosen->GetArrivalPoint(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	// TeleportTo stays put if something blocks the spot; the kunai is then kept for another try.
	if (Self->TeleportTo(Arrival, FRotator(0.f, ViewRotation.Yaw, 0.f)))
	{
		if (UShinobiMovementComponent* Movement = GetShinobiMovement())
		{
			Movement->ForceRotationCorrectionFor(1.f); // the owning client is turned too
		}
		UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
		for (AShinobiCharacterBase* Victim : ShinobiCombat::FindHostilesInRadius(Self, Arrival, ArrivalRadius))
		{
			if (ShinobiCombat::ApplyDamage(Source, Victim, ArrivalDamage))
			{
				ShinobiCombat::ApplyStatus(Source, Victim, UGE_Stagger::StaticClass());
			}
		}
		AShinobiBurst::Spawn(Self->GetWorld(), EShinobiBurst::TeleportFlash, Arrival);
		Chosen->Destroy();
		if (Owned <= 1)
		{
			Source->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_TeleportMarked));
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
