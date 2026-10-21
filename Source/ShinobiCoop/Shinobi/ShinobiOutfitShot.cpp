#include "Shinobi/ShinobiPlayerController.h"

#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// -ShinobiOutfitShot=<Outfit> (Scripts/shot_chakra.ps1 -Outfit <Outfit>): the outfit on every roster body, filmed from the
// front, the side and behind while idle, walking, running, mid-jump and settled again, for checking its cloth and hide
// mask in the game (docs/character/outfit-steps.md). Shots: ShinobiOutfitShot_<Outfit>_<Body>_<Phase>_<View>.

void AShinobiPlayerController::RunOutfitShotTest(const FString& Outfit)
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	// A camera 4.5 m out at Yaw from the character's facing, aimed at its hips (the whole body in frame), then the shot a moment later.
	auto Film = [this, At](float When, const FString& Name, float Yaw)
	{
		At(When, [this, Yaw]()
		{
			const APawn* Shinobi = GetPawn();
			if (!Shinobi)
			{
				return;
			}
			const FVector Target = Shinobi->GetActorLocation() + FVector(0.f, 0.f, -10.f);
			const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * 450.f + FVector(0.f, 0.f, 20.f);
			ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(50.f);
			Camera->SetLifeSpan(0.5f);
			SetViewTarget(Camera);
		});
		At(When + 0.1f, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), false, false);
		});
	};
	// Pushes the character forward at Speed (0..1 of a run) for Seconds.
	auto Move = [this, At](float When, float Speed, float Seconds)
	{
		At(When, [this, Speed, Seconds]()
		{
			FTimerHandle Walk;
			const double Until = GetWorld()->GetTimeSeconds() + Seconds;
			GetWorldTimerManager().SetTimer(Walk, FTimerDelegate::CreateWeakLambda(this, [this, Until, Speed]()
			{
				APawn* Shinobi = GetPawn();
				if (Shinobi && GetWorld()->GetTimeSeconds() < Until)
				{
					Shinobi->AddMovementInput(Shinobi->GetActorForwardVector(), Speed);
				}
			}), 0.01f, true);
		});
	};
#if WITH_EDITOR
	// Uncooked -game builds the bodies' and copies' meshes and textures in the background as they first load, and draws them
	// DefaultTexture grey until then: load them all now and wait, so each body is ready when its turn comes.
	TArray<TObjectPtr<UObject>>& Keep = OutfitShotAssets;
	for (int32 Body = 0; Body < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Body)
	{
		Keep.Add(LoadObject<USkeletalMesh>(nullptr, AShinobiPlayerCharacter::Roster[Body].Mesh));
		for (int32 Index = 1; Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits); ++Index)
		{
			if (Outfit.Equals(AShinobiPlayerCharacter::Outfits[Index].Name, ESearchCase::IgnoreCase))
			{
				Keep.Add(LoadObject<USkeletalMesh>(nullptr, *AShinobiPlayerCharacter::OutfitMeshPath(Index, Body), nullptr, LOAD_NoWarn | LOAD_Quiet));
				Keep.Add(LoadObject<UTexture>(nullptr, *AShinobiPlayerCharacter::OutfitHidePath(Index, Body), nullptr, LOAD_NoWarn | LOAD_Quiet));
			}
		}
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
#endif
	const TSharedRef<TOptional<FTransform>> Start = MakeShared<TOptional<FTransform>>(); // where the first body stands
	const TPair<const TCHAR*, float> Views[] = { {TEXT("Front"), 0.f}, {TEXT("Side"), 90.f}, {TEXT("Back"), 180.f} };
	constexpr float BodySeconds = 15.f;
	for (int32 Body = 0; Body < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Body)
	{
		const float T = 1.f + Body * BodySeconds;
		const FString Prefix = FString::Printf(TEXT("ShinobiOutfitShot_%s_%s_"), *Outfit, AShinobiPlayerCharacter::Roster[Body].Name);
		At(T, [this, Body, Outfit, Start]()
		{
			// Back where the run began: the last body's walk and run would otherwise carry this one into a wall.
			if (APawn* Shinobi = GetPawn())
			{
				if (!Start->IsSet())
				{
					*Start = Shinobi->GetActorTransform();
				}
				Shinobi->SetActorTransform(Start->GetValue(), false, nullptr, ETeleportType::ResetPhysics);
				SetControlRotation(Start->GetValue().Rotator());
			}
			ShinobiCharacter(AShinobiPlayerCharacter::Roster[Body].Name);
			ShinobiOutfit(Outfit);
		});
		for (int32 View = 0; View < UE_ARRAY_COUNT(Views); ++View)
		{
			Film(T + 2.5f + View * 0.3f, Prefix + TEXT("1Idle_") + Views[View].Key, Views[View].Value);
			Film(T + 4.5f + View * 0.3f, Prefix + TEXT("2Walk_") + Views[View].Key, Views[View].Value);
			Film(T + 6.5f + View * 0.3f, Prefix + TEXT("3Run_") + Views[View].Key, Views[View].Value);
			// One jump per view, filmed near its top.
			At(T + 7.5f + View * 1.5f, [this]() { if (ACharacter* Shinobi = Cast<ACharacter>(GetPawn())) { Shinobi->Jump(); } });
			Film(T + 7.75f + View * 1.5f, Prefix + TEXT("4Jump_") + Views[View].Key, Views[View].Value);
			Film(T + 13.5f + View * 0.3f, Prefix + TEXT("5Settled_") + Views[View].Key, Views[View].Value);
		}
		Move(T + 3.5f, 0.4f, 1.8f);
		Move(T + 5.5f, 1.f, 1.8f);
	}
	At(1.f + UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster) * BodySeconds + 0.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}
