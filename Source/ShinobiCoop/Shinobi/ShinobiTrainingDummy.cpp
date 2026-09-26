#include "Shinobi/ShinobiTrainingDummy.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDummyAttack(
		TEXT("Shinobi.DummyAttack"),
		false,
		TEXT("Dev aid until M3's enemies: training dummies punch the nearest player within reach every 2 s for 25 damage; ")
		TEXT("their text turns yellow just before. For trying Substitution, jutsu interrupts and the chakra-charge cancel."));
}

AShinobiTrainingDummy::AShinobiTrainingDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Team = EShinobiTeam::Enemies;
	AutoPossessAI = EAutoPossessAI::Disabled;

	HealthText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthText"));
	HealthText->SetupAttachment(RootComponent);
	HealthText->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HealthText->SetHorizontalAlignment(EHTA_Center);
	HealthText->SetWorldSize(28.f);
	HealthText->SetTextRenderColor(FColor::Red);

	UseMannequinMesh();

	// Attributes is constructed by AShinobiCharacterBase's constructor, which has already run at this
	// point, so its defaults are readable here. Avoids showing the widget's default "Text" for a frame.
	DisplayedHealth = FMath::RoundToInt(Attributes->GetHealth());
	DisplayedMaxHealth = FMath::RoundToInt(Attributes->GetMaxHealth());
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d"), DisplayedHealth, DisplayedMaxHealth)));
}

void AShinobiTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The clock runs on every machine so clients see the telegraph too (PIE windows share the cvar); only the server punches.
	PunchClock = CVarDummyAttack.GetValueOnGameThread() ? PunchClock + DeltaSeconds : 0.f;
	if (PunchClock >= PunchInterval)
	{
		PunchClock = 0.f;
		if (HasAuthority())
		{
			Punch();
		}
	}
	const bool bTelegraph = PunchClock >= PunchInterval - PunchTelegraph;
	if (bTelegraph != bDisplayedTelegraph)
	{
		bDisplayedTelegraph = bTelegraph;
		HealthText->SetTextRenderColor(bTelegraph ? FColor::Yellow : FColor::Red);
		if (bTelegraph && HasAuthority())
		{
			LockPunchTarget();
		}
	}

	RefreshText();

	// TextRender geometry lies flat facing its local +X axis, so it's mirrored/culled when viewed from
	// behind. Face the local camera (yaw only) on machines that have one. GetFirstPlayerController()
	// would return any PC in the world -- on a listen server that includes remote clients' PCs, which
	// all have a camera manager but aren't local -- so use the game instance's local-player lookup
	// instead, which is null on a dedicated server and in the test world.
	const UGameInstance* GI = GetGameInstance();
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	const APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager : nullptr;
	if (CameraManager)
	{
		const FVector CameraLocation = CameraManager->GetCameraLocation();
		const FVector TextLocation = HealthText->GetComponentLocation();
		HealthText->SetWorldRotation(FRotator(0.f, (CameraLocation - TextLocation).Rotation().Yaw, 0.f));
	}
}

void AShinobiTrainingDummy::RefreshText()
{
	const int32 CurrentHealth = FMath::RoundToInt(Attributes->GetHealth());
	const int32 CurrentMaxHealth = FMath::RoundToInt(Attributes->GetMaxHealth());
	// ponytail: status text until M2b's cues show them on the body
	FString Statuses;
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Burning_Normal))
	{
		Statuses += TEXT(" [Burn]");
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Burning_Black))
	{
		Statuses += TEXT(" [Black Flame]");
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Shocked))
	{
		Statuses += TEXT(" [Shock]");
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_ShockBonus))
	{
		Statuses += TEXT(" [+25%]");
	}
	if (CurrentHealth != DisplayedHealth || CurrentMaxHealth != DisplayedMaxHealth || Statuses != DisplayedStatuses)
	{
		DisplayedHealth = CurrentHealth;
		DisplayedMaxHealth = CurrentMaxHealth;
		DisplayedStatuses = Statuses;
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d%s"), DisplayedHealth, DisplayedMaxHealth, *DisplayedStatuses)));
	}
}

void AShinobiTrainingDummy::LockPunchTarget()
{
	AShinobiCharacterBase* Nearest = nullptr;
	float NearestDistance = PunchReach;
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (IsHostileTo(*It) && Distance <= NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = *It;
		}
	}
	PunchTarget = Nearest;
	if (Nearest)
	{
		SetActorRotation(FRotator(0.f, (Nearest->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
	}
}

void AShinobiTrainingDummy::Punch()
{
	// Committed at the telegraph: a dodge that carries the target out of reach still "meets" the punch while
	// invulnerable, so a well-timed dodge triggers Substitution instead of just escaping it.
	const AShinobiCharacterBase* Target = PunchTarget.Get();
	PunchTarget = nullptr;
	if (Target && (FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()) <= PunchReach
		|| Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Invulnerable)))
	{
		ShinobiCombat::ApplyDamage(AbilitySystem, Target, PunchDamage);
	}
}

void AShinobiTrainingDummy::HandleOutOfHealth()
{
	Super::HandleOutOfHealth();
	GetWorldTimerManager().SetTimer(ResetTimer, this, &AShinobiTrainingDummy::ResetHealth, 1.5f, false);
}

void AShinobiTrainingDummy::ResetHealth()
{
	Attributes->SetHealth(Attributes->GetMaxHealth());
}
