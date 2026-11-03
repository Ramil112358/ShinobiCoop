#include "Shinobi/UI/ShinobiSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
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
	const TCHAR* FaceKey = TEXT("Face");
	const TCHAR* SnowKey = TEXT("Snow");
	const TCHAR* SnowNames[] = { TEXT("Off"), TEXT("Light"), TEXT("Blizzard") };

	/** The game world this machine is playing in, if any (the settings panel is also open in the main menu). */
	UWorld* PlayingWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && Context.World()->IsGameWorld())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	UGameUserSettings* Settings()
	{
		return GEngine->GetGameUserSettings();
	}

	/** Index wrapped into [0, Num). */
	int32 SettingsWrap(int32 Index, int32 Num)
	{
		return Num > 0 ? ((Index % Num) + Num) % Num : 0;
	}

	/** Index of the value Delta steps from Current in Values (the first one, or going back the last, if Current isn't in
	 * the list). */
	template <typename T, int32 N>
	int32 StepIndex(const T (&Values)[N], T Current, int32 Delta)
	{
		for (int32 Index = 0; Index < N; ++Index)
		{
			if (FMath::IsNearlyEqual(double(Values[Index]), double(Current), 0.5))
			{
				return SettingsWrap(Index + Delta, N);
			}
		}
		return Delta >= 0 ? 0 : N - 1;
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

FString ShinobiSettings::GetSavedFace()
{
	FString Name;
	GConfig->GetString(ConfigSection, FaceKey, Name, GGameUserSettingsIni);
	return Name;
}

void ShinobiSettings::SetSavedFace(const FString& Name)
{
	GConfig->SetString(ConfigSection, FaceKey, *Name, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

EShinobiSnow ShinobiSettings::GetSavedSnow()
{
	int32 Value = 0;
	GConfig->GetInt(ConfigSection, SnowKey, Value, GGameUserSettingsIni);
	return EShinobiSnow(FMath::Clamp(Value, 0, int32(UE_ARRAY_COUNT(SnowNames)) - 1));
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

FString ShinobiSettings::RowName(ERow Row)
{
	switch (Row)
	{
	case ERow::Character: return TEXT("Character");
	case ERow::Face: return TEXT("Face");
	case ERow::Outfit: return TEXT("Outfit");
	case ERow::Quality: return TEXT("Graphics quality");
	case ERow::RenderScale: return TEXT("Render scale");
	case ERow::WindowMode: return TEXT("Window");
	case ERow::FrameCap: return TEXT("Frame cap");
	case ERow::VSync: return TEXT("VSync");
	case ERow::Outline: return TEXT("Character outline");
	case ERow::Snow: return TEXT("Snow weather");
	default: return FString();
	}
}

FString ShinobiSettings::RowValue(ERow Row)
{
	UGameUserSettings* S = Settings();
	switch (Row)
	{
	case ERow::Character:
		return GetSavedCharacter();
	case ERow::Face:
	{
		const int32 Body = AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter());
		const FShinobiFaceEntry* Face = AShinobiPlayerCharacter::FindFace(Body, FMath::Max(0, AShinobiPlayerCharacter::FindFaceIndex(Body, GetSavedFace())));
		return Face ? Face->Name : TEXT("Default");
	}
	case ERow::Outfit:
		return GetSavedOutfit();
	case ERow::Quality:
	{
		const int32 Level = S->GetOverallScalabilityLevel();
		return Level >= 0 && Level < UE_ARRAY_COUNT(QualityNames) ? QualityNames[Level] : TEXT("Custom");
	}
	case ERow::RenderScale:
		return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(GetRenderScale()));
	case ERow::WindowMode:
	{
		const EWindowMode::Type Mode = S->GetFullscreenMode();
		return Mode == EWindowMode::Fullscreen ? TEXT("Fullscreen") : Mode == EWindowMode::WindowedFullscreen ? TEXT("Borderless") : TEXT("Windowed");
	}
	case ERow::FrameCap:
	{
		const float Cap = S->GetFrameRateLimit();
		return Cap <= 0.f ? FString(TEXT("Unlimited")) : FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Cap));
	}
	case ERow::VSync:
		return S->IsVSyncEnabled() ? TEXT("On") : TEXT("Off");
	case ERow::Outline:
		return ShinobiLook::IsOutlineEnabled() ? TEXT("On") : TEXT("Off");
	case ERow::Snow:
	{
		// A client sees its own pick, kept for when it hosts; the session shows the host's.
		const UWorld* World = PlayingWorld();
		return FString::Printf(TEXT("%s%s"), SnowNames[uint8(GetSavedSnow())], World && World->GetNetMode() == NM_Client ? TEXT(" (host decides)") : TEXT(""));
	}
	default:
		return FString();
	}
}

FString ShinobiSettings::Label(ERow Row)
{
	return FString::Printf(TEXT("%s: %s"), *RowName(Row), *RowValue(Row));
}

FString ShinobiSettings::RowDescription(ERow Row)
{
	switch (Row)
	{
	case ERow::Character: return TEXT("Who you play. In a game you switch at once, and the pick is kept for the next one.");
	case ERow::Face: return TEXT("The face your character wears, on characters that have more than one.");
	case ERow::Outfit: return TEXT("What your character wears over their own clothes.");
	case ERow::Quality: return TEXT("The overall graphics preset. Lower it for more FPS. Custom means a single setting was changed elsewhere.");
	case ERow::RenderScale: return TEXT("How sharp the 3D scene is drawn, scaled to your screen. Lower values give more FPS but a softer picture.");
	case ERow::WindowMode: return TEXT("Exclusive fullscreen, a borderless window the size of the screen, or a normal window.");
	case ERow::FrameCap: return TEXT("The highest frame rate the game runs at. A cap saves power and keeps fans quiet.");
	case ERow::VSync: return TEXT("Matches frames to your monitor's refresh rate so the picture doesn't tear. Adds a little input delay.");
	case ERow::Outline: return TEXT("Draws a thin outline round characters so they stand out from the scenery.");
	case ERow::Snow: return TEXT("Snowfall in games you host. In someone else's game the host's pick applies to everyone.");
	default: return FString();
	}
}

FString ShinobiSettings::RowSection(ERow Row)
{
	switch (Row)
	{
	case ERow::Character:
	case ERow::Face:
	case ERow::Outfit:
		return TEXT("CHARACTER");
	case ERow::Quality:
	case ERow::RenderScale:
		return TEXT("GRAPHICS");
	case ERow::WindowMode:
	case ERow::FrameCap:
	case ERow::VSync:
		return TEXT("DISPLAY");
	default:
		return TEXT("EFFECTS");
	}
}

void ShinobiSettings::SetSavedCharacter(const FString& Name)
{
	const int32 Index = AShinobiPlayerCharacter::FindRosterIndex(Name);
	if (Index != INDEX_NONE)
	{
		GConfig->SetString(ConfigSection, CharacterKey, AShinobiPlayerCharacter::Roster[Index].Name, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		UShinobiCharacterSave* Made = UShinobiCharacterSave::Load(); // a roster pick: no made character is played
		if (Made->Active != INDEX_NONE)
		{
			Made->Active = INDEX_NONE;
			Made->Save();
		}
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
		if (!bMainMenu || (ERow(Row) != ERow::Character && ERow(Row) != ERow::Face && ERow(Row) != ERow::Outfit))
		{
			Rows.Add(ERow(Row));
		}
	}
	return Rows;
}

void ShinobiSettings::Cycle(ERow Row, UWorld* World)
{
	Step(Row, World, 1);
}

void ShinobiSettings::Step(ERow Row, UWorld* World, int32 Delta)
{
	UGameUserSettings* S = Settings();
	switch (Row)
	{
	case ERow::Character:
	{
		const int32 Next = SettingsWrap(AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter()) + Delta, UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster));
		const FString Picked = AShinobiPlayerCharacter::Roster[Next].Name;
		GConfig->SetString(ConfigSection, CharacterKey, *Picked, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		// In a game (not the main menu), switch now too.
		if (AShinobiPlayerController* PC = World ? World->GetFirstPlayerController<AShinobiPlayerController>() : nullptr)
		{
			PC->ShinobiCharacter(Picked);
		}
		return;
	}
	case ERow::Face:
	{
		const int32 Body = AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter());
		const int32 Faces = AShinobiPlayerCharacter::NumFaces(Body);
		const int32 Next = SettingsWrap(FMath::Max(0, AShinobiPlayerCharacter::FindFaceIndex(Body, GetSavedFace())) + Delta, Faces);
		const FShinobiFaceEntry* Face = Faces > 1 ? AShinobiPlayerCharacter::FindFace(Body, Next) : nullptr;
		if (!Face)
		{
			return; // one face only
		}
		SetSavedFace(Face->Name);
		if (AShinobiPlayerController* PC = World ? World->GetFirstPlayerController<AShinobiPlayerController>() : nullptr)
		{
			PC->ShinobiFace(Face->Name);
		}
		return;
	}
	case ERow::Outfit:
	{
		const int32 Next = SettingsWrap(AShinobiPlayerCharacter::FindOutfitIndex(GetSavedOutfit()) + Delta, UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits));
		const FString Picked = AShinobiPlayerCharacter::Outfits[Next].Name;
		GConfig->SetString(ConfigSection, OutfitKey, *Picked, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		if (AShinobiPlayerController* PC = World ? World->GetFirstPlayerController<AShinobiPlayerController>() : nullptr)
		{
			PC->ShinobiOutfit(Picked);
		}
		return;
	}
	case ERow::Quality:
	{
		const int32 Level = S->GetOverallScalabilityLevel(); // -1: Custom
		const int32 Count = UE_ARRAY_COUNT(QualityNames);
		S->SetOverallScalabilityLevel(Level < 0 ? (Delta >= 0 ? 0 : Count - 1) : SettingsWrap(Level + Delta, Count));
		break;
	}
	case ERow::RenderScale:
		SetRenderScale(RenderScales[StepIndex(RenderScales, GetRenderScale(), Delta)]);
		break;
	case ERow::WindowMode:
	{
		int32 Next = Delta >= 0 ? 0 : UE_ARRAY_COUNT(WindowModes) - 1;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(WindowModes); ++Index)
		{
			Next = WindowModes[Index] == S->GetFullscreenMode() ? SettingsWrap(Index + Delta, UE_ARRAY_COUNT(WindowModes)) : Next;
		}
		S->SetFullscreenMode(WindowModes[Next]);
		S->ApplyResolutionSettings(false);
		break;
	}
	case ERow::FrameCap:
		S->SetFrameRateLimit(FrameCaps[StepIndex(FrameCaps, S->GetFrameRateLimit(), Delta)]);
		break;
	case ERow::VSync:
		S->SetVSyncEnabled(!S->IsVSyncEnabled());
		break;
	case ERow::Outline:
		ShinobiLook::SetOutlineEnabled(World, !ShinobiLook::IsOutlineEnabled());
		return;
	case ERow::Snow:
	{
		const EShinobiSnow Next = EShinobiSnow(SettingsWrap(int32(GetSavedSnow()) + Delta, UE_ARRAY_COUNT(SnowNames)));
		GConfig->SetInt(ConfigSection, SnowKey, int32(Next), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		if (AShinobiGameState* Session = World ? World->GetGameState<AShinobiGameState>() : nullptr)
		{
			Session->SetSnow(Next); // the host changes it for everyone; on a client this does nothing
		}
		return;
	}
	default:
		return;
	}
	S->ApplyNonResolutionSettings();
	S->SaveSettings();
}
