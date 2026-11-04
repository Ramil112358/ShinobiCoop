#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiGraphics.h"
#include "Shinobi/UI/ShinobiSettings.h"

namespace
{
	/** Keeps the machine's graphics picks (GameUserSettings.ini) across a test: removed for it, put back after. */
	struct FShinobiGraphicsKeysGuard
	{
		const TCHAR* Section = TEXT("/Script/ShinobiCoop.ShinobiSettings");
		TArray<FString> Keys = { TEXT("Upscaler"), TEXT("UpscalingQuality"), TEXT("AntiAliasing"), TEXT("Sharpness"), TEXT("bRayTracing"),
			TEXT("bRayReconstruction"), TEXT("FrameGeneration"), TEXT("ReflexMode"), TEXT("bLowLatency") };
		TMap<FString, FString> Old;
		float OldScale = 100.f;

		FShinobiGraphicsKeysGuard()
		{
			OldScale = ShinobiSettings::GetRenderScale();
			for (const FString& Key : Keys)
			{
				FString Value;
				if (GConfig->GetString(Section, *Key, Value, GGameUserSettingsIni))
				{
					Old.Add(Key, Value);
				}
				GConfig->RemoveKey(Section, *Key, GGameUserSettingsIni);
			}
		}

		~FShinobiGraphicsKeysGuard()
		{
			ShinobiGraphics::SetCapsOverride(nullptr);
			for (const FString& Key : Keys)
			{
				if (const FString* Value = Old.Find(Key))
				{
					GConfig->SetString(Section, *Key, **Value, GGameUserSettingsIni);
				}
				else
				{
					GConfig->RemoveKey(Section, *Key, GGameUserSettingsIni);
				}
			}
			GConfig->Flush(false, GGameUserSettingsIni);
			ShinobiSettings::SetRenderScale(OldScale);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
			GEngine->GetGameUserSettings()->SaveSettings();
			ShinobiGraphics::ApplySaved();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSettingsCycleTest, "ShinobiCoop.UI.SettingsCycleAndSave", ShinobiTestFlags)
bool FShinobiSettingsCycleTest::RunTest(const FString& Parameters)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	const int32 OldQuality = Settings->GetOverallScalabilityLevel();
	const float OldCap = Settings->GetFrameRateLimit();
	const bool bOldOutline = ShinobiLook::IsOutlineEnabled();
	const TCHAR* Section = TEXT("/Script/ShinobiCoop.ShinobiSettings");
	FString OldCharacter;
	GConfig->GetString(Section, TEXT("Character"), OldCharacter, GGameUserSettingsIni);
	FString OldOutfit;
	GConfig->GetString(Section, TEXT("Outfit"), OldOutfit, GGameUserSettingsIni);
	FShinobiTestWorld W;

	// (the Character row selects a played made character over the roster pick: play the roster one meanwhile)
	const int32 OldMade = UShinobiCharacterSave::Load()->Active;
	if (OldMade != INDEX_NONE)
	{
		UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
		Made->Active = INDEX_NONE;
		Made->Save();
	}
	GConfig->SetString(Section, TEXT("Character"), TEXT("Hero"), GGameUserSettingsIni);
	ShinobiSettings::Cycle(ShinobiSettings::ERow::Character, W.World);
	const FString Second = AShinobiPlayerCharacter::Roster[1 % UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster)].Name;
	TestEqual(TEXT("Character Hero -> the next roster entry (wrapping)"), ShinobiSettings::GetSavedCharacter(), Second);
	TestTrue(TEXT("Character label"), ShinobiSettings::Label(ShinobiSettings::ERow::Character).Contains(Second));
	GConfig->SetString(Section, TEXT("Character"), TEXT("NoSuchNinja"), GGameUserSettingsIni);
	TestEqual(TEXT("An unknown saved name reads as the hero"), ShinobiSettings::GetSavedCharacter(), FString(TEXT("Hero")));

	GConfig->SetString(Section, TEXT("Outfit"), AShinobiPlayerCharacter::Outfits[0].Name, GGameUserSettingsIni);
	ShinobiSettings::Cycle(ShinobiSettings::ERow::Outfit, W.World);
	const FString NextOutfit = AShinobiPlayerCharacter::Outfits[1 % UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits)].Name;
	TestEqual(TEXT("Outfit None -> the next outfit (wrapping)"), ShinobiSettings::GetSavedOutfit(), NextOutfit);
	TestTrue(TEXT("Outfit label"), ShinobiSettings::Label(ShinobiSettings::ERow::Outfit).Contains(NextOutfit));
	GConfig->SetString(Section, TEXT("Outfit"), TEXT("NoSuchCloak"), GGameUserSettingsIni);
	TestEqual(TEXT("An unknown saved outfit reads as none"), ShinobiSettings::GetSavedOutfit(), FString(AShinobiPlayerCharacter::Outfits[0].Name));

	Settings->SetOverallScalabilityLevel(0);
	ShinobiSettings::Cycle(ShinobiSettings::ERow::Quality, W.World);
	TestEqual(TEXT("Quality Low -> Medium"), Settings->GetOverallScalabilityLevel(), 1);
	TestTrue(TEXT("Quality label"), ShinobiSettings::Label(ShinobiSettings::ERow::Quality).Contains(TEXT("Medium")));

	Settings->SetFrameRateLimit(30.f);
	ShinobiSettings::Cycle(ShinobiSettings::ERow::FrameCap, W.World);
	TestEqual(TEXT("Frame cap 30 -> 60"), Settings->GetFrameRateLimit(), 60.f);
	Settings->SetFrameRateLimit(144.f);
	ShinobiSettings::Cycle(ShinobiSettings::ERow::FrameCap, W.World);
	TestEqual(TEXT("Frame cap 144 -> unlimited"), Settings->GetFrameRateLimit(), 0.f);
	TestTrue(TEXT("Unlimited label"), ShinobiSettings::Label(ShinobiSettings::ERow::FrameCap).Contains(TEXT("Unlimited")));

	{
		FShinobiGraphicsKeysGuard Graphics;
		ShinobiGraphics::SetUpscaler(ShinobiGraphics::EUpscaler::TSR);
		ShinobiGraphics::SetUpscalingQuality(0);
		ShinobiSettings::Cycle(ShinobiSettings::ERow::RenderScale, W.World);
		TestEqual(TEXT("Upscaling quality Quality -> Balanced"), ShinobiGraphics::GetUpscalingQuality(), 1);
		TestEqual(TEXT("...draws at 58 %"), FMath::RoundToInt(ShinobiSettings::GetRenderScale()), 58);
		TestEqual(TEXT("shown by name"), ShinobiSettings::RowValue(ShinobiSettings::ERow::RenderScale), FString(TEXT("Balanced")));
	}

	ShinobiLook::SetOutlineEnabled(W.World, true);
	AShinobiPlayerCharacter* Player = W.Spawn<AShinobiPlayerCharacter>(FVector::ZeroVector);
	TestTrue(TEXT("Outline on"), Player->GetBodyMesh()->bRenderCustomDepth);
	ShinobiSettings::Cycle(ShinobiSettings::ERow::Outline, W.World);
	TestFalse(TEXT("Outline off after cycling"), ShinobiLook::IsOutlineEnabled());
	TestFalse(TEXT("Existing character stops writing custom depth"), Player->GetBodyMesh()->bRenderCustomDepth);
	AShinobiPlayerCharacter* Late = W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f));
	TestFalse(TEXT("New characters honour the setting"), Late->GetBodyMesh()->bRenderCustomDepth);

	// Leave the machine's settings as they were.
	Settings->SetOverallScalabilityLevel(OldQuality);
	Settings->SetFrameRateLimit(OldCap);
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
	ShinobiLook::SetOutlineEnabled(W.World, bOldOutline);
	GConfig->SetString(Section, TEXT("Character"), *OldCharacter, GGameUserSettingsIni);
	GConfig->SetString(Section, TEXT("Outfit"), *OldOutfit, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	if (OldMade != INDEX_NONE)
	{
		UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
		Made->Active = OldMade;
		Made->Save();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSettingsStepBackTest, "ShinobiCoop.UI.SettingsStepBackWraps", ShinobiTestFlags)
bool FShinobiSettingsStepBackTest::RunTest(const FString& Parameters)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	const int32 OldQuality = Settings->GetOverallScalabilityLevel();
	const float OldCap = Settings->GetFrameRateLimit();
	const float OldScale = ShinobiSettings::GetRenderScale();
	const TCHAR* Section = TEXT("/Script/ShinobiCoop.ShinobiSettings");
	FString OldOutfit;
	GConfig->GetString(Section, TEXT("Outfit"), OldOutfit, GGameUserSettingsIni);
	FShinobiTestWorld W;
	using ShinobiSettings::ERow;

	Settings->SetOverallScalabilityLevel(0);
	ShinobiSettings::Step(ERow::Quality, W.World, -1);
	TestEqual(TEXT("Quality Low, back -> Cinematic"), Settings->GetOverallScalabilityLevel(), 4);
	ShinobiSettings::Step(ERow::Quality, W.World, 1);
	TestEqual(TEXT("Quality Cinematic, on -> Low"), Settings->GetOverallScalabilityLevel(), 0);
	TestEqual(TEXT("Quality value"), ShinobiSettings::RowValue(ERow::Quality), FString(TEXT("Low")));
	TestEqual(TEXT("Quality name"), ShinobiSettings::RowName(ERow::Quality), FString(TEXT("Graphics quality")));
	TestEqual(TEXT("Label is name: value"), ShinobiSettings::Label(ERow::Quality), FString(TEXT("Graphics quality: Low")));

	Settings->SetFrameRateLimit(30.f);
	ShinobiSettings::Step(ERow::FrameCap, W.World, -1);
	TestEqual(TEXT("Frame cap 30, back -> unlimited"), Settings->GetFrameRateLimit(), 0.f);
	ShinobiSettings::Step(ERow::FrameCap, W.World, -1);
	TestEqual(TEXT("Frame cap unlimited, back -> 144"), Settings->GetFrameRateLimit(), 144.f);
	TestEqual(TEXT("Frame cap value"), ShinobiSettings::RowValue(ERow::FrameCap), FString(TEXT("144 FPS")));

	{
		FShinobiGraphicsKeysGuard Graphics;
		ShinobiGraphics::SetUpscaler(ShinobiGraphics::EUpscaler::TSR);
		ShinobiGraphics::SetUpscalingQuality(0);
		ShinobiSettings::Step(ERow::RenderScale, W.World, -1);
		TestEqual(TEXT("Upscaling quality Quality, back -> Ultra Performance"), ShinobiSettings::RowValue(ERow::RenderScale), FString(TEXT("Ultra Performance")));
		TestEqual(TEXT("...draws at 33 %"), FMath::RoundToInt(ShinobiSettings::GetRenderScale()), 33);
		ShinobiGraphics::SetUpscaler(ShinobiGraphics::EUpscaler::Off);
		TestEqual(TEXT("upscaling off: native resolution"), FMath::RoundToInt(ShinobiSettings::GetRenderScale()), 100);
		TestFalse(TEXT("...and the quality is locked"), ShinobiSettings::RowLock(ERow::RenderScale).IsEmpty());
		ShinobiSettings::Step(ERow::RenderScale, W.World, 1);
		TestEqual(TEXT("...it doesn't step"), ShinobiGraphics::GetUpscalingQuality(), 3);
	}

	GConfig->SetString(Section, TEXT("Outfit"), AShinobiPlayerCharacter::Outfits[0].Name, GGameUserSettingsIni);
	ShinobiSettings::Step(ERow::Outfit, W.World, -1);
	TestEqual(TEXT("Outfit None, back -> the last outfit"), ShinobiSettings::GetSavedOutfit(),
		FString(AShinobiPlayerCharacter::Outfits[UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits) - 1].Name));

	for (uint8 Row = 0; Row < uint8(ERow::Num); ++Row)
	{
		TestFalse(FString::Printf(TEXT("row %d has a name"), Row), ShinobiSettings::RowName(ERow(Row)).IsEmpty());
		TestFalse(FString::Printf(TEXT("row %d has a description"), Row), ShinobiSettings::RowDescription(ERow(Row)).IsEmpty());
		TestFalse(FString::Printf(TEXT("row %d has a section"), Row), ShinobiSettings::RowSection(ERow(Row)).IsEmpty());
	}

	Settings->SetOverallScalabilityLevel(OldQuality);
	Settings->SetFrameRateLimit(OldCap);
	ShinobiSettings::SetRenderScale(OldScale);
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
	GConfig->SetString(Section, TEXT("Outfit"), *OldOutfit, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSettingsOptionsTest, "ShinobiCoop.UI.SettingsOptionsPickDirectly", ShinobiTestFlags)
bool FShinobiSettingsOptionsTest::RunTest(const FString& Parameters)
{
	// Rows with many values are picked from a list (a dropdown), any value in one go; On / Off rows stay steppers.
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	const int32 OldQuality = Settings->GetOverallScalabilityLevel();
	const float OldCap = Settings->GetFrameRateLimit();
	FShinobiTestWorld W;
	using ShinobiSettings::ERow;

	TestTrue(TEXT("quality: a dropdown"), ShinobiSettings::UsesDropdown(ERow::Quality));
	TestTrue(TEXT("upscaling quality: a dropdown"), ShinobiSettings::UsesDropdown(ERow::RenderScale));
	TestFalse(TEXT("dynamic resolution: a stepper"), ShinobiSettings::UsesDropdown(ERow::DynamicResolution));
	TestTrue(TEXT("frame cap: a dropdown"), ShinobiSettings::UsesDropdown(ERow::FrameCap));
	TestTrue(TEXT("character: a dropdown"), ShinobiSettings::UsesDropdown(ERow::Character));
	TestFalse(TEXT("VSync: a stepper"), ShinobiSettings::UsesDropdown(ERow::VSync));
	TestFalse(TEXT("outline: a stepper"), ShinobiSettings::UsesDropdown(ERow::Outline));

	const TArray<FString> Quality = ShinobiSettings::RowOptions(ERow::Quality);
	TestEqual(TEXT("five presets"), Quality.Num(), 5);
	Settings->SetOverallScalabilityLevel(0);
	ShinobiSettings::SetRowIndex(ERow::Quality, W.World, 3);
	TestEqual(TEXT("Low straight to Epic"), Settings->GetOverallScalabilityLevel(), 3);
	TestEqual(TEXT("the shown value is the picked option"), ShinobiSettings::RowValue(ERow::Quality), Quality[3]);
	TestEqual(TEXT("its index"), ShinobiSettings::RowIndex(ERow::Quality), 3);

	const TArray<FString> Caps = ShinobiSettings::RowOptions(ERow::FrameCap);
	ShinobiSettings::SetRowIndex(ERow::FrameCap, W.World, Caps.Num() - 1);
	TestEqual(TEXT("the last frame cap: unlimited"), Settings->GetFrameRateLimit(), 0.f);
	TestEqual(TEXT("listed as Unlimited"), Caps.Last(), FString(TEXT("Unlimited")));
	ShinobiSettings::SetRowIndex(ERow::FrameCap, W.World, Caps.Num()); // out of range
	TestEqual(TEXT("an option off the list changes nothing"), Settings->GetFrameRateLimit(), 0.f);

	for (uint8 Row = 0; Row < uint8(ERow::Num); ++Row)
	{
		const int32 Index = ShinobiSettings::RowIndex(ERow(Row));
		TestTrue(FString::Printf(TEXT("row %d: its index is on its list (or none)"), Row), Index == INDEX_NONE || ShinobiSettings::RowOptions(ERow(Row)).IsValidIndex(Index));
	}

	Settings->SetOverallScalabilityLevel(OldQuality);
	Settings->SetFrameRateLimit(OldCap);
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSettingsGraphicsTechTest, "ShinobiCoop.UI.SettingsUpscalerFrameGenRayTracing", ShinobiTestFlags)
bool FShinobiSettingsGraphicsTechTest::RunTest(const FString& Parameters)
{
	// Every graphics row is listed; what the machine can't do is locked with a reason (faked here: no plugin or RTX card
	// needed), and the saved picks fall back when it can't.
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	const bool bOldVSync = Settings->IsVSyncEnabled();
	const bool bOldDynamicRes = Settings->IsDynamicResolutionEnabled();
	FShinobiGraphicsKeysGuard Graphics;
	const TCHAR* Section = Graphics.Section;
	FShinobiTestWorld W;
	using ShinobiSettings::ERow;
	using ShinobiGraphics::EUpscaler;
	using ShinobiGraphics::EAntiAliasing;
	const ERow GraphicsRows[] = { ERow::Upscaler, ERow::RenderScale, ERow::AntiAliasing, ERow::Sharpness, ERow::DynamicResolution, ERow::FrameGeneration,
		ERow::LowLatency, ERow::RayTracing, ERow::RayReconstruction };

	// A PC with none of it: every row is still listed; DLSS / DLAA, frame generation, Reflex and ray tracing are locked, with reasons.
	ShinobiGraphics::FCaps None;
	None.DLSSLock = None.RayReconstructionLock = None.ReflexLock = None.FrameGenLock = TEXT("Requires an NVIDIA GeForce RTX graphics card.");
	None.RayTracingLock = TEXT("Requires a graphics card with hardware ray tracing, running DirectX 12.");
	None.FrameGenAllModes = { TEXT("Off"), TEXT("On2X"), TEXT("On3X"), TEXT("On4X") };
	None.FrameGenModes = { TEXT("Off") };
	ShinobiGraphics::SetCapsOverride(&None);
	for (const ERow Row : GraphicsRows)
	{
		TestTrue(FString::Printf(TEXT("row %d: listed"), int32(Row)), ShinobiSettings::RowsFor(true).Contains(Row));
	}
	TestTrue(TEXT("Resolution is listed, under Screen mode"), ShinobiSettings::RowsFor(true).IndexOfByKey(ERow::Resolution) == ShinobiSettings::RowsFor(true).IndexOfByKey(ERow::WindowMode) + 1);
	for (const ERow Row : { ERow::FrameGeneration, ERow::LowLatency, ERow::RayTracing, ERow::RayReconstruction })
	{
		TestFalse(FString::Printf(TEXT("row %d: locked, with a reason"), int32(Row)), ShinobiSettings::RowLock(Row).IsEmpty());
		TestTrue(FString::Printf(TEXT("row %d: the note says it's locked"), int32(Row)), ShinobiSettings::RowNote(Row).StartsWith(TEXT("Locked: ")));
	}
	TestEqual(TEXT("upscaler options"), FString::Join(ShinobiSettings::RowOptions(ERow::Upscaler), TEXT(",")), FString(TEXT("Off,TSR,NVIDIA DLSS")));
	TestEqual(TEXT("upscaling quality options: no DLAA / native among them"), FString::Join(ShinobiSettings::RowOptions(ERow::RenderScale), TEXT(",")),
		FString(TEXT("Quality,Balanced,Performance,Ultra Performance")));
	TestEqual(TEXT("anti-aliasing options"), FString::Join(ShinobiSettings::RowOptions(ERow::AntiAliasing), TEXT(",")), FString(TEXT("TSR,NVIDIA DLAA")));
	TestFalse(TEXT("NVIDIA DLSS is locked"), ShinobiSettings::OptionLock(ERow::Upscaler, 2).IsEmpty());
	TestTrue(TEXT("...and the note says why"), ShinobiSettings::RowNote(ERow::Upscaler).StartsWith(TEXT("NVIDIA DLSS: ")));
	ShinobiGraphics::SetUpscaler(EUpscaler::TSR);
	ShinobiSettings::Step(ERow::Upscaler, W.World, 1);
	TestEqual(TEXT("stepping on from TSR skips the locked DLSS (to Off)"), ShinobiSettings::RowValue(ERow::Upscaler), FString(TEXT("Off")));
	ShinobiSettings::SetRowIndex(ERow::Upscaler, W.World, 2);
	TestEqual(TEXT("...and DLSS can't be picked"), ShinobiSettings::RowValue(ERow::Upscaler), FString(TEXT("Off")));
	TestTrue(TEXT("upscaling off: anti-aliasing can be picked"), ShinobiSettings::RowLock(ERow::AntiAliasing).IsEmpty());
	TestTrue(TEXT("...TSR, without DLSS"), ShinobiGraphics::GetAntiAliasing() == EAntiAliasing::TSR);
	TestFalse(TEXT("...NVIDIA DLAA is locked"), ShinobiSettings::OptionLock(ERow::AntiAliasing, 1).IsEmpty());
	TestTrue(TEXT("anti-aliasing is a dropdown, to show the locked DLAA"), ShinobiSettings::UsesDropdown(ERow::AntiAliasing));
	ShinobiSettings::Step(ERow::RayTracing, W.World, 1);
	TestEqual(TEXT("a locked row doesn't step"), ShinobiSettings::RowValue(ERow::RayTracing), FString(TEXT("Off")));
	GConfig->SetString(Section, TEXT("Upscaler"), TEXT("DLSS"), GGameUserSettingsIni);
	TestTrue(TEXT("a saved DLSS pick falls back to TSR"), ShinobiGraphics::GetUpscaler() == EUpscaler::TSR);
	GConfig->SetBool(Section, TEXT("bRayTracing"), true, GGameUserSettingsIni);
	TestFalse(TEXT("saved ray tracing is off without hardware support"), ShinobiGraphics::IsRayTracing());
	TestEqual(TEXT("frame generation options are listed anyway"), FString::Join(ShinobiSettings::RowOptions(ERow::FrameGeneration), TEXT(",")), FString(TEXT("Off,2X,3X,4X")));

	// An RTX 40-class PC with the plugins: 2X frame generation, not 3X / 4X (RTX 50).
	ShinobiGraphics::FCaps Rtx40;
	Rtx40.bDLSS = Rtx40.bRayReconstruction = Rtx40.bReflex = Rtx40.bRayTracing = true;
	Rtx40.FrameGenAllModes = { TEXT("Off"), TEXT("On2X"), TEXT("On3X"), TEXT("On4X") };
	Rtx40.FrameGenModes = { TEXT("Off"), TEXT("On2X") };
	ShinobiGraphics::SetCapsOverride(&Rtx40);
	TestTrue(TEXT("RTX 40: 2X can be picked"), ShinobiSettings::OptionLock(ERow::FrameGeneration, 1).IsEmpty());
	TestFalse(TEXT("RTX 40: 3X is locked"), ShinobiSettings::OptionLock(ERow::FrameGeneration, 2).IsEmpty());
	TestTrue(TEXT("...the note names the locked ones"), ShinobiSettings::RowNote(ERow::FrameGeneration).StartsWith(TEXT("3X, 4X: ")));

	// An RTX 50-class PC.
	ShinobiGraphics::FCaps Rtx;
	Rtx.bDLSS = Rtx.bRayReconstruction = Rtx.bReflex = Rtx.bRayTracing = true;
	Rtx.FrameGenAllModes = Rtx.FrameGenModes = { TEXT("Off"), TEXT("On2X"), TEXT("On3X"), TEXT("On4X"), TEXT("OnDynamic") };
	ShinobiGraphics::SetCapsOverride(&Rtx);
	for (const ERow Row : GraphicsRows)
	{
		TestTrue(FString::Printf(TEXT("row %d: no option locked"), int32(Row)), ShinobiSettings::RowNote(Row).IsEmpty() || Row == ERow::AntiAliasing || Row == ERow::RenderScale);
	}
	TestEqual(TEXT("frame generation options"), FString::Join(ShinobiSettings::RowOptions(ERow::FrameGeneration), TEXT(",")), FString(TEXT("Off,2X,3X,4X,Dynamic")));
	TestEqual(TEXT("Reflex options"), FString::Join(ShinobiSettings::RowOptions(ERow::LowLatency), TEXT(",")), FString(TEXT("Off,On,On + Boost")));

	// First run: no saved upscaler. A 100 % render scale keeps native resolution (with DLAA where it runs); below it, DLSS.
	GConfig->RemoveKey(Section, TEXT("Upscaler"), GGameUserSettingsIni);
	GConfig->RemoveKey(Section, TEXT("AntiAliasing"), GGameUserSettingsIni);
	GConfig->RemoveKey(Section, TEXT("UpscalingQuality"), GGameUserSettingsIni);
	ShinobiSettings::SetRenderScale(100.f);
	TestTrue(TEXT("first run at 100 %: upscaling off"), ShinobiGraphics::GetUpscaler() == EUpscaler::Off);
	TestTrue(TEXT("...anti-aliased by DLAA where DLSS runs"), ShinobiGraphics::GetAntiAliasing() == EAntiAliasing::DLAA);
	TestTrue(TEXT("...so DLSS draws it"), ShinobiGraphics::IsDLSSActive());
	TestEqual(TEXT("...the row reads NVIDIA DLAA"), ShinobiSettings::RowValue(ERow::AntiAliasing), FString(TEXT("NVIDIA DLAA")));
	ShinobiSettings::SetRenderScale(66.7f);
	TestTrue(TEXT("first run below 100 %: DLSS where it runs"), ShinobiGraphics::GetUpscaler() == EUpscaler::DLSS);
	TestEqual(TEXT("...at the nearest quality"), ShinobiGraphics::GetUpscalingQuality(), 0);

	ShinobiSettings::SetRowIndex(ERow::Upscaler, W.World, 2);
	TestEqual(TEXT("upscaling with DLSS"), ShinobiSettings::RowValue(ERow::Upscaler), FString(TEXT("NVIDIA DLSS")));
	TestFalse(TEXT("...anti-aliasing is locked (DLSS smooths edges itself)"), ShinobiSettings::RowLock(ERow::AntiAliasing).IsEmpty());
	TestEqual(TEXT("...and reads the upscaler"), ShinobiSettings::RowValue(ERow::AntiAliasing), FString(TEXT("NVIDIA DLSS")));
	ShinobiSettings::SetRowIndex(ERow::RenderScale, W.World, 2);
	TestEqual(TEXT("Performance: 50 %"), FMath::RoundToInt(ShinobiGraphics::GetTargetScreenPercentage()), 50);
	ShinobiSettings::Step(ERow::Upscaler, W.World, 1);
	TestEqual(TEXT("NVIDIA DLSS -> Off (wraps)"), ShinobiSettings::RowValue(ERow::Upscaler), FString(TEXT("Off")));
	TestEqual(TEXT("upscaling off: 100 %"), FMath::RoundToInt(ShinobiGraphics::GetTargetScreenPercentage()), 100);
	TestEqual(TEXT("...the quality is kept for later"), ShinobiGraphics::GetUpscalingQuality(), 2);

	ShinobiSettings::SetRowIndex(ERow::Sharpness, W.World, 2);
	TestEqual(TEXT("sharpness"), ShinobiSettings::RowValue(ERow::Sharpness), FString(TEXT("Medium")));

	// Ray Reconstruction needs DLSS and ray tracing: with upscaling off it turns DLAA on, while upscaling it picks DLSS.
	ShinobiGraphics::SetAntiAliasing(EAntiAliasing::TSR);
	ShinobiGraphics::SetRayTracing(false);
	ShinobiSettings::SetRowIndex(ERow::RayReconstruction, W.World, 1);
	TestTrue(TEXT("Ray Reconstruction on"), ShinobiGraphics::IsRayReconstruction());
	TestTrue(TEXT("...turned DLAA on (upscaling is off)"), ShinobiGraphics::GetAntiAliasing() == EAntiAliasing::DLAA);
	TestTrue(TEXT("...and ray tracing"), ShinobiGraphics::IsRayTracing());
	ShinobiGraphics::SetUpscaler(EUpscaler::TSR);
	TestFalse(TEXT("TSR upscaling: no Ray Reconstruction"), ShinobiGraphics::IsRayReconstruction());
	ShinobiSettings::SetRowIndex(ERow::RayReconstruction, W.World, 0);
	ShinobiSettings::SetRowIndex(ERow::RayReconstruction, W.World, 1);
	TestTrue(TEXT("...turning it on picks DLSS upscaling"), ShinobiGraphics::GetUpscaler() == EUpscaler::DLSS);
	ShinobiSettings::SetRowIndex(ERow::RayTracing, W.World, 0);
	TestFalse(TEXT("no ray tracing: no Ray Reconstruction"), ShinobiGraphics::IsRayReconstruction());

	Settings->SetVSyncEnabled(true);
	ShinobiSettings::SetRowIndex(ERow::LowLatency, W.World, 0);
	TestEqual(TEXT("Reflex off"), ShinobiGraphics::GetReflexMode(), 0);
	ShinobiSettings::SetRowIndex(ERow::FrameGeneration, W.World, 2);
	TestEqual(TEXT("frame generation 3X"), ShinobiSettings::RowValue(ERow::FrameGeneration), FString(TEXT("3X")));
	TestFalse(TEXT("frame generation turned VSync off"), Settings->IsVSyncEnabled());
	TestEqual(TEXT("...and forces Reflex on"), ShinobiSettings::RowValue(ERow::LowLatency), FString(TEXT("On")));
	TestFalse(TEXT("...Reflex Off is locked"), ShinobiSettings::OptionLock(ERow::LowLatency, 0).IsEmpty());
	ShinobiSettings::SetRowIndex(ERow::LowLatency, W.World, 2);
	TestEqual(TEXT("...but On + Boost can be picked"), ShinobiSettings::RowValue(ERow::LowLatency), FString(TEXT("On + Boost")));
	TestFalse(TEXT("VSync is locked while frame generation is on"), ShinobiSettings::RowLock(ERow::VSync).IsEmpty());
	ShinobiSettings::SetRowIndex(ERow::VSync, W.World, 1);
	TestFalse(TEXT("...and stays off"), Settings->IsVSyncEnabled());
	ShinobiSettings::SetRowIndex(ERow::FrameGeneration, W.World, 0);
	TestEqual(TEXT("frame generation off: Reflex back to the pick"), ShinobiSettings::RowValue(ERow::LowLatency), FString(TEXT("On + Boost")));
	TestTrue(TEXT("VSync unlocked"), ShinobiSettings::RowLock(ERow::VSync).IsEmpty());
	ShinobiSettings::SetRowIndex(ERow::VSync, W.World, 1);
	TestTrue(TEXT("VSync can come back on"), Settings->IsVSyncEnabled());

	ShinobiSettings::SetRowIndex(ERow::DynamicResolution, W.World, 1);
	TestTrue(TEXT("dynamic resolution on"), Settings->IsDynamicResolutionEnabled());
	TestEqual(TEXT("its row"), ShinobiSettings::RowValue(ERow::DynamicResolution), FString(TEXT("On")));

	TestEqual(TEXT("labels: plain On is 2X"), ShinobiGraphics::FrameGenLabel(TEXT("On")), FString(TEXT("2X")));
	TestEqual(TEXT("labels: Off"), ShinobiGraphics::FrameGenLabel(TEXT("Off")), FString(TEXT("Off")));
	TestEqual(TEXT("labels: 6X"), ShinobiGraphics::FrameGenLabel(TEXT("On6X")), FString(TEXT("6X")));
	TestEqual(TEXT("labels: Dynamic"), ShinobiGraphics::FrameGenLabel(TEXT("OnDynamic")), FString(TEXT("Dynamic")));
	TestFalse(TEXT("the report has the probe's findings"), ShinobiGraphics::Report().IsEmpty());

	// Resolution: listed under Screen mode, its index on its list (or none), locked in borderless.
	const int32 ResolutionIndex = ShinobiSettings::RowIndex(ERow::Resolution);
	TestTrue(TEXT("resolution: its index is on its list (or none)"), ResolutionIndex == INDEX_NONE || ShinobiSettings::RowOptions(ERow::Resolution).IsValidIndex(ResolutionIndex));
	TestTrue(TEXT("resolution: locked only in borderless"),
		ShinobiSettings::RowLock(ERow::Resolution).IsEmpty() == (Settings->GetFullscreenMode() != EWindowMode::WindowedFullscreen));

	Settings->SetVSyncEnabled(bOldVSync);
	Settings->SetDynamicResolutionEnabled(bOldDynamicRes);
	return true; // (Graphics puts the machine's picks back)
}

#endif // WITH_DEV_AUTOMATION_TESTS
