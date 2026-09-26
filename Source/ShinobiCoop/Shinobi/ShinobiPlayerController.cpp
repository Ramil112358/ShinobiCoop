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
#include "Shinobi/ShinobiInputConfig.h"
#include "Shinobi/UI/ShinobiHUDWidget.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "EnhancedInputComponent.h"
#include "Shinobi/VFX/ShinobiBurst.h"

void AShinobiPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	// The main menu left the viewport in UI-only mode with a cursor (input mode outlives the travel).
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
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
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiJutsuShot")))
	{
		RunJutsuShotTest();
	}
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiLookShot")))
	{
		RunLookShotTest();
	}
}

void AShinobiPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(UShinobiInputConfig::Get().Pause, ETriggerEvent::Started, this, &AShinobiPlayerController::TogglePauseMenu);
	}
}

void AShinobiPlayerController::TogglePauseMenu()
{
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
		PauseMenu = nullptr;
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		return;
	}
	PauseMenu = CreateWidget<UShinobiPauseMenuWidget>(this, UShinobiPauseMenuWidget::StaticClass());
	if (!PauseMenu)
	{
		return;
	}
	PauseMenu->AddToViewport(10);
	// Game and UI: the Pause key still reaches the game to close the menu; the world keeps running (online).
	FInputModeGameAndUI Mode; // the menu focuses its first button itself
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
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

void AShinobiPlayerController::ShinobiCast(int32 JutsuSlot)
{
	AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>();
	if (!Shinobi || JutsuSlot < 1 || JutsuSlot > 4)
	{
		return;
	}
	const int32 Input = static_cast<int32>(EShinobiAbilityInputID::Jutsu1) + JutsuSlot - 1;
	Shinobi->GetAbilitySystemComponent()->AbilityLocalInputPressed(Input);
	Shinobi->GetAbilitySystemComponent()->AbilityLocalInputReleased(Input);
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
		UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiStats %s local=%d H=%.1f C=%.1f S=%.1f Burn=%d"),
			*It->GetName(), It->IsLocallyControlled() ? 1 : 0, Stats->GetHealth(), Stats->GetChakra(), Stats->GetStamina(),
			It->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Burning) ? 1 : 0);
	}
}

void AShinobiPlayerController::RunSmokeTest()
{
	// ponytail: fixed delays assume a local connection; raise them if the smoke test flakes on a slow machine
	FTimerHandle DamageTimer;
	GetWorldTimerManager().SetTimer(DamageTimer, FTimerDelegate::CreateUObject(this, &AShinobiPlayerController::ShinobiDamageSelf, 50.f), 5.f, false);
	// The dummy row sits on lower ground than the PlayerStart, below the boom camera's level-aim height at that
	// range; pitch down enough that the crosshair (used by ShinobiCast's Fireball) lands on the dummy instead of
	// sailing over its head. Yaw is left alone -- the spawn rotation already faces the dummies. Set after any
	// restart-time ClientSetRotation (not at BeginPlay, which an in-flight one could still clobber), but measured to
	// need a few seconds of margin before the cast: this control rotation only reaches the server's copy of this
	// pawn through the character movement component's own replication, whose cadence for a pawn with no movement
	// input is not tight enough to land inside the Fireball's 0.4 s wind-up if set right before ShinobiCast -- that
	// arrangement passed 1 of 3 local runs.
	FTimerHandle PitchTimer;
	GetWorldTimerManager().SetTimer(PitchTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		SetControlRotation(FRotator(-8.f, GetControlRotation().Yaw, 0.f));
	}), 1.f, false);
	FTimerHandle CastTimer;
	GetWorldTimerManager().SetTimer(CastTimer, FTimerDelegate::CreateUObject(this, &AShinobiPlayerController::ShinobiCast, 1), 5.5f, false);
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
	auto Input = [this](EShinobiAbilityInputID InputID, bool bPressed)
	{
		if (AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>())
		{
			const int32 ID = static_cast<int32>(InputID);
			bPressed ? Shinobi->GetAbilitySystemComponent()->AbilityLocalInputPressed(ID)
			         : Shinobi->GetAbilitySystemComponent()->AbilityLocalInputReleased(ID);
		}
	};
	auto Shot = [](const TCHAR* Name, bool bShowUI)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), bShowUI, false);
	};
	At(4.f, [this]()
	{
		if (const APawn* Shinobi = GetPawn())
		{
			SetControlRotation(FRotator(-10.f, Shinobi->GetActorRotation().Yaw + 180.f, 0.f)); // camera in front, looking back
		}
	});
	At(5.f, [Shot]() { Shot(TEXT("ShinobiShot_0_Idle"), false); });
	At(5.5f, [Input]() { Input(EShinobiAbilityInputID::ChakraCharge, true); });
	At(6.f, [Shot]() { Shot(TEXT("ShinobiShot_1_Charge"), false); });
	At(7.f, [Shot]() { Shot(TEXT("ShinobiShot_2_Charge"), false); });
	At(8.f, [Shot]() { Shot(TEXT("ShinobiShot_3_Charge"), false); });
	At(8.2f, [Input]() { Input(EShinobiAbilityInputID::ChakraCharge, false); });
	At(8.8f, [Shot]() { Shot(TEXT("ShinobiShot_4_Released"), false); });
	// HUD: throw a Fireball (slot 1 goes on cooldown), then retry at once so slot 1 flashes red.
	At(9.f, [Input]() { Input(EShinobiAbilityInputID::Jutsu1, true); Input(EShinobiAbilityInputID::Jutsu1, false); });
	At(9.6f, [Input]() { Input(EShinobiAbilityInputID::Jutsu1, true); Input(EShinobiAbilityInputID::Jutsu1, false); });
	At(9.7f, [Shot]() { Shot(TEXT("ShinobiShot_5_Hud"), true); });
	At(10.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}

void AShinobiPlayerController::RunLookShotTest()
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Shot = [](const TCHAR* Name, bool bShowUI)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), bShowUI, false);
	};
	auto Look = [this](float Pitch, float YawOffset)
	{
		if (const APawn* Shinobi = GetPawn())
		{
			SetControlRotation(FRotator(Pitch, Shinobi->GetActorRotation().Yaw + YawOffset, 0.f));
		}
	};
	auto FaceNearestEnemy = [this]()
	{
		const AShinobiCharacterBase* Self = GetPawn<AShinobiCharacterBase>();
		const AShinobiCharacterBase* Nearest = nullptr;
		for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); Self && It; ++It)
		{
			if (Self->IsHostileTo(*It) && It->IsAlive() && (!Nearest || FVector::Dist(It->GetActorLocation(), Self->GetActorLocation()) < FVector::Dist(Nearest->GetActorLocation(), Self->GetActorLocation())))
			{
				Nearest = *It;
			}
		}
		if (Nearest)
		{
			SetControlRotation(FRotator(-12.f, (Nearest->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f));
		}
	};
	At(4.f, [Look]() { Look(-8.f, 0.f); });
	At(5.f, [Shot]() { Shot(TEXT("ShinobiLookShot_0_Forward"), true); });
	At(5.5f, [Look]() { Look(-10.f, 180.f); });
	At(6.5f, [Shot]() { Shot(TEXT("ShinobiLookShot_1_Character"), false); });
	At(7.f, [Look]() { Look(-40.f, 90.f); });
	At(8.f, [Shot]() { Shot(TEXT("ShinobiLookShot_2_Overview"), false); });
	At(13.f, FaceNearestEnemy);
	At(13.5f, [Shot]() { Shot(TEXT("ShinobiLookShot_3_WaveIncoming"), true); });
	At(15.5f, FaceNearestEnemy);
	At(15.6f, [this]() { ShinobiCast(1); });
	At(16.3f, [Shot]() { Shot(TEXT("ShinobiLookShot_4_Fight"), true); });
	At(17.5f, [Shot]() { Shot(TEXT("ShinobiLookShot_5_Fight"), true); });
	At(18.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}

void AShinobiPlayerController::RunJutsuShotTest()
{
	// ponytail: fixed timeline against the training dummy straight ahead of the PlayerStart; retune if the map changes
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Shot = [](const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), false, false);
	};
	auto CastJutsu = [this](int32 JutsuSlot)
	{
		ShinobiDrainChakra(-100.f); // refill first, so every jutsu in the sequence is affordable
		ShinobiCast(JutsuSlot);
	};
	At(2.f, [this]() { SetControlRotation(FRotator(-8.f, GetControlRotation().Yaw, 0.f)); }); // the dummies stand lower
	At(3.f, [CastJutsu]() { CastJutsu(1); }); // Fireball
	At(3.25f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_01_FireballWindUp")); });
	At(3.55f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_03_FireballImpact")); });
	At(4.6f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_04_Burn")); });
	At(5.f, [CastJutsu]() { CastJutsu(4); }); // Amaterasu
	At(6.f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_05_BlackFlame")); });
	At(6.5f, [CastJutsu]() { CastJutsu(2); }); // Chidori
	At(6.9f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_06_ChidoriWindUp")); });
	At(7.35f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_07_ChidoriHit")); });
	At(9.f, [CastJutsu]() { CastJutsu(3); }); // Rasengan; the black flame is still burning, so Firestorm goes off
	At(9.5f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_08_RasenganWindUp")); });
	At(9.95f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_09_Firestorm")); });
	At(10.5f, [this]()
	{
		if (const APawn* Shinobi = GetPawn())
		{
			AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::Substitution, Shinobi->GetActorLocation() + Shinobi->GetActorForwardVector() * 250.f);
			AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::Fire, Shinobi->GetActorLocation() + Shinobi->GetActorForwardVector() * 250.f + Shinobi->GetActorRightVector() * 250.f);
		}
	});
	At(10.7f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_10_Substitution")); });
	At(11.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}