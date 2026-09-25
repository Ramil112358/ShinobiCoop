#include "Shinobi/ShinobiPlayerController.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
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
		// CreateWidget returns null if this controller has no Player attached yet.
		Hud = CreateWidget<UShinobiHUDWidget>(this, UShinobiHUDWidget::StaticClass());
		if (Hud)
		{
			Hud->AddToViewport();
		}
	}
	if (GetNetMode() == NM_Client && FParse::Param(FCommandLine::Get(), TEXT("ShinobiSmoke")))
	{
		RunSmokeTest();
	}
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiShot")))
	{
		RunShotTest();
	}
}

void AShinobiPlayerController::ShinobiDamageSelf(float Amount)
{
	ServerDamageSelf(Amount);
}

void AShinobiPlayerController::ServerDamageSelf_Implementation(float Amount)
{
	ApplyToOwnPawn(UGE_Damage::StaticClass(), ShinobiTags::Data_Damage, Amount);
}

void AShinobiPlayerController::ShinobiDrainChakra(float Amount)
{
	ServerDrainChakra(Amount);
}

void AShinobiPlayerController::ServerDrainChakra_Implementation(float Amount)
{
	ApplyToOwnPawn(UGE_ChakraGain::StaticClass(), ShinobiTags::Data_ChakraGain, -Amount);
}

void AShinobiPlayerController::ApplyToOwnPawn(TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Magnitude)
{
	AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>();
	if (!Shinobi)
	{
		return;
	}
	UAbilitySystemComponent* ASC = Shinobi->GetAbilitySystemComponent();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
	Spec.Data->SetSetByCallerMagnitude(DataTag, Magnitude);
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

void AShinobiPlayerController::RunShotTest()
{
	// ponytail: fixed delays; raise them if shaders are still compiling when the first shot lands
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto ChargeInput = [this](bool bPressed)
	{
		if (AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>())
		{
			const int32 Input = static_cast<int32>(EShinobiAbilityInputID::ChakraCharge);
			bPressed ? Shinobi->GetAbilitySystemComponent()->AbilityLocalInputPressed(Input)
			         : Shinobi->GetAbilitySystemComponent()->AbilityLocalInputReleased(Input);
		}
	};
	auto Shot = [](const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), false, false);
	};
	At(4.f, [this]()
	{
		if (const APawn* Shinobi = GetPawn())
		{
			SetControlRotation(FRotator(-10.f, Shinobi->GetActorRotation().Yaw + 180.f, 0.f)); // camera in front, looking back
		}
	});
	At(5.f, [Shot]() { Shot(TEXT("ShinobiShot_0_Idle")); });
	At(5.5f, [ChargeInput]() { ChargeInput(true); });
	At(6.f, [Shot]() { Shot(TEXT("ShinobiShot_1_Charge")); });
	At(7.f, [Shot]() { Shot(TEXT("ShinobiShot_2_Charge")); });
	At(8.f, [Shot]() { Shot(TEXT("ShinobiShot_3_Charge")); });
	At(8.2f, [ChargeInput]() { ChargeInput(false); });
	At(8.8f, [Shot]() { Shot(TEXT("ShinobiShot_4_Released")); });
	At(9.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}
