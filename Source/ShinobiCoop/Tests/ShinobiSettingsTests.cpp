#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameUserSettings.h"
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
	FShinobiTestWorld W;

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
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
