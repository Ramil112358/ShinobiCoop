#include "Shinobi/ShinobiPlayerController.h"

#include "Shinobi/ShinobiPlayerCharacter.h"

#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HighResScreenshot.h"
#include "Components/CapsuleComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

// -ShinobiPortraitShot=<Character> (Scripts/shot_chakra.ps1 -Portrait <Character>): the character's face from 110 cm in
// front, three-quarter and side, then its whole body from the front, standing idle. Shots: ShinobiPortraitShot_<Character>_<View>.
// With -ShinobiPortraitBP=<blueprint class path> (a MetaHuman's built BP_MHC_*), that actor is filmed instead, standing where
// the character stands (hidden): Epic's own assembly, to compare with the parts UShinobiBodyComponent wears.
// With -ShinobiFace=<Face> (AShinobiPlayerCharacter::Faces), that face of the character instead of the saved one.

void AShinobiPlayerController::RunPortraitShotTest(const FString& Who)
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	At(0.5f, [this, Who]()
	{
		ShinobiCharacter(Who);
		if (FString Face; FParse::Value(FCommandLine::Get(), TEXT("ShinobiFace="), Face))
		{
			ShinobiFace(Face); // standalone: the pick above is already on the pawn
		}
	});
	FString BlueprintPath;
	const bool bBlueprint = FParse::Value(FCommandLine::Get(), TEXT("ShinobiPortraitBP="), BlueprintPath);
	const TSharedRef<TWeakObjectPtr<AActor>> Doll = MakeShared<TWeakObjectPtr<AActor>>();
	if (bBlueprint)
	{
		At(0.6f, [this, BlueprintPath, Doll]()
		{
			ACharacter* Shinobi = Cast<ACharacter>(GetPawn());
			UClass* Class = LoadObject<UClass>(nullptr, *BlueprintPath);
			if (!Shinobi || !Class)
			{
				return;
			}
			const FVector Feet = Shinobi->GetActorLocation() - FVector(0.f, 0.f, Shinobi->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			*Doll = GetWorld()->SpawnActor<AActor>(Class, Feet, Shinobi->GetActorRotation() + FRotator(0.f, -90.f, 0.f)); // MetaHumans face +Y
			Shinobi->SetActorHiddenInGame(true);
		});
	}
#if WITH_EDITOR
	// Uncooked -game compiles the new body's shaders and textures in the background: wait for them.
	At(1.5f, []()
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
	});
#endif
	// -ShinobiPortraitPhone: the face views from a front-camera selfie's distance and lens (38 cm, 45 degrees across),
	// to compare with a phone capture without the long lens flattening the face.
	const bool bPhone = FParse::Param(FCommandLine::Get(), TEXT("ShinobiPortraitPhone"));
	// (48 cm from the head point, ~10 cm behind the face; 76 degrees across the 16:9 frame is 45 across its centre square)
	const float FaceDistance = bPhone ? 48.f : 110.f, FaceFOV = bPhone ? 76.f : 30.f;
	// Yaw from the character's facing; Distance from the aim point (the head, or the hips for the whole body).
	const struct { const TCHAR* Name; float Yaw; float Distance; bool bFace; } Views[] = {
		{TEXT("Warmup"), 0.f, FaceDistance, true}, // the first switch to a portrait camera catches the LODs and grooms mid-update
		{TEXT("Face_Front"), 0.f, FaceDistance, true}, {TEXT("Face_ThreeQuarter"), 35.f, FaceDistance, true}, {TEXT("Face_Side"), 90.f, FaceDistance, true},
		{TEXT("Body_Front"), 0.f, 330.f, false}, {TEXT("Body_Side"), 90.f, 330.f, false}, {TEXT("Body_ThreeQuarter"), 40.f, 200.f, false},
		{TEXT("Body_Far"), 0.f, 700.f, false}, {TEXT("Body_Farther"), 0.f, 1400.f, false}, // the groom's distance LODs
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Views); ++Index)
	{
		const float When = 10.f + Index * 1.5f; // grooms and streamed textures settle first
		const auto View = Views[Index];
		const FString Name = FString::Printf(TEXT("ShinobiPortraitShot_%s%s_%s"), *Who, bBlueprint ? TEXT("_BP") : TEXT(""), View.Name);
		At(When, [this, View, Doll, FaceFOV]()
		{
			const ACharacter* Shinobi = Cast<ACharacter>(GetPawn());
			if (!Shinobi)
			{
				return;
			}
			const USkeletalMeshComponent* HeadMesh = Shinobi->GetMesh();
			if (const AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(Shinobi))
			{
				HeadMesh = Player->GetHeroMesh() ? Player->GetHeroMesh() : HeadMesh; // the worn body: Manny's head is at 180 cm whatever its height
			}
			if (const AActor* Shown = Doll->Get())
			{
				TInlineComponentArray<USkeletalMeshComponent*> Meshes(Shown);
				HeadMesh = Meshes.Num() ? Meshes[0] : HeadMesh;
			}
			const FVector Head = HeadMesh->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, 8.f);
			const FVector Target = View.bFace ? Head : Shinobi->GetActorLocation();
			const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(View.Yaw, FVector::UpVector) * View.Distance;
			ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(View.bFace ? FaceFOV : 50.f);
			Camera->SetLifeSpan(1.f);
			SetViewTarget(Camera);
		});
		At(When + 0.4f, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / Name), false, false);
		});
	}
	At(10.f + UE_ARRAY_COUNT(Views) * 1.5f + 0.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}
