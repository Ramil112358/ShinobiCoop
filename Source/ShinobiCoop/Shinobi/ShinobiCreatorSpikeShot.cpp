#include "Shinobi/ShinobiPlayerController.h"

#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "Shinobi/Hero/ShinobiFaceShapeAnimInstance.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

// -ShinobiCreatorSpike (character creator phase 0; Scripts/setup_creator.py spike assets): the MetaHuman body in the
// Akatsuki copy shaped by bone scales (thicker, thinner, broader shoulders, a walk, two heights), then the static Ramil head
// on the same body. Shots: ShinobiCreatorSpike_<Phase>_<View>.

void AShinobiPlayerController::RunCreatorSpikeShot()
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Body = [this]() -> UShinobiBodyComponent*
	{
		const AShinobiPlayerCharacter* Shinobi = GetPawn<AShinobiPlayerCharacter>();
		return Shinobi ? Cast<UShinobiBodyComponent>(Shinobi->GetHeroMesh()) : nullptr;
	};
	// Yaw from the character's facing; a face view aims at the head from 110 cm, a body view at the hips from 4.5 m.
	auto Film = [this, At](float When, const FString& Name, float Yaw, bool bFace)
	{
		At(When, [this, Yaw, bFace]()
		{
			const AShinobiPlayerCharacter* Shinobi = GetPawn<AShinobiPlayerCharacter>();
			if (!Shinobi)
			{
				return;
			}
			const FVector Target = bFace ? Shinobi->GetHeroMesh()->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, 8.f)
				: Shinobi->GetActorLocation() + FVector(0.f, 0.f, -10.f);
			const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * (bFace ? 110.f : 450.f);
			ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(bFace ? 30.f : 50.f);
			Camera->SetLifeSpan(0.5f);
			SetViewTarget(Camera);
		});
		At(When + 0.15f, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("ShinobiCreatorSpike_") + Name)), false, false);
		});
	};
	auto Views = [Film](float When, const FString& Phase)
	{
		Film(When, Phase + TEXT("_Front"), 0.f, false);
		Film(When + 0.3f, Phase + TEXT("_Side"), 90.f, false);
		Film(When + 0.6f, Phase + TEXT("_Back"), 180.f, false);
	};
	const FShinobiRosterEntry* RamilMH = nullptr;
	for (const FShinobiRosterEntry& Entry : AShinobiPlayerCharacter::Roster)
	{
		if (FCString::Strcmp(Entry.Name, TEXT("RamilMH")) == 0)
		{
			RamilMH = &Entry;
		}
	}
	if (!RamilMH)
	{
		return;
	}
	const TSharedRef<FTransform> Start = MakeShared<FTransform>(); // where the pawn stands before the walks move it
	At(0.5f, [this, Start]()
	{
		if (const APawn* Shinobi = GetPawn())
		{
			*Start = Shinobi->GetActorTransform();
		}
		ShinobiCharacter(TEXT("RamilMH"));
		IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.OutfitCopySuffix"))->Set(TEXT("_Morph"));
	});
	At(1.f, [this, Body, RamilMH]()
	{
		if (UShinobiBodyComponent* Worn = Body())
		{
			Worn->Wear(TEXT("/Game/Shinobi/Characters/Creator/SK_MHBase.SK_MHBase"), RamilMH->Retargeter, RamilMH->MetaHuman);
		}
		ShinobiOutfit(TEXT("Akatsuki"));
	});
#if WITH_EDITOR
	At(1.5f, []()
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
	});
#endif
	// Bone-shape sliders: limb thickness on the twist/corrective helpers (leaves), the chest on its pec and lat helpers,
	// the torso's width and depth on spine_02..04 (MetaHuman bones run along X); shoulders by clavicle length.
	auto Build = [Body](float Thickness)
	{
		UShinobiBodyComponent* Worn = Body();
		if (!Worn)
		{
			return;
		}
		const FVector Limb(1.f, Thickness, Thickness);
		const FVector Torso(1.f, 1.f + (Thickness - 1.f) * 0.6f, 1.f + (Thickness - 1.f) * 0.6f);
		for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
		{
			for (const TCHAR* Helper : {TEXT("upperarm_twist_01"), TEXT("upperarm_twist_02"), TEXT("upperarm_correctiveRoot"),
				TEXT("lowerarm_twist_01"), TEXT("lowerarm_twist_02"), TEXT("lowerarm_correctiveRoot"), TEXT("thigh_twist_01"),
				TEXT("thigh_twist_02"), TEXT("thigh_correctiveRoot"), TEXT("calf_twist_01"), TEXT("calf_twist_02"),
				TEXT("calf_correctiveRoot"), TEXT("clavicle_pec"), TEXT("spine_04_latissimus")})
			{
				Worn->SetBoneShape(*FString::Printf(TEXT("%s_%s"), Helper, Side), Limb);
			}
		}
		for (const TCHAR* Spine : {TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04")})
		{
			Worn->SetBoneShape(Spine, Torso);
		}
	};
	Views(6.f, TEXT("1Base"));
	At(7.5f, [Build]() { Build(1.25f); });
	Views(9.5f, TEXT("2BuildPlus"));
	At(11.f, [Build]() { Build(0.85f); });
	Views(13.f, TEXT("3BuildMinus"));
	At(14.5f, [Build, Body]()
	{
		Build(1.f);
		if (UShinobiBodyComponent* Worn = Body())
		{
			Worn->SetBoneShape(TEXT("clavicle_l"), FVector(1.2f, 1.f, 1.f));
			Worn->SetBoneShape(TEXT("clavicle_r"), FVector(1.2f, 1.f, 1.f));
		}
	});
	Views(16.5f, TEXT("4ShouldersPlus"));
	At(18.f, [Build, Body]()
	{
		Build(1.25f);
		if (UShinobiBodyComponent* Worn = Body())
		{
			Worn->SetBoneShape(TEXT("clavicle_l"), FVector::OneVector);
			Worn->SetBoneShape(TEXT("clavicle_r"), FVector::OneVector);
		}
	});
	At(18.5f, [this]()
	{
		FTimerHandle Walk;
		const double Until = GetWorld()->GetTimeSeconds() + 1.8;
		GetWorldTimerManager().SetTimer(Walk, FTimerDelegate::CreateWeakLambda(this, [this, Until]()
		{
			APawn* Shinobi = GetPawn();
			if (Shinobi && GetWorld()->GetTimeSeconds() < Until)
			{
				Shinobi->AddMovementInput(Shinobi->GetActorForwardVector(), 0.4f);
			}
		}), 0.01f, true);
	});
	Views(19.5f, TEXT("5BuildPlusWalk"));
	At(21.f, [Build, Body]() { Build(1.f); if (UShinobiBodyComponent* Worn = Body()) { Worn->SetRelativeScale3D(FVector(0.92f)); } });
	Views(23.f, TEXT("6Short"));
	At(24.f, [Body]() { if (UShinobiBodyComponent* Worn = Body()) { Worn->SetRelativeScale3D(FVector(1.08f)); } });
	Views(26.f, TEXT("7Tall"));
	At(27.f, [this, Body]()
	{
		if (UShinobiBodyComponent* Worn = Body())
		{
			Worn->SetRelativeScale3D(FVector(1.f));
			Worn->WearStaticHead(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Shinobi/Characters/Creator/SK_Head_Ramil.SK_Head_Ramil")));
		}
		ShinobiOutfit(TEXT("None"));
	});
#if WITH_EDITOR
	At(27.5f, []() { FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); } });
#endif
	Film(30.5f, TEXT("8Head_Warmup"), 0.f, true); // the first close-up after a swap catches the LODs mid-update
	Film(31.f, TEXT("8Head_Face_Front"), 0.f, true);
	Film(31.5f, TEXT("8Head_Face_ThreeQuarter"), 35.f, true);
	Film(32.f, TEXT("8Head_Face_Side"), 90.f, true);
	Film(32.5f, TEXT("8Head_Face_Back"), 160.f, true);
	Views(33.f, TEXT("8Head_Body"));
	// Face sliders on the MetaHuman face (RamilMH's, back on the same body).
	At(36.f, [Body, RamilMH]()
	{
		if (UShinobiBodyComponent* Worn = Body())
		{
			Worn->Wear(TEXT("/Game/Shinobi/Characters/Creator/SK_MHBase.SK_MHBase"), RamilMH->Retargeter, RamilMH->MetaHuman);
		}
	});
	struct FFaceShot { const TCHAR* Name; float Jaw, Nose, Eyes; };
	const FFaceShot Faces[] = {
		{TEXT("9Face_Base"), 0.f, 0.f, 0.f}, {TEXT("9Face_JawWide"), 1.f, 0.f, 0.f}, {TEXT("9Face_NoseLong"), 0.f, 1.f, 0.f},
		{TEXT("9Face_EyesApart"), 0.f, 0.f, 1.f}, {TEXT("9Face_AllMinus"), -1.f, -1.f, -1.f}, {TEXT("9Face_AllPlus"), 1.f, 1.f, 1.f},
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Faces); ++Index)
	{
		const float When = 42.f + Index * 2.f;
		const FFaceShot Shot = Faces[Index];
		At(When - 0.8f, [Body, Shot]()
		{
			if (UShinobiBodyComponent* Worn = Body())
			{
				Worn->SetFaceShape(TEXT("JawWidth"), Shot.Jaw);
				Worn->SetFaceShape(TEXT("NoseLength"), Shot.Nose);
				Worn->SetFaceShape(TEXT("EyeSpacing"), Shot.Eyes);
			}
		});
		if (Index == 0)
		{
			Film(When - 0.5f, TEXT("9Face_Warmup"), 0.f, true);
		}
		Film(When, FString(Shot.Name) + TEXT("_Front"), 0.f, true);
		Film(When + 0.5f, FString(Shot.Name) + TEXT("_ThreeQuarter"), 35.f, true);
	}
	Views(42.f + UE_ARRAY_COUNT(Faces) * 2.f, TEXT("9Face_AllPlus_Body"));
	// Creator looks from the City Sample crowd kit (ShinobiLook <sex> <build> <face> <hair> <beard> <mustache> <top> <bottom> <shoes>).
	const TCHAR* Looks[] = {
		TEXT("0 1 0 0 255 255 0 0 0"), TEXT("0 2 1 2 0 255 3 2 1"), TEXT("0 0 3 4 255 1 5 4 0"),
		TEXT("1 1 0 0 255 255 0 0 0"), TEXT("1 2 2 3 255 255 3 2 1"), TEXT("1 0 4 5 255 255 5 4 2"),
	};
	const float LooksStart = 42.f + UE_ARRAY_COUNT(Faces) * 2.f + 3.f;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Looks); ++Index)
	{
		const float When = LooksStart + Index * 6.f;
		const FString Command = FString(TEXT("ShinobiLook ")) + Looks[Index];
		At(When, [this, Command, Start]()
		{
			if (APawn* Shinobi = GetPawn())
			{
				Shinobi->SetActorTransform(*Start, false, nullptr, ETeleportType::ResetPhysics);
			}
			ConsoleCommand(Command);
		});
#if WITH_EDITOR
		At(When + 0.5f, []() { FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); } });
#endif
		const FString Name = FString::Printf(TEXT("10Look%d"), Index);
		Film(When + 3.f, Name + TEXT("_Face"), 25.f, true);
		Views(When + 3.6f, Name);
	}
	At(LooksStart + UE_ARRAY_COUNT(Looks) * 6.f + 1.f, [this]() { ConsoleCommand(TEXT("quit")); });
}

// -ShinobiFaceSliderShot (Scripts/shot_chakra.ps1 -FaceSliders): a creator look's face with every face slider
// (UShinobiFaceShapeAnimInstance) at -1 then +1, front and three-quarter, the others at 0.
// Shots: ShinobiFaceSliderShot_<NN>_<Slider>_<Minus|Plus>_<Front|ThreeQuarter>.

void AShinobiPlayerController::RunFaceSliderShot()
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Body = [this]() -> UShinobiBodyComponent*
	{
		const AShinobiPlayerCharacter* Shinobi = GetPawn<AShinobiPlayerCharacter>();
		return Shinobi ? Cast<UShinobiBodyComponent>(Shinobi->GetHeroMesh()) : nullptr;
	};
	auto Film = [this, At](float When, const FString& Name, float Yaw)
	{
		At(When, [this, Yaw]()
		{
			const AShinobiPlayerCharacter* Shinobi = GetPawn<AShinobiPlayerCharacter>();
			if (!Shinobi)
			{
				return;
			}
			const FVector Target = Shinobi->GetHeroMesh()->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, 4.f);
			const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * 120.f;
			ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(30.f);
			Camera->SetLifeSpan(0.5f);
			SetViewTarget(Camera);
		});
		At(When + 0.15f, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("ShinobiFaceSliderShot_") + Name)), false, false);
		});
	};
	// Male face 2 with the pulled-back hair: forehead, brows and ears in view.
	At(0.5f, [this]() { ConsoleCommand(TEXT("ShinobiLook 0 1 2 5 255 255 0 0 0")); });
#if WITH_EDITOR
	At(1.f, []() { FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); } });
#endif
	// Frozen in its idle pose so each pair of shots differs only by the slider (the face still evaluates every frame).
	At(4.5f, [this]() { if (APawn* Shinobi = GetPawn()) { Shinobi->CustomTimeDilation = 0.f; } });
	const TArray<FName> Sliders = UShinobiFaceShapeAnimInstance::GetSliderNames();
	Film(5.f, TEXT("00_Warmup"), 0.f);
	Film(5.6f, TEXT("00_Base_Front"), 0.f);
	Film(6.f, TEXT("00_Base_ThreeQuarter"), 40.f);
	for (int32 Index = 0; Index < Sliders.Num(); ++Index)
	{
		const float T = 7.f + Index * 2.4f;
		for (int32 Sign = 0; Sign < 2; ++Sign)
		{
			const float Value = Sign ? 1.f : -1.f;
			At(T + Sign * 1.2f, [Body, Sliders, Index, Value]()
			{
				if (UShinobiBodyComponent* Worn = Body())
				{
					for (int32 Other = 0; Other < Sliders.Num(); ++Other)
					{
						Worn->SetFaceShape(Sliders[Other], Other == Index ? Value : 0.f);
					}
				}
			});
			const FString Name = FString::Printf(TEXT("%02d_%s_%s"), Index + 1, *Sliders[Index].ToString(), Sign ? TEXT("Plus") : TEXT("Minus"));
			Film(T + Sign * 1.2f + 0.4f, Name + TEXT("_Front"), 0.f);
			Film(T + Sign * 1.2f + 0.8f, Name + TEXT("_ThreeQuarter"), 40.f);
		}
	}
	At(8.f + Sliders.Num() * 2.4f, [this]() { ConsoleCommand(TEXT("quit")); });
}

// -ShinobiFacePresetShot (Scripts/shot_chakra.ps1 -FacePresets): every catalog face (City Sample and Creator), front and
// three-quarter, men with pulled-back hair and women with a low ponytail, for picking presets.
// Shots: ShinobiFacePresetShot_<M|F>_<NN>_<FaceId>_<Front|ThreeQuarter>.

void AShinobiPlayerController::RunFacePresetShot()
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Film = [this, At](float When, const FString& Name, float Yaw)
	{
		At(When, [this, Yaw]()
		{
			const AShinobiPlayerCharacter* Shinobi = GetPawn<AShinobiPlayerCharacter>();
			if (!Shinobi)
			{
				return;
			}
			const FVector Target = Shinobi->GetHeroMesh()->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, 4.f);
			const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * 120.f;
			ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(30.f);
			Camera->SetLifeSpan(0.5f);
			SetViewTarget(Camera);
		});
		At(When + 0.15f, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("ShinobiFacePresetShot_") + Name)), false, false);
		});
	};
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
	if (!Catalog)
	{
		return;
	}
	float T = 1.f;
	for (const EShinobiSex Sex : {EShinobiSex::Male, EShinobiSex::Female})
	{
		const bool bMale = Sex == EShinobiSex::Male;
		for (int32 Index = 0; Index < Catalog->CountFaces(Sex); ++Index)
		{
			const FString Id = Catalog->FindFace(Sex, Index)->Id.ToString();
			const FString Command = FString::Printf(TEXT("ShinobiLook %d 1 %d %d 255 255 0 0 0"), bMale ? 0 : 1, Index, bMale ? 5 : 2);
			At(T, [this, Command]()
			{
				if (APawn* Shinobi = GetPawn())
				{
					Shinobi->CustomTimeDilation = 1.f;
				}
				ConsoleCommand(Command);
			});
#if WITH_EDITOR
			At(T + 0.3f, []() { FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); } });
#endif
			At(T + 2.5f, [this]() { if (APawn* Shinobi = GetPawn()) { Shinobi->CustomTimeDilation = 0.f; } });
			const FString Name = FString::Printf(TEXT("%s_%02d_%s"), bMale ? TEXT("M") : TEXT("F"), Index, *Id);
			Film(T + 3.f, Name + TEXT("_Warmup"), 0.f);
			Film(T + 3.5f, Name + TEXT("_Front"), 0.f);
			Film(T + 4.f, Name + TEXT("_ThreeQuarter"), 40.f);
			T += 5.f;
		}
	}
	At(T + 1.f, [this]() { ConsoleCommand(TEXT("quit")); });
}

// -ShinobiFaceBlendShot (Scripts/shot_chakra.ps1 -FaceBlend): preset blending. A face blended toward another at 0, 25, 50,
// 75 and 100%, then the other face itself, front and three-quarter: how close joint-only blending gets.
// Shots: ShinobiFaceBlendShot_<Pair>_<Step>_<Front|ThreeQuarter>.

void AShinobiPlayerController::RunFaceBlendShot()
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Film = [this, At](float When, const FString& Name, float Yaw)
	{
		At(When, [this, Yaw]()
		{
			const AShinobiPlayerCharacter* Shinobi = GetPawn<AShinobiPlayerCharacter>();
			if (!Shinobi)
			{
				return;
			}
			const FVector Target = Shinobi->GetHeroMesh()->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, 4.f);
			const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * 120.f;
			ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(30.f);
			Camera->SetLifeSpan(0.5f);
			SetViewTarget(Camera);
		});
		At(When + 0.15f, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("ShinobiFaceBlendShot_") + Name)), false, false);
		});
	};
	auto Freeze = [this](float Dilation) { if (APawn* Shinobi = GetPawn()) { Shinobi->CustomTimeDilation = Dilation; } };
	// (sex, face A, face B, hair): catalog indices within the sex
	const struct { const TCHAR* Name; int32 Sex, A, B, Hair; } Pairs[] = {
		{TEXT("M"), 0, 1, 5, 5}, {TEXT("MC"), 0, 6, 8, 5}, {TEXT("F"), 1, 1, 0, 2}, {TEXT("FC"), 1, 7, 8, 2}};
	float T = 1.f;
	for (const auto& Pair : Pairs)
	{
		const FString LookA = FString::Printf(TEXT("ShinobiLook %d 1 %d %d 255 255 0 0 0"), Pair.Sex, Pair.A, Pair.Hair);
		const FString LookB = FString::Printf(TEXT("ShinobiLook %d 1 %d %d 255 255 0 0 0"), Pair.Sex, Pair.B, Pair.Hair);
		At(T, [this, Freeze, LookA]() { Freeze(1.f); ConsoleCommand(LookA); });
#if WITH_EDITOR
		At(T + 0.3f, []() { FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); } });
#endif
		At(T + 2.5f, [Freeze]() { Freeze(0.f); });
		Film(T + 3.f, FString(Pair.Name) + TEXT("_Warmup"), 0.f);
		T += 3.5f;
		for (int32 Step = 0; Step <= 4; ++Step)
		{
			const FString Blend = FString::Printf(TEXT("ShinobiFaceBlend %d %.2f"), Pair.B, Step * 0.25f);
			At(T, [this, Blend]() { ConsoleCommand(Blend); });
			const FString Name = FString::Printf(TEXT("%s_%d_Blend%03d"), Pair.Name, Step, Step * 25);
			Film(T + 0.4f, Name + TEXT("_Front"), 0.f);
			Film(T + 0.8f, Name + TEXT("_ThreeQuarter"), 40.f);
			T += 1.2f;
		}
		At(T, [this, Freeze, LookB]() { Freeze(1.f); ConsoleCommand(LookB); });
#if WITH_EDITOR
		At(T + 0.3f, []() { FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); } });
#endif
		At(T + 2.5f, [Freeze]() { Freeze(0.f); });
		Film(T + 3.f, FString(Pair.Name) + TEXT("_5_Target_Front"), 0.f);
		Film(T + 3.4f, FString(Pair.Name) + TEXT("_5_Target_ThreeQuarter"), 40.f);
		T += 4.f;
	}
	At(T + 1.f, [this]() { ConsoleCommand(TEXT("quit")); });
}

// -ShinobiBodySliderShot (Scripts/shot_chakra.ps1 -BodySliders): a creator look (man, normal build, crewneck and jeans)
// with each body slider at -1 then +1, front and side, the others at 0.
// Shots: ShinobiBodySliderShot_<NN>_<Slider>_<Minus|Plus>_<Front|Side>.

void AShinobiPlayerController::RunBodySliderShot()
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Film = [this, At](float When, const FString& Name, float Yaw)
	{
		At(When, [this, Yaw]()
		{
			const APawn* Shinobi = GetPawn();
			if (!Shinobi)
			{
				return;
			}
			const FVector Target = Shinobi->GetActorLocation() + FVector(0.f, 0.f, -5.f);
			const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * 420.f;
			ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(40.f);
			Camera->SetLifeSpan(0.5f);
			SetViewTarget(Camera);
		});
		At(When + 0.15f, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("ShinobiBodySliderShot_") + Name)), false, false);
		});
	};
	At(0.5f, [this]() { ConsoleCommand(TEXT("ShinobiLook 0 1 2 5 255 255 8 0 0")); });
#if WITH_EDITOR
	At(1.f, []() { FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); } });
#endif
	At(4.5f, [this]() { if (APawn* Shinobi = GetPawn()) { Shinobi->CustomTimeDilation = 0.f; } });
	Film(5.f, TEXT("00_Warmup"), 0.f);
	Film(5.5f, TEXT("00_Base_Front"), 0.f);
	Film(5.9f, TEXT("00_Base_Side"), 90.f);
	const TArray<FName> Sliders = UShinobiBodyComponent::GetBodySliderNames();
	for (int32 Index = 0; Index < Sliders.Num(); ++Index)
	{
		const float T = 7.f + Index * 2.4f;
		for (int32 Sign = 0; Sign < 2; ++Sign)
		{
			const FString Command = FString::Printf(TEXT("ShinobiBodyShape %s %d"), *Sliders[Index].ToString(), Sign ? 1 : -1);
			const FString Reset = FString::Printf(TEXT("ShinobiBodyShape %s 0"), *Sliders[Index].ToString());
			At(T + Sign * 1.2f, [this, Command]() { ConsoleCommand(Command); });
			const FString Name = FString::Printf(TEXT("%02d_%s_%s"), Index + 1, *Sliders[Index].ToString(), Sign ? TEXT("Plus") : TEXT("Minus"));
			Film(T + Sign * 1.2f + 0.4f, Name + TEXT("_Front"), 0.f);
			Film(T + Sign * 1.2f + 0.8f, Name + TEXT("_Side"), 90.f);
			if (Sign)
			{
				At(T + 2.3f, [this, Reset]() { ConsoleCommand(Reset); });
			}
		}
	}
	At(8.f + Sliders.Num() * 2.4f, [this]() { ConsoleCommand(TEXT("quit")); });
}

void AShinobiPlayerController::RunCreatorOutfitShot()
{
	auto At = [this](float Seconds, TFunction<void()> Fn)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Fn)), Seconds, false);
	};
	auto Film = [this, At](float When, const FString& Name, float Yaw)
	{
		At(When, [this, Yaw]()
		{
			const APawn* Shinobi = GetPawn();
			if (!Shinobi)
			{
				return;
			}
			const FVector Target = Shinobi->GetActorLocation() + FVector(0.f, 0.f, -5.f);
			const FVector Eye = Target + Shinobi->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * 420.f;
			ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Target - Eye).Rotation());
			Camera->GetCameraComponent()->SetFieldOfView(40.f);
			Camera->SetLifeSpan(0.5f);
			SetViewTarget(Camera);
		});
		At(When + 0.15f, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("ShinobiCreatorOutfitShot_") + Name)), false, false);
		});
	};
	auto Freeze = [this](float Dilation) { if (APawn* Shinobi = GetPawn()) { Shinobi->CustomTimeDilation = Dilation; } };
	float T = 0.5f;
	for (const TCHAR* Cloak : {TEXT("Hokage"), TEXT("Akatsuki")})
	{
		At(T, [this, Cloak]() { ShinobiOutfit(Cloak); });
		for (int32 Sex = 0; Sex < 2; ++Sex)
		{
			for (int32 Build = 0; Build < 3; ++Build)
			{
				// (ShinobiLook keeps the worn cloak, as its copy fitted to the new frame)
				At(T + 0.2f, [this, Sex, Build]() { ConsoleCommand(FString::Printf(TEXT("ShinobiLook %d %d 0 0 255 255 0 0 0"), Sex, Build)); });
#if WITH_EDITOR
				At(T + 0.6f, []() { FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); } });
#endif
				At(T + 3.f, [Freeze]() { Freeze(0.f); }); // the cloth settled, then held still for both angles
				const FString Name = FString::Printf(TEXT("%s_%s"), Cloak, *UShinobiCreatorCatalog::FittingName(EShinobiSex(Sex), EShinobiBuild(Build)));
				Film(T + 3.2f, Name + TEXT("_Front"), 0.f);
				Film(T + 3.6f, Name + TEXT("_Side"), 90.f);
				At(T + 4.f, [Freeze]() { Freeze(1.f); });
				T += 4.5f;
			}
		}
	}
	At(T + 0.5f, [this]() { ConsoleCommand(TEXT("quit")); });
}
