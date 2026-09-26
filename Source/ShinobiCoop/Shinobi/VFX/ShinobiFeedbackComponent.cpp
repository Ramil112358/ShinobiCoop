#include "Shinobi/VFX/ShinobiFeedbackComponent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/VFX/ShinobiSfx.h"

namespace
{
	TSubclassOf<UCameraShakeBase> LoadShake(const TCHAR* Path)
	{
		static TMap<FString, TSubclassOf<UCameraShakeBase>> Cache;
		if (const TSubclassOf<UCameraShakeBase>* Found = Cache.Find(Path))
		{
			return *Found;
		}
		UClass* Class = LoadClass<UCameraShakeBase>(nullptr, Path);
		if (Class)
		{
			Class->AddToRoot();
		}
		return Cache.Add(Path, Class);
	}

	APlayerController* LocalController(const AActor* Actor)
	{
		const UGameInstance* GI = Actor ? Actor->GetGameInstance() : nullptr;
		return GI ? GI->GetFirstLocalPlayerController(Actor->GetWorld()) : nullptr;
	}
}

AShinobiDamageNumber::AShinobiDamageNumber()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);
	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Text;
	InitialLifeSpan = Lifetime;
}

void AShinobiDamageNumber::Init(int32 Amount, bool bOnPlayer)
{
	BaseSize = Amount >= UShinobiFeedbackComponent::HeavyHit ? 48.f : 32.f;
	Text->SetWorldSize(BaseSize);
	Text->SetTextRenderColor(bOnPlayer ? FColor(255, 70, 50) : (Amount >= UShinobiFeedbackComponent::HeavyHit ? FColor(255, 220, 90) : FColor::White));
	Text->SetText(FText::AsNumber(Amount));
}

void AShinobiDamageNumber::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	AddActorWorldOffset(FVector(0.f, 0.f, 120.f * DeltaSeconds));
	const float T = Age / Lifetime;
	Text->SetWorldSize(BaseSize * (T < 0.15f ? FMath::Lerp(1.6f, 1.f, T / 0.15f) : FMath::Lerp(1.f, 0.2f, FMath::Max(0.f, T - 0.6f) / 0.4f)));
	if (const APlayerController* PC = LocalController(this); PC && PC->PlayerCameraManager)
	{
		SetActorRotation(FRotator(0.f, (PC->PlayerCameraManager->GetCameraLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
	}
}

UShinobiFeedbackComponent::UShinobiFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UShinobiFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	AShinobiCharacterBase* Owner = Cast<AShinobiCharacterBase>(GetOwner());
	if (!Owner || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	ReadyTime = GetWorld()->GetTimeSeconds() + SettleSeconds;
	Owner->GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(UShinobiAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UShinobiFeedbackComponent::OnHealthChanged);
	if (UAnimInstance* Anim = Owner->GetMesh()->GetAnimInstance())
	{
		Anim->OnMontageStarted.AddDynamic(this, &UShinobiFeedbackComponent::OnMontageStarted);
	}
}

void UShinobiFeedbackComponent::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// Replication can merge several hits into one change; that shows as one bigger number, which is fine.
	if (Data.NewValue < Data.OldValue && GetWorld()->GetTimeSeconds() >= ReadyTime)
	{
		OnDamaged(Data.OldValue - Data.NewValue);
	}
}

void UShinobiFeedbackComponent::OnDamaged(float Amount)
{
	AShinobiCharacterBase* Owner = CastChecked<AShinobiCharacterBase>(GetOwner());
	const int32 Shown = FMath::RoundToInt(Amount);
	if (Shown <= 0)
	{
		return;
	}
	const bool bOnPlayer = Owner->GetTeam() == EShinobiTeam::Players;
	const FVector Head = Owner->GetActorLocation() + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 100.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AShinobiDamageNumber* Number = GetWorld()->SpawnActor<AShinobiDamageNumber>(Head, FRotator::ZeroRotator, Params))
	{
		Number->Init(Shown, bOnPlayer);
	}
	const bool bHeavy = Amount >= HeavyHit;
	ShinobiSfx::Play(Owner, EShinobiSfx::Impact, Owner->GetActorLocation(), bHeavy ? 1.f : 0.5f);
	if (!bHeavy)
	{
		return;
	}

	// Hit-stop: freeze the victim's animation for a beat. Anim rate only: movement, timers and abilities are untouched.
	Owner->GetMesh()->GlobalAnimRateScale = 0.f;
	GetWorld()->GetTimerManager().SetTimer(HitStopTimer, this, &UShinobiFeedbackComponent::EndHitStop, HitStopSeconds, false);

	// Camera shake: hard when the local player is hit, light when an enemy right next to them is.
	APlayerController* PC = LocalController(Owner);
	const APawn* LocalPawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !PC->PlayerCameraManager || !LocalPawn)
	{
		return;
	}
	if (LocalPawn == Owner)
	{
		PC->PlayerCameraManager->StartCameraShake(LoadShake(TEXT("/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Player.BP_CameraShake_Hit_Player_C")),
			FMath::Clamp(Amount / 40.f, 0.5f, 1.5f));
	}
	else if (Owner->IsHostileTo(Cast<AShinobiCharacterBase>(LocalPawn)) && FVector::Dist(LocalPawn->GetActorLocation(), Owner->GetActorLocation()) < 400.f)
	{
		PC->PlayerCameraManager->StartCameraShake(LoadShake(TEXT("/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Enemy.BP_CameraShake_Hit_Enemy_C")), 0.6f);
	}
}

void UShinobiFeedbackComponent::EndHitStop()
{
	if (const AShinobiCharacterBase* Owner = Cast<AShinobiCharacterBase>(GetOwner()))
	{
		Owner->GetMesh()->GlobalAnimRateScale = 1.f;
	}
}

void UShinobiFeedbackComponent::OnMontageStarted(UAnimMontage* Montage)
{
	const FString Name = Montage ? Montage->GetName() : FString();
	const FVector Where = GetOwner()->GetActorLocation();
	if (Name.StartsWith(TEXT("AM_Melee")))
	{
		ShinobiSfx::Play(this, EShinobiSfx::Whoosh, Where, 0.6f);
	}
	else if (Name == TEXT("AM_Dodge"))
	{
		ShinobiSfx::Play(this, EShinobiSfx::Swish, Where, 0.6f);
	}
	else if (Name == TEXT("AM_Downed"))
	{
		ShinobiSfx::Play(this, EShinobiSfx::Thud, Where);
	}
	else if (Name == TEXT("AM_Fireball") || Name == TEXT("AM_Chidori") || Name == TEXT("AM_Rasengan") || Name == TEXT("AM_Amaterasu"))
	{
		ShinobiSfx::Play(this, EShinobiSfx::Cast, Where, 0.5f);
	}
}
