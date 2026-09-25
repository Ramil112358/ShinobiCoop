#include "Shinobi/ShinobiTrainingDummy.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiAttributeSet.h"

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
}

void AShinobiTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ponytail: per-frame poll with change detection; switch to an attribute-change delegate if dummies multiply
	const int32 CurrentHealth = FMath::RoundToInt(Attributes->GetHealth());
	const int32 CurrentMaxHealth = FMath::RoundToInt(Attributes->GetMaxHealth());
	if (CurrentHealth != DisplayedHealth || CurrentMaxHealth != DisplayedMaxHealth)
	{
		DisplayedHealth = CurrentHealth;
		DisplayedMaxHealth = CurrentMaxHealth;
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d"), DisplayedHealth, DisplayedMaxHealth)));
	}

	// TextRender geometry lies flat facing its local +X axis, so it's mirrored/culled when viewed from
	// behind. Face the local camera (yaw only) on machines that have one; skip on dedicated servers and
	// in the test world, where there is no local player.
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager : nullptr;
	if (CameraManager)
	{
		const FVector CameraLocation = CameraManager->GetCameraLocation();
		const FVector TextLocation = HealthText->GetComponentLocation();
		HealthText->SetWorldRotation(FRotator(0.f, (CameraLocation - TextLocation).Rotation().Yaw, 0.f));
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
