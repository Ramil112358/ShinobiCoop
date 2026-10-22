#include "Shinobi/UI/ShinobiSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"

namespace
{
	const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	const float RenderScales[] = { 50.f, 67.f, 75.f, 100.f };
	const float FrameCaps[] = { 30.f, 60.f, 120.f, 144.f, 0.f }; // 0 = unlimited
	const EWindowMode::Type WindowModes[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };

	const TCHAR* ConfigSection = TEXT("/Script/ShinobiCoop.ShinobiSettings");
	const TCHAR* CharacterKey = TEXT("Character");
	const TCHAR* OutfitKey = TEXT("Outfit");

	UGameUserSettings* Settings()
	{
		return GEngine->GetGameUserSettings();
	}

	/** Index of the value after Current in Values (the first one if Current isn't in the list). */
	template <typename T, int32 N>
	int32 NextIndex(const T (&Values)[N], T Current)
	{
		for (int32 Index = 0; Index < N; ++Index)
		{
			if (FMath::IsNearlyEqual(double(Values[Index]), double(Current), 0.5))
			{
				return (Index + 1) % N;
			}
		}
		return 0;
	}
}

FString ShinobiSettings::GetSavedCharacter()
{
	FString Name;
	GConfig->GetString(ConfigSection, CharacterKey, Name, GGameUserSettingsIni);
	return AShinobiPlayerCharacter::FindRosterIndex(Name) == INDEX_NONE ? FString(AShinobiPlayerCharacter::Roster[0].Name) : Name;
}

FString ShinobiSettings::GetSavedOutfit()
{
	FString Name;
	GConfig->GetString(ConfigSection, OutfitKey, Name, GGameUserSettingsIni);
	return AShinobiPlayerCharacter::FindOutfitIndex(Name) == INDEX_NONE ? FString(AShinobiPlayerCharacter::Outfits[0].Name) : Name;
}

float ShinobiSettings::GetRenderScale()
{
	float Current = 100.f, Min = 0.f, Max = 0.f, Default = 100.f;
	Settings()->GetResolutionScaleInformationEx(Default, Current, Min, Max);
	return Current;
}

void ShinobiSettings::SetRenderScale(float Percent)
{
	Settings()->SetResolutionScaleValueEx(Percent);
}

FString ShinobiSettings::Label(ERow Row)
{
	UGameUserSettings* S = Settings();
	switch (Row)
	{
	case ERow::Character:
		return FString::Printf(TEXT("Character: %s"), *GetSavedCharacter());
	case ERow::Outfit:
		return FString::Printf(TEXT("Outfit: %s"), *GetSavedOutfit());
	case ERow::Quality:
	{
		const int32 Level = S->GetOverallScalabilityLevel();
		return FString::Printf(TEXT("Graphics quality: %s"), Level >= 0 && Level < UE_ARRAY_COUNT(QualityNames) ? QualityNames[Level] : TEXT("Custom"));
	}
	case ERow::RenderScale:
		return FString::Printf(TEXT("Render scale: %d%%"), FMath::RoundToInt(GetRenderScale()));
	case ERow::WindowMode:
	{
		const EWindowMode::Type Mode = S->GetFullscreenMode();
		return FString::Printf(TEXT("Window: %s"), Mode == EWindowMode::Fullscreen ? TEXT("Fullscreen")
			: Mode == EWindowMode::WindowedFullscreen ? TEXT("Borderless") : TEXT("Windowed"));
	}
	case ERow::FrameCap:
	{
		const float Cap = S->GetFrameRateLimit();
		return Cap <= 0.f ? FString(TEXT("Frame cap: Unlimited")) : FString::Printf(TEXT("Frame cap: %d"), FMath::RoundToInt(Cap));
	}
	case ERow::VSync:
		return FString::Printf(TEXT("VSync: %s"), S->IsVSyncEnabled() ? TEXT("On") : TEXT("Off"));
	case ERow::Outline:
		return FString::Printf(TEXT("Character outline: %s"), ShinobiLook::IsOutlineEnabled() ? TEXT("On") : TEXT("Off"));
	default:
		return FString();
	}
}

void ShinobiSettings::SetSavedCharacter(const FString& Name)
{
	const int32 Index = AShinobiPlayerCharacter::FindRosterIndex(Name);
	if (Index != INDEX_NONE)
	{
		GConfig->SetString(ConfigSection, CharacterKey, AShinobiPlayerCharacter::Roster[Index].Name, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void ShinobiSettings::SetSavedOutfit(const FString& Name)
{
	const int32 Index = AShinobiPlayerCharacter::FindOutfitIndex(Name);
	if (Index != INDEX_NONE)
	{
		GConfig->SetString(ConfigSection, OutfitKey, AShinobiPlayerCharacter::Outfits[Index].Name, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

TArray<ShinobiSettings::ERow> ShinobiSettings::RowsFor(bool bMainMenu)
{
	TArray<ERow> Rows;
	for (uint8 Row = 0; Row < uint8(ERow::Num); ++Row)
	{
		if (!bMainMenu || (ERow(Row) != ERow::Character && ERow(Row) != ERow::Outfit))
		{
			Rows.Add(ERow(Row));
		}
	}
	return Rows;
}

void ShinobiSettings::Cycle(ERow Row, UWorld* World)
{
	UGameUserSettings* S = Settings();
	switch (Row)
	{
	case ERow::Character:
	{
		const int32 Next = (AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter()) + 1) % UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster);
		const FString Name = AShinobiPlayerCharacter::Roster[Next].Name;
		GConfig->SetString(ConfigSection, CharacterKey, *Name, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		// In a game (not the main menu), switch now too.
		if (AShinobiPlayerController* PC = World ? World->GetFirstPlayerController<AShinobiPlayerController>() : nullptr)
		{
			PC->ShinobiCharacter(Name);
		}
		return;
	}
	case ERow::Outfit:
	{
		const int32 Next = (AShinobiPlayerCharacter::FindOutfitIndex(GetSavedOutfit()) + 1) % UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits);
		const FString Name = AShinobiPlayerCharacter::Outfits[Next].Name;
		GConfig->SetString(ConfigSection, OutfitKey, *Name, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		if (AShinobiPlayerController* PC = World ? World->GetFirstPlayerController<AShinobiPlayerController>() : nullptr)
		{
			PC->ShinobiOutfit(Name);
		}
		return;
	}
	case ERow::Quality:
		S->SetOverallScalabilityLevel((FMath::Max(S->GetOverallScalabilityLevel(), -1) + 1) % UE_ARRAY_COUNT(QualityNames));
		break;
	case ERow::RenderScale:
		SetRenderScale(RenderScales[NextIndex(RenderScales, GetRenderScale())]);
		break;
	case ERow::WindowMode:
	{
		int32 Next = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(WindowModes); ++Index)
		{
			Next = WindowModes[Index] == S->GetFullscreenMode() ? (Index + 1) % UE_ARRAY_COUNT(WindowModes) : Next;
		}
		S->SetFullscreenMode(WindowModes[Next]);
		S->ApplyResolutionSettings(false);
		break;
	}
	case ERow::FrameCap:
		S->SetFrameRateLimit(FrameCaps[NextIndex(FrameCaps, S->GetFrameRateLimit())]);
		break;
	case ERow::VSync:
		S->SetVSyncEnabled(!S->IsVSyncEnabled());
		break;
	case ERow::Outline:
		ShinobiLook::SetOutlineEnabled(World, !ShinobiLook::IsOutlineEnabled());
		return;
	default:
		return;
	}
	S->ApplyNonResolutionSettings();
	S->SaveSettings();
}
