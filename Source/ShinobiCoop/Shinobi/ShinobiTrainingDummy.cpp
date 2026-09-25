#include "Shinobi/ShinobiTrainingDummy.h"
#include "Components/TextRenderComponent.h"
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
	// ponytail: per-frame text refresh; switch to an attribute-change delegate if dummies multiply
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %.0f / %.0f"), Attributes->GetHealth(), Attributes->GetMaxHealth())));
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
