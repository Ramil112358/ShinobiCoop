#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiSettings.h"

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

	ShinobiSettings::SetRenderScale(50.f);
	ShinobiSettings::Cycle(ShinobiSettings::ERow::RenderScale, W.World);
	TestEqual(TEXT("Render scale 50 -> 67"), FMath::RoundToInt(ShinobiSettings::GetRenderScale()), 67);

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

	ShinobiSettings::SetRenderScale(50.f);
	ShinobiSettings::Step(ERow::RenderScale, W.World, -1);
	TestEqual(TEXT("Render scale 50, back -> 100"), FMath::RoundToInt(ShinobiSettings::GetRenderScale()), 100);

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
	TestTrue(TEXT("render scale: a dropdown"), ShinobiSettings::UsesDropdown(ERow::RenderScale));
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

#endif // WITH_DEV_AUTOMATION_TESTS
