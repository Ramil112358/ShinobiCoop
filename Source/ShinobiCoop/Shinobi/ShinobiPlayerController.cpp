#include "Shinobi/ShinobiPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "RHI.h"
#include "GameFramework/SpringArmComponent.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiBarrier.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMarkerKunai.h"
#include "Shinobi/ShinobiMeteor.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/Enemies/ShinobiOwl.h"
#include "Shinobi/ShinobiDragonMount.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "Shinobi/ShinobiInputConfig.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiSettings.h"
#include "Shinobi/UI/ShinobiHUDWidget.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "EnhancedInputComponent.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiFirePatch.h"

void AShinobiPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	ShinobiCharacter(ShinobiSettings::GetSavedCharacter());
	ShinobiOutfit(ShinobiSettings::GetSavedOutfit());
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
	if (FParse::Param(FCommandLine::Get(), TEXT("ShinobiFollowTest")))
	{
		RunFollowTest();
	}
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiShot")))
	{
		RunShotTest();
	}
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiJutsuShot")))
	{
		RunJutsuShotTest();
	}
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiFireShot")))
	{
		RunFireShotTest();
	}
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiSusanooShot")))
	{
		RunSusanooShotTest();
	}
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiLookShot")))
	{
		RunLookShotTest();
	}
	if (FApp::CanEverRender() && FParse::Param(FCommandLine::Get(), TEXT("ShinobiWorldTour")))
	{
		RunWorldTourTest();
	}
}

void AShinobiPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(UShinobiInputConfig::Get().Pause, ETriggerEvent::Started, this, &AShinobiPlayerController::TogglePauseMenu);
		Input->BindAction(UShinobiInputConfig::Get().MapToggle, ETriggerEvent::Started, this, &AShinobiPlayerController::ToggleMap);
		Input->BindAction(UShinobiInputConfig::Get().MapZoom, ETriggerEvent::Started, this, &AShinobiPlayerController::OnMapZoom);
	}
}

void AShinobiPlayerController::ToggleMap()
{
	if (Hud)
	{
		Hud->ToggleBigMap();
	}
}

bool AShinobiPlayerController::IsBigMapOpen() const
{
	return Hud && Hud->IsBigMapOpen();
}

void AShinobiPlayerController::ZoomMap(int32 Steps, bool bWrap)
{
	if (Hud)
	{
		Hud->ZoomMap(Steps, bWrap);
	}
}

void AShinobiPlayerController::OnMapZoom(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (!FMath::IsNearlyZero(Axis))
	{
		ZoomMap(Axis > 0.f ? 1 : -1, true);
	}
}

void AShinobiPlayerController::TogglePauseMenu()
{
	if (AShinobiPlayerCharacter* ControlledPawn = GetPawn<AShinobiPlayerCharacter>(); ControlledPawn && ControlledPawn->CancelJutsuTargetPlacement())
	{
		return;
	}
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

void AShinobiPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	AShinobiCharacterBase::RefreshAllOutlines(GetWorld()); // every outline is relative to the pawn we now control
}

void AShinobiPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AShinobiPlayerCharacter* Body = Cast<AShinobiPlayerCharacter>(InPawn))
	{
		Body->SetCharacterIndex(PickedCharacter);
		Body->SetOutfitIndex(PickedOutfit);
	}
}

void AShinobiPlayerController::ShinobiCharacter(const FString& Name)
{
	const int32 Index = AShinobiPlayerCharacter::FindRosterIndex(Name);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("ShinobiCharacter: no roster character named '%s'"), *Name);
		return;
	}
	ServerPickCharacter(uint8(Index));
}

void AShinobiPlayerController::ServerPickCharacter_Implementation(uint8 Index)
{
	PickedCharacter = Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster) ? Index : 0; // a client's value: clamp it
	if (AShinobiPlayerCharacter* Body = GetPawn<AShinobiPlayerCharacter>())
	{
		Body->SetCharacterIndex(PickedCharacter);
	}
}

void AShinobiPlayerController::ShinobiOutfit(const FString& Name)
{
	const int32 Index = AShinobiPlayerCharacter::FindOutfitIndex(Name);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("ShinobiOutfit: no outfit named '%s'"), *Name);
		return;
	}
	ServerPickOutfit(uint8(Index));
}

void AShinobiPlayerController::ServerPickOutfit_Implementation(uint8 Index)
{
	PickedOutfit = Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits) ? Index : 0; // a client's value: clamp it
	if (AShinobiPlayerCharacter* Body = GetPawn<AShinobiPlayerCharacter>())
	{
		Body->SetOutfitIndex(PickedOutfit);
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

void AShinobiPlayerController::ShinobiPage(int32 Page)
{
	if (AShinobiPlayerCharacter* Shinobi = GetPawn<AShinobiPlayerCharacter>())
	{
		Shinobi->SetJutsuPage(Page);
	}
}

void AShinobiPlayerController::ShinobiCastHold(int32 JutsuSlot, float Seconds)
{
	AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>();
	if (!Shinobi || JutsuSlot < 1 || JutsuSlot > 4)
	{
		return;
	}
	const int32 Input = static_cast<int32>(EShinobiAbilityInputID::Jutsu1) + JutsuSlot - 1;
	Shinobi->GetAbilitySystemComponent()->AbilityLocalInputPressed(Input);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Input]()
	{
		if (const AShinobiCharacterBase* Held = GetPawn<AShinobiCharacterBase>())
		{
			Held->GetAbilitySystemComponent()->AbilityLocalInputReleased(Input);
		}
	}), FMath::Max(Seconds, 0.01f), false);
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
		UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiStats %s local=%d H=%.1f C=%.1f S=%.1f Burn=%d Phased=%d Susanoo=%d Mounted=%d"),
			*It->GetName(), It->IsLocallyControlled() ? 1 : 0, Stats->GetHealth(), Stats->GetChakra(), Stats->GetStamina(),
			It->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Burning) ? 1 : 0,
			It->IsPhased() ? 1 : 0,
			It->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Susanoo) ? 1 : 0,
			It->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Mounted) ? 1 : 0);
	}
	// What the replicated jutsu actors look like from this machine (the co-op check compares both sides).
	int32 Barriers = 0, Kunai = 0, Meteors = 0, Summons = 0, Dragons = 0;
	for (TActorIterator<AShinobiBarrier> It(GetWorld()); It; ++It)
	{
		++Barriers;
	}
	for (TActorIterator<AShinobiMarkerKunai> It(GetWorld()); It; ++It)
	{
		++Kunai;
	}
	for (TActorIterator<AShinobiMeteor> It(GetWorld()); It; ++It)
	{
		++Meteors;
	}
	for (TActorIterator<AShinobiSummon> It(GetWorld()); It; ++It)
	{
		Summons += It->IsAlive() ? 1 : 0;
	}
	for (TActorIterator<AShinobiDragonMount> It(GetWorld()); It; ++It)
	{
		++Dragons;
	}
	const AShinobiPlayerCharacter* Me = GetPawn<AShinobiPlayerCharacter>();
	UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiJutsuWorld Page=%d Barriers=%d Kunai=%d Meteors=%d Summons=%d Dragons=%d"), Me ? Me->GetJutsuPage() : 0, Barriers, Kunai, Meteors, Summons, Dragons);
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
	// The movement/defense jutsu over the wire: page 2, an Earth Wall (a replicated actor), then Kamui (a replicated tag).
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	At(6.3f, [this]() { ShinobiPage(4); });
	At(6.5f, [this]() { ShinobiCast(4); }); // Pabu: appears from about 7.2 s
	At(7.4f, [this]() { ShinobiDrainChakra(-100.f); });
	At(7.6f, [this]() { ShinobiCast(1); }); // Susanoo: the armour is up from about 8.3 s for 20 s
	At(8.4f, [this]() { ShinobiDrainChakra(-100.f); ShinobiPage(3); });
	At(8.6f, [this]() { ShinobiCast(4); }); // Ice Meteors: the shards are queued from about 9.15 s and land over the next 4.5 s
	At(9.2f, [this]() { ShinobiDrainChakra(-100.f); ShinobiPage(2); });
	At(9.5f, [this]() { ShinobiCast(4); }); // Earth Wall
	At(10.3f, [this]() { ShinobiCast(2); }); // Kamui
	FTimerHandle DumpTimer;
	GetWorldTimerManager().SetTimer(DumpTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ShinobiDumpStats();
	}), 11.3f, false);
	// Kamui blocks casting while it lasts (until about 13.9 s); then the dragon: page 5, mounted from about 15.2 s.
	At(14.2f, [this]() { ShinobiDrainChakra(-100.f); ShinobiPage(5); });
	At(14.4f, [this]() { ShinobiCast(1); });
	At(15.8f, [this]() { ShinobiDumpStats(); ConsoleCommand(TEXT("quit")); });
}

void AShinobiPlayerController::RunFollowTest()
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Log = [this](const TCHAR* When)
	{
		const APawn* Me = GetPawn();
		FString Line;
		for (TActorIterator<AShinobiSummon> It(GetWorld()); It; ++It)
		{
			Line += FString::Printf(TEXT(" [%d gap=%.0f speed=%.0f]"), static_cast<int32>(It->GetKind()), Me ? FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) : -1.f, It->GetVelocity().Size2D());
		}
		UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiFollow %s player=(%.0f,%.0f)%s"), When, Me ? Me->GetActorLocation().X : 0.f, Me ? Me->GetActorLocation().Y : 0.f, *Line);
	};
	At(4.f, [this]() { ShinobiPage(4); });
	At(4.3f, [this]() { ShinobiDrainChakra(-100.f); ShinobiCast(4); }); // Pabu
	At(5.5f, [this]() { ShinobiDrainChakra(-100.f); ShinobiCast(2); }); // the owl
	At(6.7f, [this]() { ShinobiDrainChakra(-100.f); ShinobiCast(3); }); // Misha
	At(9.f, [Log]() { Log(TEXT("settled")); });
	// Walk four seconds along the freest direction (the London start is hemmed in by buildings).
	TSharedRef<FTimerHandle> Walk = MakeShared<FTimerHandle>();
	TSharedRef<FVector> WalkDirection = MakeShared<FVector>(FVector::ForwardVector);
	At(10.f, [this, Walk, WalkDirection]()
	{
		if (const APawn* Me = GetPawn())
		{
			float Best = -1.f;
			for (int32 Step = 0; Step < 16; ++Step)
			{
				const FVector Direction = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Step * 22.5f, FVector::UpVector);
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(FollowTest), false, Me);
				const FVector From = Me->GetActorLocation();
				const float Free = GetWorld()->LineTraceSingleByChannel(Hit, From, From + Direction * 4000.f, ECC_WorldStatic, Params) ? Hit.Distance : 4000.f;
				if (Free > Best)
				{
					Best = Free;
					*WalkDirection = Direction;
				}
			}
			UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiFollow walking %s for %.0f free"), *WalkDirection->ToString(), Best);
		}
		GetWorldTimerManager().SetTimer(*Walk, FTimerDelegate::CreateWeakLambda(this, [this, WalkDirection]()
		{
			if (APawn* Me = GetPawn())
			{
				Me->SetActorLocation(Me->GetActorLocation() + *WalkDirection * 12.f, false, nullptr, ETeleportType::None); // 600 u/s; input is not honoured on this map's start
			}
		}), 0.02f, true, 0.f);
	});
	At(12.f, [Log]() { Log(TEXT("walking 2s")); });
	At(14.f, [Log]() { Log(TEXT("walking 4s")); });
	At(14.2f, [this, Walk]() { GetWorldTimerManager().ClearTimer(*Walk); });
	At(17.f, [Log]() { Log(TEXT("stopped 3s")); });
	At(19.f, [this]() { ConsoleCommand(TEXT("quit")); });
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

void AShinobiPlayerController::RunWorldTourTest()
{
	struct FTourPoint
	{
		FString Name;
		FVector Location;
		/** Camera stand-off from the point (a TourOffset:x,y,z tag in cm), zero = the default for its name. */
		FVector Offset = FVector::ZeroVector;
	};
	TArray<FTourPoint> Points;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Points.Add({ TEXT("Village"), It->GetActorLocation() });
		break;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Tour")) || It->ActorHasTag(TEXT("ShinobiCampSite")))
		{
			FString Name = It->GetName();
			FVector Offset = FVector::ZeroVector;
			for (const FName& Tag : It->Tags)
			{
				const FString TagText = Tag.ToString();
				if (TagText.StartsWith(TEXT("Camp:")) || TagText.StartsWith(TEXT("Tour:")))
				{
					Name = TagText.RightChop(5);
				}
				else if (TagText.StartsWith(TEXT("TourOffset:")))
				{
					TArray<FString> Parts;
					if (TagText.RightChop(11).ParseIntoArray(Parts, TEXT(",")) == 3)
					{
						Offset = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
					}
				}
				else if (TagText == TEXT("Boss"))
				{
					Name = TEXT("Boss");
				}
			}
			Points.Add({ Name, It->GetActorLocation(), Offset });
		}
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("World tour: %d points"), Points.Num());
	// -ShinobiPerf: hold each point 10 s so streaming settles, and time the last 5 s.
	const bool bPerf = FParse::Param(FCommandLine::Get(), TEXT("ShinobiPerf"));
	const float FirstAt = 4.f, Step = bPerf ? 10.f : 4.f, Window = bPerf ? 5.f : 2.f;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FTourPoint Point = Points[Index];
		FTimerHandle Move, Shot;
		GetWorldTimerManager().SetTimer(Move, FTimerDelegate::CreateWeakLambda(this, [this, Point]()
		{
			if (ACharacter* Body = GetPawn<ACharacter>())
			{
				if (const AShinobiCharacterBase* Shinobi = Cast<AShinobiCharacterBase>(Body))
				{
					Shinobi->GetAbilitySystemComponent()->SetLooseGameplayTagCount(ShinobiTags::State_Invulnerable, 1); // camps wake up as we pass
				}
				Body->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				Body->GetCharacterMovement()->StopMovementImmediately();
				// The spring arm sits ~4.5 m behind the pawn: stand off 16 m, 9 m up, looking at the point.
				// Close-up points (ground and plant detail) stand off ~5 m, 2.5 m up.
				static const TSet<FString> CloseUps = { TEXT("Meadow"), TEXT("Forest"), TEXT("Street"), TEXT("Edge"), TEXT("Riverbank") };
				const FVector Eye = Point.Location + (!Point.Offset.IsZero() ? Point.Offset
					: Point.Name == TEXT("Meadow") ? FVector(-300.f, 0.f, 120.f)
					: CloseUps.Contains(Point.Name) ? FVector(-450.f, -250.f, 250.f) : FVector(-1300.f, -900.f, 900.f));
				Body->SetActorLocation(Eye, false, nullptr, ETeleportType::TeleportPhysics);
				if (USpringArmComponent* Boom = Body->FindComponentByClass<USpringArmComponent>())
				{
					// Plant close-ups: camera 1.2 m behind the pawn; everything else keeps the normal boom.
					static const float NormalArm = Boom->TargetArmLength;
					const bool bPlants = Point.Name == TEXT("Meadow") || Point.Name == TEXT("Forest");
					Boom->TargetArmLength = bPlants ? 120.f : NormalArm;
					Boom->bDoCollisionTest = !bPlants;
				}
				SetControlRotation((Point.Location - Eye).Rotation());
			}
		}), FirstAt + Index * Step, false);
		const FString File = FString::Printf(TEXT("ShinobiWorldTour_%02d_%s"), Index, *Point.Name);
		// Average frame time over the Window before each shot, for the perf targets.
		TSharedRef<TPair<double, uint64>> Mark = MakeShared<TPair<double, uint64>>();
		FTimerHandle Measure;
		GetWorldTimerManager().SetTimer(Measure, FTimerDelegate::CreateWeakLambda(this, [Mark]()
		{
			*Mark = { FPlatformTime::Seconds(), GFrameCounter };
		}), FirstAt + Index * Step + Step - 0.5f - Window, false);
		GetWorldTimerManager().SetTimer(Shot, FTimerDelegate::CreateWeakLambda(this, [File, Mark, Point]()
		{
			const double Ms = 1000.0 * (FPlatformTime::Seconds() - Mark->Key) / FMath::Max<uint64>(1, GFrameCounter - Mark->Value);
			UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiPerf %s: %.2f ms (%.0f fps) game %.1f render %.1f gpu %.1f"), *Point.Name, Ms, 1000.0 / Ms,
				FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / File), false, false);
		}), FirstAt + Index * Step + Step - 0.5f, false);
	}
	FTimerHandle Quit;
	GetWorldTimerManager().SetTimer(Quit, FTimerDelegate::CreateWeakLambda(this, [this]() { ConsoleCommand(TEXT("quit")); }),
		FirstAt + Points.Num() * Step + 1.f, false);
}

void AShinobiPlayerController::RunFireShotTest()
{
	// A fire patch and a black-flame patch side by side, filmed from a camera 6 m away at dusk-free daylight: close-ups of the cards.
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	At(1.5f, [this]()
	{
		const APawn* Shinobi = GetPawn();
		if (!Shinobi)
		{
			return;
		}
		const FVector Forward = Shinobi->GetActorForwardVector();
		const FVector Right = Shinobi->GetActorRightVector();
		const FVector Ground = Shinobi->GetActorLocation() - FVector(0.f, 0.f, 90.f);
		AShinobiFirePatch::Spawn(GetWorld(), EShinobiFirePatch::Fire, Ground + Forward * 500.f - Right * 170.f, nullptr, 1.f, 0.f);
		AShinobiFirePatch::Spawn(GetWorld(), EShinobiFirePatch::BlackFlame, Ground + Forward * 500.f + Right * 170.f, nullptr, 1.f, 0.f);
		const FVector Target = Ground + Forward * 500.f + FVector(0.f, 0.f, 90.f);
		const FVector Eye = Ground - Forward * 250.f + FVector(0.f, 0.f, 160.f); // behind the pawn, not inside its cloak
		ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(50.f);
		Camera->SetLifeSpan(20.f);
		SetViewTarget(Camera);
	});
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FString Name = FString::Printf(TEXT("ShinobiFireShot_%d"), Index);
		At(2.2f + Index * 0.7f, [Name]() { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), false, false); });
	}
	// The one-shot bursts, each spawned at a fixed spot ahead and filmed 0.12 s after (a late shot misses the effect).
	struct FBurstShot { EShinobiBurst Kind; const TCHAR* Name; float Right; float Scale; };
	static const FBurstShot Bursts[] = {
		{ EShinobiBurst::FireBreath, TEXT("ShinobiFireShot_Breath"), 0.f, 1.f },
		{ EShinobiBurst::Fire, TEXT("ShinobiFireShot_Impact"), 0.f, 1.f },
		{ EShinobiBurst::Lightning, TEXT("ShinobiFireShot_Chidori"), -150.f, 1.f },
		{ EShinobiBurst::Spiral, TEXT("ShinobiFireShot_Rasengan"), 150.f, 1.f },
		{ EShinobiBurst::BlackFlameIgnite, TEXT("ShinobiFireShot_Ignite"), 0.f, 1.f },
		{ EShinobiBurst::Firestorm, TEXT("ShinobiFireShot_Firestorm"), 0.f, 1.f },
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bursts); ++Index)
	{
		const float When = 4.5f + Index * 1.2f;
		At(When, [this, Index]()
		{
			const APawn* Shinobi = GetPawn();
			if (Shinobi)
			{
				const FVector At = Shinobi->GetActorLocation() - FVector(0.f, 0.f, 90.f) + Shinobi->GetActorForwardVector() * 500.f + Shinobi->GetActorRightVector() * Bursts[Index].Right + FVector(0.f, 0.f, 80.f);
				AShinobiBurst::Spawn(GetWorld(), Bursts[Index].Kind, At, Bursts[Index].Scale, Shinobi->GetActorRotation());
			}
		});
		At(When + 0.12f, [Index]() { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Bursts[Index].Name), false, false); });
	}
	At(4.5f + UE_ARRAY_COUNT(Bursts) * 1.2f + 0.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}

void AShinobiPlayerController::RunSusanooShotTest()
{
	// The Perfect Susanoo: cast it, film it from four sides far enough back for all 12 m, from the gameplay camera once the
	// boom has pulled back, through a melee swing (it slashes with its owner), then walking.
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Shot = [](const FString& Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), false, false);
	};
	auto Orbit = [this](float Yaw)
	{
		const APawn* Shinobi = GetPawn();
		if (!Shinobi)
		{
			return;
		}
		const FVector Target = Shinobi->GetActorLocation() + FVector(0.f, 0.f, 520.f); // the figure's chest
		const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * 2600.f + FVector(0.f, 0.f, -150.f);
		ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(60.f);
		Camera->SetLifeSpan(1.f);
		SetViewTarget(Camera);
	};
	At(1.0f, [this]() { ShinobiPage(5); ShinobiDrainChakra(-100.f); });
	At(1.2f, [this]() { ShinobiCast(3); }); // 0.6 s seals: the armour rises at about 1.9
	const float Yaws[] = { 0.f, 0.f, 40.f, 90.f, 160.f }; // the front twice: the first frame after a camera cut can lag
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Yaws); ++Index)
	{
		const float When = 3.2f + Index * 0.5f;
		At(When, [Orbit, Yaw = Yaws[Index]]() { Orbit(Yaw); });
		At(When + 0.2f, [Shot, Index]() { Shot(FString::Printf(TEXT("ShinobiSusanooShot_0%d_Orbit"), Index + 1)); });
	}
	At(5.8f, [this]() { SetViewTarget(GetPawn()); });
	At(7.6f, [Shot]() { Shot(TEXT("ShinobiSusanooShot_06_Gameplay")); }); // the boom has pulled back by now
	At(7.8f, [this, Orbit]() { Orbit(40.f); if (AShinobiPlayerCharacter* P = Cast<AShinobiPlayerCharacter>(GetPawn())) { P->GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EShinobiAbilityInputID::Melee)); } });
	At(7.95f, [this, Orbit]() { Orbit(40.f); });
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float When = 8.1f + Index * 0.35f;
		At(When - 0.05f, [Orbit]() { Orbit(40.f); });
		At(When, [Shot, Index]() { Shot(FString::Printf(TEXT("ShinobiSusanooShot_0%d_Slash"), Index + 7)); });
	}
	// Walking: the figure strides along with its wearer (pushed forward for 2.5 s, filmed from the side).
	At(9.8f, [this]()
	{
		FTimerHandle Walk;
		const double Until = GetWorld()->GetTimeSeconds() + 2.5;
		GetWorldTimerManager().SetTimer(Walk, FTimerDelegate::CreateWeakLambda(this, [this, Until]()
		{
			APawn* Shinobi = GetPawn();
			if (Shinobi && GetWorld()->GetTimeSeconds() < Until)
			{
				Shinobi->AddMovementInput(Shinobi->GetActorForwardVector(), 0.6f);
			}
		}), 0.01f, true);
	});
	for (int32 Index = 0; Index < 2; ++Index)
	{
		At(10.6f + Index * 0.6f, [Orbit]() { Orbit(90.f); });
		At(10.8f + Index * 0.6f, [Shot, Index]() { Shot(FString::Printf(TEXT("ShinobiSusanooShot_1%d_Walk"), Index)); });
	}
	At(12.6f, [this]() { ConsoleCommand(TEXT("quit")); });
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
	// Each cast is held: the seals, then HoldSeconds minus the seals of charge, then the release.
	auto CastJutsu = [this](int32 JutsuSlot, float HoldSeconds)
	{
		ShinobiDrainChakra(-100.f); // refill first, so every jutsu in the sequence is affordable
		ShinobiCastHold(JutsuSlot, HoldSeconds);
	};
	// The aim at the dummies, set once and restored well before each later cast (the server sees the client's camera
	// with some lag, so a restore right at the cast can miss): the dashes and the face shots can leave it turned.
	TSharedRef<FRotator> Aim = MakeShared<FRotator>();
	TSharedRef<FTransform> Start = MakeShared<FTransform>();
	At(2.f, [this, Aim, Start]() { *Aim = FRotator(-8.f, GetControlRotation().Yaw, 0.f); SetControlRotation(*Aim); if (const APawn* Shinobi = GetPawn()) { *Start = Shinobi->GetActorTransform(); } }); // the dummies stand lower
	auto RestoreAim = [this, Aim]() { SetControlRotation(*Aim); };
	At(5.7f, RestoreAim);
	At(8.7f, RestoreAim);
	At(12.7f, RestoreAim);
	At(3.f, [CastJutsu]() { CastJutsu(1, 2.1f); }); // Fireball: 0.6 s seals, 1.5 s charge, released at 5.1
	At(3.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_01_FireballSeal")); });
	// Face shots come from a separate camera in front of the character: turning the player's own camera would also
	// turn the aim, which reaches the server late and throws off the next jutsu.
	// Yaw 0 looks at the face from the front; 90 watches from the side, aimed LookAhead in front of the character.
	auto FaceCamera = [this](float FieldOfView, float Yaw = 0.f, float LookAhead = 0.f, float Distance = 260.f)
	{
		const APawn* Shinobi = GetPawn();
		if (!Shinobi)
		{
			return;
		}
		const FVector Head = Shinobi->GetActorLocation() + FVector(0.f, 0.f, 70.f); // the capsule centre is at the hips
		const FVector Target = Head + Shinobi->GetActorForwardVector() * LookAhead;
		const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * Distance + FVector(0.f, 0.f, 10.f);
		ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(FieldOfView);
		Camera->SetLifeSpan(1.f);
		SetViewTarget(Camera);
	};
	auto BackToPlayer = [this]() { SetViewTarget(GetPawn()); };
	At(4.6f, [FaceCamera]() { FaceCamera(60.f); });
	At(4.9f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_02_FireballCharge")); });
	At(5.f, BackToPlayer);
	At(5.27f, [FaceCamera]() { FaceCamera(70.f, 90.f, 250.f, 520.f); }); // after the launch: the aim follows the view
	At(5.33f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_03_FireballBreath")); });
	At(5.4f, BackToPlayer);
	At(5.45f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_04_FireballImpact")); });
	At(6.2f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_04b_GroundFire")); }); // the patch burning where the ball landed
	At(6.5f, [CastJutsu]() { CastJutsu(4, 1.5f); }); // Amaterasu: 0.4 s seals, released at 8.0
	At(6.7f, [FaceCamera]() { FaceCamera(25.f); }); // zoomed in on the eyes
	At(7.f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_05_AmaterasuEyes")); });
	At(7.2f, BackToPlayer);
	At(8.8f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_06_BlackFlame")); });
	At(9.5f, [CastJutsu]() { CastJutsu(2, 2.f); }); // Chidori: 0.5 s seals, released at 11.5
	At(11.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_07_ChidoriCharge")); });
	At(11.75f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_08_ChidoriHit")); });
	At(13.5f, [CastJutsu]() { CastJutsu(3, 1.9f); }); // Rasengan, released at 15.4; the black flame still burns: Firestorm
	At(15.2f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_09_RasenganCharge")); });
	At(15.65f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_10_RasenganHit")); });
	At(16.5f, [this]()
	{
		if (const APawn* Shinobi = GetPawn())
		{
			AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::Substitution, Shinobi->GetActorLocation() + Shinobi->GetActorForwardVector() * 250.f);
			AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::Fire, Shinobi->GetActorLocation() + Shinobi->GetActorForwardVector() * 250.f + Shinobi->GetActorRightVector() * 250.f);
		}
	});
	At(16.7f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_11_Substitution")); });

	// Page 2 (Almighty Push, Earth Wall, Kamui, Teleport), then page 3 (Water Wall), each with the HUD showing its page.
	// Teleport goes last: it leaves the caster standing against the scene's pillar, in the way of the walls.
	auto ShotUI = [](const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), true, false);
	};
	At(17.2f, [this, RestoreAim]() { ShinobiPage(2); RestoreAim(); });
	At(17.6f, [ShotUI]() { ShotUI(TEXT("ShinobiJutsuShot_12_HudPage2")); }); // after the icons have streamed in
	At(17.8f, [CastJutsu]() { CastJutsu(3, 1.2f); }); // Almighty Push: 0.5 s seals, released at 19.0, the wave 0.15 s later
	At(19.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_13_Push")); });
	At(19.6f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_13b_Push")); });
	At(20.4f, RestoreAim);
	At(20.6f, [CastJutsu]() { CastJutsu(4, 0.8f); }); // Earth Wall: 0.5 s seals, the wall rises at 21.4
	At(22.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_14_EarthWall")); });
	At(23.0f, [this, RestoreAim]() { ShinobiPage(3); RestoreAim(); });
	At(23.2f, [CastJutsu]() { CastJutsu(1, 0.7f); }); // Water Wall: 0.4 s seals, the wall rises at 23.9 (replaces the earth wall: one per caster)
	At(24.8f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_15_WaterWall")); });
	At(25.0f, [ShotUI]() { ShotUI(TEXT("ShinobiJutsuShot_16_HudPage3")); });
	At(25.8f, [this, RestoreAim]() { ShinobiPage(2); RestoreAim(); });
	At(26.0f, [CastJutsu]() { CastJutsu(2, 0.5f); }); // Kamui: 0.3 s seals, phased from 26.5
	At(26.8f, [FaceCamera]() { FaceCamera(55.f, 90.f, 0.f, 320.f); }); // from the side: the water wall stands 350 ahead
	At(26.95f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_17_Kamui")); });
	At(27.1f, BackToPlayer);
	// Page 3's elemental jutsu once Kamui has worn off (it blocks casting): Fire Meteor, Ice Meteors, Water Dragon.
	At(29.7f, [this, RestoreAim]() { ShinobiPage(3); RestoreAim(); });
	At(29.9f, [CastJutsu]() { CastJutsu(3, 0.8f); }); // Fire Meteor: 0.6 s seals, the ring and the meteor appear at 30.7
	At(31.1f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_20_FireMeteorRing")); });
	At(32.25f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_21_FireMeteorImpact")); }); // lands about 32.1
	At(34.0f, RestoreAim);
	At(34.2f, [CastJutsu]() { CastJutsu(4, 0.7f); }); // Ice Meteors: 0.5 s seals, shards fall from 34.9 for about 4.5 s
	At(36.0f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_22_IceShower")); });
	At(37.4f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_22b_IceShower")); });
	At(39.8f, RestoreAim);
	At(40.0f, [CastJutsu]() { CastJutsu(2, 0.9f); }); // Water Dragon: 0.6 s seals, flies from 40.9
	At(41.0f, [FaceCamera]() { FaceCamera(70.f, 90.f, 250.f, 520.f); }); // from the side, just after it leaves
	At(41.1f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_23_WaterDragon")); });
	At(41.2f, BackToPlayer);
	At(41.5f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_23b_WaterDragon")); });
	At(42.6f, [ShotUI]() { ShotUI(TEXT("ShinobiJutsuShot_24_HudPage3Full")); });

	// Back to page 2 for Teleport.
	At(43.2f, [this, RestoreAim]() { ShinobiPage(2); RestoreAim(); });
	At(43.4f, [CastJutsu]() { CastJutsu(1, 0.6f); }); // Teleport: 0.4 s seals, the kunai fly at 43.9
	At(44.05f, [FaceCamera]() { FaceCamera(70.f, 90.f, 250.f, 520.f); });
	At(44.15f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_18_TeleportKunai")); });
	At(44.3f, BackToPlayer);
	At(45.0f, [this]() { ShinobiCast(1); }); // the same key jumps to a kunai
	At(45.5f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_19_TeleportArrival")); });

	// Page 5, slot 3: Susanoo. A tall figure, so the camera stands back and watches from the side.
	At(46.0f, [this, RestoreAim]() { ShinobiPage(5); RestoreAim(); });
	At(46.2f, [CastJutsu]() { CastJutsu(3, 0.7f); }); // Susanoo: 0.6 s seals, the armour rises at 46.9
	At(47.4f, [FaceCamera]() { FaceCamera(80.f, -60.f, 0.f, 1000.f); });
	At(47.55f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_25_Susanoo")); });
	At(47.7f, [FaceCamera]() { FaceCamera(80.f, 150.f, 0.f, 1000.f); }); // two angles: the arena's walls hide one or the other
	At(47.85f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_25b_Susanoo")); });
	At(48.2f, BackToPlayer);
	At(49.0f, [ShotUI]() { ShotUI(TEXT("ShinobiJutsuShot_26_HudSusanoo")); });

	// The summons (page 4 slots 4, 2, 3, 1), each watched from the side as it arrives and goes for the dummies.
	At(49.1f, [this, Start, RestoreAim]() // the teleport and the dashes left the caster somewhere else: back to where it started
	{
		ShinobiPage(4);
		if (APawn* Shinobi = GetPawn())
		{
			Shinobi->SetActorTransform(*Start, false, nullptr, ETeleportType::TeleportPhysics);
			RestoreAim();
		}
	});
	At(49.3f, [CastJutsu]() { CastJutsu(4, 0.8f); }); // Pabu: 0.7 s seals, Pabu appears at 50.0
	At(50.5f, [FaceCamera]() { FaceCamera(70.f, 70.f, 400.f, 900.f); });
	At(50.6f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_27_Pabu")); });
	At(51.2f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_27b_Pabu")); });
	auto ClearSummons = [this]()
	{
		for (TActorIterator<AShinobiSummon> It(GetWorld()); It; ++It) { It->Dismiss(); }
		for (TActorIterator<AShinobiOwl> It(GetWorld()); It; ++It) { It->Dismiss(); }
	};
	At(51.5f, [BackToPlayer, ClearSummons]() { BackToPlayer(); ClearSummons(); });
	At(52.0f, [CastJutsu]() { CastJutsu(2, 0.8f); }); // the owl: at the shoulder at 52.7
	At(53.2f, [FaceCamera]() { FaceCamera(70.f, 70.f, 0.f, 400.f); }); // close: she is a plush at the shoulder
	At(53.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_28_Owl")); });
	At(54.4f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_28b_Owl")); });
	At(54.8f, [BackToPlayer, ClearSummons]() { BackToPlayer(); ClearSummons(); });
	At(55.0f, [CastJutsu]() { CastJutsu(3, 0.8f); }); // Misha: appears at 55.7
	At(56.2f, [FaceCamera]() { FaceCamera(70.f, 70.f, 500.f, 1200.f); });
	At(56.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_29_Misha")); });
	At(57.4f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_29b_Misha")); });
	At(57.8f, [BackToPlayer, ClearSummons]() { BackToPlayer(); ClearSummons(); });
	At(58.0f, [CastJutsu]() { CastJutsu(1, 0.8f); }); // Pusho: appears at 58.7 and pounces on a dummy
	At(59.2f, [FaceCamera]() { FaceCamera(70.f, 70.f, 400.f, 900.f); });
	At(59.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_29c_Pusho")); });
	At(60.1f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_29d_Pusho")); });
	At(60.4f, [BackToPlayer, ClearSummons]() { BackToPlayer(); ClearSummons(); });
	At(60.8f, [ShotUI]() { ShotUI(TEXT("ShinobiJutsuShot_30_HudPage4Full")); });
	// The dragon (page 5): mount, ride in the default (pulled-back) view, breathe fire at the dummies, climb, dismount.
	auto Rider = [this]() { return GetPawn<AShinobiPlayerCharacter>(); };
	At(61.0f, [Rider]() { if (AShinobiPlayerCharacter* P = Rider()) { P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Susanoo)); } }); // the armour hides the rider
	At(61.2f, [this, RestoreAim]() { ShinobiPage(5); RestoreAim(); });
	At(61.4f, [CastJutsu]() { CastJutsu(1, 0.9f); }); // Summon Dragon: 0.8 s seals, mounted from about 62.2
	At(64.2f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_31_DragonRide")); });
	At(64.4f, [FaceCamera]() { FaceCamera(75.f, 60.f, 0.f, 2300.f); });
	At(64.55f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_31b_DragonSide")); });
	At(64.6f, [FaceCamera]() { FaceCamera(45.f, 70.f, 0.f, 800.f); }); // the rider close up
	At(64.65f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_31c_DragonRider")); });
	At(64.8f, BackToPlayer);
	At(65.0f, [Rider]() { if (AShinobiPlayerCharacter* P = Rider()) { P->GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EShinobiAbilityInputID::Melee)); } }); // breath
	At(65.4f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_32_DragonBreath")); });
	At(65.45f, [ShotUI]() { ShotUI(TEXT("ShinobiJutsuShot_32c_DragonCrosshair")); }); // with the HUD: the crosshair where the fire goes
	At(65.7f, [FaceCamera]() { FaceCamera(75.f, 60.f, 500.f, 2300.f); });
	At(65.8f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_32b_DragonBreath")); });
	At(66.0f, BackToPlayer);
	At(66.5f, [FaceCamera]() { FaceCamera(75.f, 80.f, 900.f, 2400.f); }); // the ground where the fire fell
	At(66.6f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_32d_DragonGroundFire")); });
	At(66.7f, BackToPlayer);
	At(66.2f, [Rider]() { if (AShinobiPlayerCharacter* P = Rider()) { P->GetAbilitySystemComponent()->AbilityLocalInputReleased(static_cast<int32>(EShinobiAbilityInputID::Melee)); P->SetFlightClimb(true); } });
	At(67.8f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_33_DragonClimb")); });
	At(67.9f, [Rider]() { if (AShinobiPlayerCharacter* P = Rider()) { P->SetFlightClimb(false); P->SetFlightDive(true); } });
	At(68.1f, [this]() { SetControlRotation(GetControlRotation() + FRotator(0.f, 120.f, 0.f)); }); // a sharp turn to the right: the dragon banks into it
	At(68.22f, [FaceCamera]() { FaceCamera(70.f, 0.f, 0.f, 2200.f); }); // from the front
	At(68.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_33b_DragonBank")); });
	At(68.45f, BackToPlayer);
	At(69.2f, [Rider]() { if (AShinobiPlayerCharacter* P = Rider()) { P->SetFlightDive(false); } });
	At(69.6f, [this]() { ShinobiCast(1); }); // the same key: dismount
	// The other dragon (page 5, slot 2): the crimson one, with ordinary orange fire.
	At(70.2f, [CastJutsu]() { CastJutsu(2, 0.9f); }); // mounted from about 71.0
	At(72.6f, [FaceCamera]() { FaceCamera(75.f, 60.f, 0.f, 2300.f); });
	At(72.7f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_35_CrimsonDragon")); });
	At(72.8f, [FaceCamera]() { FaceCamera(45.f, 70.f, 0.f, 800.f); }); // the rider close up
	At(72.85f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_35c_CrimsonRider")); });
	At(72.9f, BackToPlayer);
	At(73.1f, [Rider]() { if (AShinobiPlayerCharacter* P = Rider()) { P->GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(EShinobiAbilityInputID::Melee)); } });
	At(73.8f, [FaceCamera]() { FaceCamera(75.f, 60.f, 500.f, 2300.f); });
	At(73.9f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_35b_CrimsonBreath")); });
	At(74.2f, [FaceCamera]() { FaceCamera(75.f, 80.f, 900.f, 2400.f); });
	At(74.3f, [Shot]() { Shot(TEXT("ShinobiJutsuShot_35d_CrimsonGroundFire")); });
	At(74.5f, BackToPlayer);
	At(74.8f, [this]() { ShinobiCast(1); }); // the same key: dismount
	At(75.6f, [ShotUI]() { ShotUI(TEXT("ShinobiJutsuShot_34_HudPage5")); });
	At(76.2f, [this]() { ConsoleCommand(TEXT("quit")); });
}