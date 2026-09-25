#include "Shinobi/ShinobiPlayerController.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/UI/ShinobiHUDWidget.h"

void AShinobiPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	if (FApp::CanEverRender())
	{
		Hud = CreateWidget<UShinobiHUDWidget>(this, UShinobiHUDWidget::StaticClass());
		Hud->AddToViewport();
	}
	if (GetNetMode() == NM_Client && FParse::Param(FCommandLine::Get(), TEXT("ShinobiSmoke")))
	{
		RunSmokeTest();
	}
}

void AShinobiPlayerController::ShinobiDamageSelf(float Amount)
{
	ServerDamageSelf(Amount);
}

void AShinobiPlayerController::ServerDamageSelf_Implementation(float Amount)
{
	AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>();
	if (!Shinobi)
	{
		return;
	}
	UAbilitySystemComponent* ASC = Shinobi->GetAbilitySystemComponent();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_Damage::StaticClass(), 1.f, ASC->MakeEffectContext());
	Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Damage, Amount);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void AShinobiPlayerController::ShinobiDumpStats()
{
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const UShinobiAttributeSet* Stats = It->GetAttributes();
		UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiStats %s local=%d H=%.1f C=%.1f S=%.1f"),
			*It->GetName(), It->IsLocallyControlled() ? 1 : 0, Stats->GetHealth(), Stats->GetChakra(), Stats->GetStamina());
	}
}

void AShinobiPlayerController::RunSmokeTest()
{
	// ponytail: fixed delays assume a local connection; raise them if the smoke test flakes on a slow machine
	FTimerHandle DamageTimer;
	GetWorldTimerManager().SetTimer(DamageTimer, FTimerDelegate::CreateUObject(this, &AShinobiPlayerController::ShinobiDamageSelf, 50.f), 5.f, false);
	FTimerHandle DumpTimer;
	GetWorldTimerManager().SetTimer(DumpTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ShinobiDumpStats();
		ConsoleCommand(TEXT("quit"));
	}), 9.f, false);
}
