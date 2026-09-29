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
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "Shinobi/ShinobiInputConfig.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiSettings.h"
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
	ShinobiCharacter(ShinobiSettings::GetSavedCharacter());
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

void AShinobiPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AShinobiPlayerCharacter* Body = Cast<AShinobiPlayerCharacter>(InPawn))
	{
		Body->SetCharacterIndex(PickedCharacter);
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

void AShinobiPlayerController::RunWorldTourTest()
{
	struct FTourPoint
	{
		FString Name;
		FVector Location;
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
			for (const FName& Tag : It->Tags)
			{
				const FString TagText = Tag.ToString();
				if (TagText.StartsWith(TEXT("Camp:")) || TagText.StartsWith(TEXT("Tour:")))
				{
					Name = TagText.RightChop(5);
				}
				else if (TagText == TEXT("Boss"))
				{
					Name = TEXT("Boss");
				}
			}
			Points.Add({ Name, It->GetActorLocation() });
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
				const FVector Eye = Point.Location + (Point.Name == TEXT("Meadow") ? FVector(-300.f, 0.f, 120.f)
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
	At(2.f, [this, Aim]() { *Aim = FRotator(-8.f, GetControlRotation().Yaw, 0.f); SetControlRotation(*Aim); }); // the dummies stand lower
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
	At(17.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}