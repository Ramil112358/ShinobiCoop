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

#endif // WITH_DEV_AUTOMATION_TESTS
