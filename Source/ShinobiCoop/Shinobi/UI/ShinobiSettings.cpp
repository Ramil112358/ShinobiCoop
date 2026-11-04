#include "Shinobi/UI/ShinobiSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/UI/ShinobiGraphics.h"

namespace
{
	const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	// Lighting quality: the global illumination and reflections scalability groups together. In 5.8 Medium is Lumen Lite.
	const TCHAR* LightingNames[] = { TEXT("Low"), TEXT("Medium (Lumen Lite)"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
	// Upscaling (ShinobiGraphics): the method, its quality (the vendors' mode names), and native anti-aliasing while it's off.
	const TCHAR* UpscalerNames[] = { TEXT("Off"), TEXT("TSR"), TEXT("NVIDIA DLSS") }; // (ShinobiGraphics::EUpscaler's order)
	const TCHAR* UpscalingQualityNames[] = { TEXT("Quality"), TEXT("Balanced"), TEXT("Performance"), TEXT("Ultra Performance") };
	static_assert(int32(UE_ARRAY_COUNT(UpscalingQualityNames)) == ShinobiGraphics::UpscalingQualities);
	const TCHAR* AntiAliasingNames[] = { TEXT("TSR"), TEXT("NVIDIA DLAA") }; // (ShinobiGraphics::EAntiAliasing's order)
	/** Resolutions smaller than this aren't offered. */
	const FIntPoint MinResolution(1280, 720);
	const TCHAR* SharpnessNames[] = { TEXT("Off"), TEXT("Low"), TEXT("Medium"), TEXT("High") };
	static_assert(int32(UE_ARRAY_COUNT(SharpnessNames)) == ShinobiGraphics::SharpnessLevels);
	const TCHAR* ReflexNames[] = { TEXT("Off"), TEXT("On"), TEXT("On + Boost") };
	static_assert(int32(UE_ARRAY_COUNT(ReflexNames)) == ShinobiGraphics::ReflexModes);
	const TCHAR* VSyncFrameGenLock = TEXT("Off while DLSS Frame Generation is on: it paces its own frames.");

	FString ResolutionText(const FIntPoint& Resolution)
	{
		return FString::Printf(TEXT("%d \u00D7 %d"), Resolution.X, Resolution.Y);
	}

	const TCHAR* OnOff(bool bOn)
	{
		return bOn ? TEXT("On") : TEXT("Off");
	}
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

	constexpr int32 SettingsRosterCount = UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster);

	/** The made character played (its slot in UShinobiCharacterSave), or INDEX_NONE when it's the roster pick. */
	int32 SettingsPlayedMade(const UShinobiCharacterSave& Made)
	{
		return Made.GetActiveLook() && !Made.Slots[Made.Active].Name.IsEmpty() ? Made.Active : INDEX_NONE;
	}

	/** Index wrapped into [0, Num). */
	int32 SettingsWrap(int32 Index, int32 Num)
	{
		return Num > 0 ? ((Index % Num) + Num) % Num : 0;
	}

	/** The resolutions the Resolution row offers, largest first: the monitor's fullscreen modes (windowed: the sizes that fit
	 * the desktop), 1280 x 720 and up, plus the current one. */
	TArray<FIntPoint> ResolutionList()
	{
		UGameUserSettings* S = Settings();
		TArray<FIntPoint> Found;
		if (S->GetFullscreenMode() == EWindowMode::Windowed)
		{
			UKismetSystemLibrary::GetConvenientWindowedResolutions(Found);
		}
		else
		{
			UKismetSystemLibrary::GetSupportedFullscreenResolutions(Found);
		}
		TArray<FIntPoint> List;
		for (const FIntPoint& Resolution : Found)
		{
			if (Resolution.X >= MinResolution.X && Resolution.Y >= MinResolution.Y)
			{
				List.AddUnique(Resolution);
			}
		}
		// The one in use is always listed, so the row shows one of its options (borderless: the desktop's).
		List.AddUnique(S->GetFullscreenMode() == EWindowMode::WindowedFullscreen ? S->GetDesktopResolution() : S->GetScreenResolution());
		List.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X > B.X : A.Y > B.Y; });
		return List;
	}

	/** The resolution shown and picked: the desktop's in borderless (it always covers the screen), else the saved one. */
	FIntPoint ShownResolution()
	{
		UGameUserSettings* S = Settings();
		return S->GetFullscreenMode() == EWindowMode::WindowedFullscreen ? S->GetDesktopResolution() : S->GetScreenResolution();
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

int32 ShinobiSettings::GetQualityPreset()
{
	const UGameUserSettings* S = Settings();
	const int32 Groups[] = { S->GetViewDistanceQuality(), S->GetAntiAliasingQuality(), S->GetShadowQuality(), S->GetGlobalIlluminationQuality(),
		S->GetReflectionQuality(), S->GetPostProcessingQuality(), S->GetTextureQuality(), S->GetVisualEffectQuality(), S->GetFoliageQuality(),
		S->GetShadingQuality() };
	for (const int32 Level : Groups)
	{
		if (Level != Groups[0])
		{
			return INDEX_NONE;
		}
	}
	return Groups[0] >= 0 && Groups[0] < UE_ARRAY_COUNT(QualityNames) ? Groups[0] : INDEX_NONE;
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
	case ERow::LightingQuality: return TEXT("Lighting quality");
	case ERow::MegaLights: return TEXT("MegaLights");
	case ERow::Upscaler: return TEXT("Upscaling method");
	case ERow::RenderScale: return TEXT("Upscaling quality");
	case ERow::AntiAliasing: return TEXT("Anti-aliasing");
	case ERow::Sharpness: return TEXT("Sharpness");
	case ERow::DynamicResolution: return TEXT("Dynamic resolution");
	case ERow::FrameGeneration: return TEXT("DLSS Frame Generation");
	case ERow::LowLatency: return TEXT("NVIDIA Reflex Low Latency");
	case ERow::RayTracing: return TEXT("Ray tracing");
	case ERow::RayReconstruction: return TEXT("DLSS Ray Reconstruction");
	case ERow::WindowMode: return TEXT("Screen mode");
	case ERow::Resolution: return TEXT("Resolution");
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
	{
		const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
		const int32 Played = SettingsPlayedMade(*Made);
		return Played != INDEX_NONE ? Made->Slots[Played].Name : GetSavedCharacter();
	}
	case ERow::Face:
	{
		if (SettingsPlayedMade(*UShinobiCharacterSave::Load()) != INDEX_NONE)
		{
			return TEXT("Default"); // (a made character's face is its own, set in the creator)
		}
		const int32 Body = AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter());
		const FShinobiFaceEntry* Face = AShinobiPlayerCharacter::FindFace(Body, FMath::Max(0, AShinobiPlayerCharacter::FindFaceIndex(Body, GetSavedFace())));
		return Face ? Face->Name : TEXT("Default");
	}
	case ERow::Outfit:
		return GetSavedOutfit();
	case ERow::Quality:
	{
		const int32 Level = GetQualityPreset();
		return Level != INDEX_NONE ? QualityNames[Level] : TEXT("Custom");
	}
	case ERow::LightingQuality:
		return LightingNames[RowIndex(Row)];
	case ERow::MegaLights:
		return OnOff(ShinobiGraphics::IsMegaLights());
	case ERow::Upscaler:
		return UpscalerNames[uint8(ShinobiGraphics::GetUpscaler())];
	case ERow::RenderScale:
		return UpscalingQualityNames[ShinobiGraphics::GetUpscalingQuality()]; // (kept, greyed out, while upscaling is off)
	case ERow::AntiAliasing:
		return AntiAliasingNames[uint8(ShinobiGraphics::GetAntiAliasing())]; // (kept, greyed out, while upscaling)
	case ERow::Sharpness:
		return SharpnessNames[ShinobiGraphics::GetSharpness()];
	case ERow::DynamicResolution:
		return OnOff(S->IsDynamicResolutionEnabled());
	case ERow::RayTracing:
		return OnOff(ShinobiGraphics::IsRayTracing());
	case ERow::RayReconstruction:
		return OnOff(ShinobiGraphics::IsRayReconstruction());
	case ERow::FrameGeneration:
		return ShinobiGraphics::FrameGenOptions()[ShinobiGraphics::GetFrameGen()];
	case ERow::LowLatency:
		return ReflexNames[ShinobiGraphics::GetReflexMode()];
	case ERow::WindowMode:
	{
		const EWindowMode::Type Mode = S->GetFullscreenMode();
		return Mode == EWindowMode::Fullscreen ? TEXT("Fullscreen") : Mode == EWindowMode::WindowedFullscreen ? TEXT("Borderless") : TEXT("Windowed");
	}
	case ERow::Resolution:
		return ResolutionText(ShownResolution());
	case ERow::FrameCap:
	{
		const float Cap = S->GetFrameRateLimit();
		return Cap <= 0.f ? FString(TEXT("Unlimited")) : FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Cap));
	}
	case ERow::VSync:
		return OnOff(S->IsVSyncEnabled() && !ShinobiGraphics::IsFrameGenOn());
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
	case ERow::Character: return TEXT("Who you play: the roster, then the characters you made. In a game you switch at once, and the pick is kept for the next one.");
	case ERow::Face: return TEXT("The face your character wears, on characters that have more than one.");
	case ERow::Outfit: return TEXT("What your character wears over their own clothes.");
	case ERow::Quality: return TEXT("The overall graphics preset. Lower it for more FPS. Custom means a single setting was changed elsewhere.");
	case ERow::LightingQuality: return TEXT("Bounce light and reflections (Lumen). Medium is Lumen Lite: about half the cost of High, with softer, simpler bounce light; the biggest single FPS win. Low turns Lumen off. Graphics quality sets this too.");
	case ERow::MegaLights: return TEXT("Lights from jutsu, fires and chakra cast soft shadows, shaded by Unreal's MegaLights at a fixed cost however many there are. Needs hardware ray tracing (DirectX 12); greyed out otherwise.");
	case ERow::Upscaler: return TEXT("Draws the scene below your screen's resolution and rebuilds it sharp, for more FPS. TSR is Unreal's own and runs on any card. NVIDIA DLSS uses a GeForce RTX card's AI cores and usually looks sharper; it's greyed out in the list without one. Off draws every pixel.");
	case ERow::RenderScale: return TEXT("How much the upscaler upscales. Quality looks closest to native, Ultra Performance gives the most FPS. Greyed out while Upscaling method is Off (it's kept for when you turn upscaling back on).");
	case ERow::AntiAliasing: return TEXT("Smooths jagged edges at native resolution. NVIDIA DLAA (DLSS without upscaling, GeForce RTX only) gives the sharpest, most stable image for a little more GPU time than TSR. Greyed out while upscaling, because the upscaler smooths edges itself; your pick applies again when Upscaling method is Off.");
	case ERow::Sharpness: return TEXT("Sharpens the final picture. Raise it if the image looks soft at a lower upscaling quality.");
	case ERow::DynamicResolution: return TEXT("Lowers the resolution for a moment when a big jutsu would drop the frame rate below your frame cap (60 FPS without one), then raises it again.");
	case ERow::FrameGeneration: return TEXT("AI adds frames between the rendered ones: 2X doubles the frame rate, 3X and up multiply it further, Dynamic picks the multiplier for your monitor. Smoother motion, slightly more input delay; best from 60 FPS up. Turns VSync off and NVIDIA Reflex on. Needs a GeForce RTX 40 Series card or newer (3X and up, and Dynamic: RTX 50 Series); greyed out otherwise.");
	case ERow::LowLatency: return TEXT("Cuts the delay between your input and the screen. On + Boost also keeps the GPU clocks up for the lowest latency, at more power. Needs an NVIDIA GeForce card; greyed out otherwise. Off is greyed out while DLSS Frame Generation is on, which needs it.");
	case ERow::RayTracing: return TEXT("Lights and reflects the world with your GPU's ray-tracing cores: more accurate bounce light and reflections on water and glass. Costs FPS. Needs a graphics card with hardware ray tracing (DirectX 12); greyed out otherwise.");
	case ERow::RayReconstruction: return TEXT("DLSS's AI cleans up ray-traced light and reflections: sharper and more stable than the usual denoiser. Works with NVIDIA DLSS or DLAA and Ray tracing, so turning it on turns those on too. Needs a GeForce RTX card; greyed out otherwise.");
	case ERow::WindowMode: return TEXT("Exclusive fullscreen, a borderless window the size of the screen, or a normal window.");
	case ERow::Resolution: return TEXT("The resolution of the game's screen or window. Your monitor's native resolution looks sharpest: for more FPS, use upscaling instead of lowering it. Greyed out in Borderless, which always covers the screen at your desktop's resolution.");
	case ERow::FrameCap: return TEXT("The highest frame rate the game runs at. A cap saves power and keeps fans quiet.");
	case ERow::VSync: return TEXT("Matches frames to your monitor's refresh rate so the picture doesn't tear. Adds a little input delay. Greyed out (off) while DLSS Frame Generation is on: it paces its own frames.");
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
	case ERow::LightingQuality:
		return TEXT("GRAPHICS");
	case ERow::Upscaler:
	case ERow::RenderScale:
	case ERow::AntiAliasing:
	case ERow::Sharpness:
	case ERow::DynamicResolution:
	case ERow::FrameGeneration:
	case ERow::LowLatency:
		return TEXT("UPSCALING & FRAME GENERATION");
	case ERow::RayTracing:
	case ERow::RayReconstruction:
	case ERow::MegaLights:
		return TEXT("RAY TRACING");
	case ERow::WindowMode:
	case ERow::Resolution:
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

FString ShinobiSettings::RowLock(ERow Row)
{
	const ShinobiGraphics::FCaps& Caps = ShinobiGraphics::Caps();
	switch (Row)
	{
	case ERow::FrameGeneration:
		return Caps.HasFrameGen() ? FString() : (Caps.FrameGenLock.IsEmpty() ? FString(TEXT("Not supported on this PC.")) : Caps.FrameGenLock);
	case ERow::LowLatency:
		return Caps.bReflex ? FString() : Caps.ReflexLock;
	case ERow::RayTracing:
		return Caps.bRayTracing ? FString() : Caps.RayTracingLock;
	case ERow::RayReconstruction:
		return Caps.bRayReconstruction ? FString() : Caps.RayReconstructionLock;
	case ERow::MegaLights:
		return Caps.bMegaLights ? FString() : Caps.MegaLightsLock;
	case ERow::VSync:
		return ShinobiGraphics::IsFrameGenOn() ? FString(VSyncFrameGenLock) : FString();
	case ERow::RenderScale:
		return ShinobiGraphics::GetUpscaler() == ShinobiGraphics::EUpscaler::Off ? FString(TEXT("Upscaling is off: the scene is drawn at your screen's resolution.")) : FString();
	case ERow::AntiAliasing:
		return ShinobiGraphics::GetUpscaler() == ShinobiGraphics::EUpscaler::Off ? FString()
			: FString(TEXT("The upscaler smooths edges itself. Set Upscaling method to Off to pick anti-aliasing."));
	case ERow::Resolution:
		return Settings()->GetFullscreenMode() == EWindowMode::WindowedFullscreen
			? FString(TEXT("Borderless always covers the screen at your desktop's resolution. Pick Fullscreen or Windowed to change it."))
			: FString();
	default:
		return FString();
	}
}

FString ShinobiSettings::OptionLock(ERow Row, int32 Option)
{
	if (const FString Locked = RowLock(Row); !Locked.IsEmpty())
	{
		return Locked;
	}
	const ShinobiGraphics::FCaps& Caps = ShinobiGraphics::Caps();
	switch (Row)
	{
	case ERow::Upscaler:
		return Option == int32(ShinobiGraphics::EUpscaler::DLSS) && !Caps.bDLSS ? Caps.DLSSLock : FString();
	case ERow::AntiAliasing:
		return Option == int32(ShinobiGraphics::EAntiAliasing::DLAA) && !Caps.bDLSS ? Caps.DLSSLock : FString();
	case ERow::FrameGeneration:
		return ShinobiGraphics::FrameGenOptionLock(Option);
	case ERow::LowLatency:
		return Option == 0 && ShinobiGraphics::IsFrameGenOn() ? FString(TEXT("DLSS Frame Generation needs NVIDIA Reflex on.")) : FString();
	default:
		return FString();
	}
}

TArray<FString> ShinobiSettings::OptionLocks(ERow Row)
{
	TArray<FString> Locks;
	const int32 Count = RowOptions(Row).Num();
	for (int32 Option = 0; Option < Count; ++Option)
	{
		Locks.Add(OptionLock(Row, Option));
	}
	return Locks;
}

FString ShinobiSettings::RowNote(ERow Row)
{
	if (const FString Locked = RowLock(Row); !Locked.IsEmpty())
	{
		return FString::Printf(TEXT("Locked: %s"), *Locked);
	}
	// Locked options, grouped by reason: "3X, 4X: Requires a GeForce RTX 50 Series graphics card."
	const TArray<FString> Options = RowOptions(Row);
	TArray<FString> Reasons;
	TArray<TArray<FString>> Names;
	for (int32 Option = 0; Option < Options.Num(); ++Option)
	{
		const FString Locked = OptionLock(Row, Option);
		if (!Locked.IsEmpty())
		{
			const int32 Reason = Reasons.AddUnique(Locked);
			Names.SetNum(Reasons.Num());
			Names[Reason].Add(Options[Option]);
		}
	}
	TArray<FString> Lines;
	for (int32 Reason = 0; Reason < Reasons.Num(); ++Reason)
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"), *FString::Join(Names[Reason], TEXT(", ")), *Reasons[Reason]));
	}
	return FString::Join(Lines, TEXT("\n"));
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

TArray<FString> ShinobiSettings::RowOptions(ERow Row)
{
	TArray<FString> Options;
	switch (Row)
	{
	case ERow::Character:
	{
		for (const FShinobiRosterEntry& Entry : AShinobiPlayerCharacter::Roster)
		{
			Options.Add(Entry.Name);
		}
		const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load(); // then the made ones (the SHINOBI tab's cards)
		for (const int32 SlotIndex : Made->GetMadeSlots())
		{
			Options.Add(Made->Slots[SlotIndex].Name);
		}
		break;
	}
	case ERow::Face:
	{
		if (SettingsPlayedMade(*UShinobiCharacterSave::Load()) != INDEX_NONE)
		{
			Options.Add(TEXT("Default")); // (nothing to pick)
			break;
		}
		const int32 Body = AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter());
		for (int32 Index = 0; Index < AShinobiPlayerCharacter::NumFaces(Body); ++Index)
		{
			const FShinobiFaceEntry* Face = AShinobiPlayerCharacter::FindFace(Body, Index);
			Options.Add(Face ? Face->Name : TEXT("Default"));
		}
		break;
	}
	case ERow::Outfit:
		for (const FShinobiOutfitEntry& Entry : AShinobiPlayerCharacter::Outfits)
		{
			Options.Add(Entry.Name);
		}
		break;
	case ERow::Quality:
		for (const TCHAR* Name : QualityNames)
		{
			Options.Add(Name);
		}
		break;
	case ERow::LightingQuality:
		for (const TCHAR* Name : LightingNames)
		{
			Options.Add(Name);
		}
		break;
	case ERow::Upscaler:
		for (const TCHAR* Name : UpscalerNames)
		{
			Options.Add(Name);
		}
		break;
	case ERow::RenderScale:
		for (const TCHAR* Name : UpscalingQualityNames)
		{
			Options.Add(Name);
		}
		break;
	case ERow::AntiAliasing:
		for (const TCHAR* Name : AntiAliasingNames)
		{
			Options.Add(Name);
		}
		break;
	case ERow::Resolution:
		for (const FIntPoint& Resolution : ResolutionList())
		{
			Options.Add(ResolutionText(Resolution));
		}
		break;
	case ERow::Sharpness:
		for (const TCHAR* Name : SharpnessNames)
		{
			Options.Add(Name);
		}
		break;
	case ERow::LowLatency:
		for (const TCHAR* Name : ReflexNames)
		{
			Options.Add(Name);
		}
		break;
	case ERow::FrameGeneration:
		Options = ShinobiGraphics::FrameGenOptions();
		break;
	case ERow::WindowMode:
		Options = { TEXT("Fullscreen"), TEXT("Borderless"), TEXT("Windowed") }; // (WindowModes' order)
		break;
	case ERow::FrameCap:
		for (const float Cap : FrameCaps)
		{
			Options.Add(Cap <= 0.f ? FString(TEXT("Unlimited")) : FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Cap)));
		}
		break;
	case ERow::DynamicResolution:
	case ERow::RayTracing:
	case ERow::RayReconstruction:
	case ERow::MegaLights:
	case ERow::VSync:
	case ERow::Outline:
		Options = { TEXT("Off"), TEXT("On") };
		break;
	case ERow::Snow:
		for (const TCHAR* Name : SnowNames)
		{
			Options.Add(Name);
		}
		break;
	default:
		break;
	}
	return Options;
}

int32 ShinobiSettings::RowIndex(ERow Row)
{
	UGameUserSettings* S = Settings();
	const auto Find = [](const auto& Values, auto Current)
	{
		for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Values)); ++Index)
		{
			if (FMath::IsNearlyEqual(double(Values[Index]), double(Current), 0.5))
			{
				return Index;
			}
		}
		return int32(INDEX_NONE);
	};
	switch (Row)
	{
	case ERow::Character:
	{
		const UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
		const int32 Played = SettingsPlayedMade(*Made);
		return Played != INDEX_NONE ? SettingsRosterCount + Made->GetMadeSlots().IndexOfByKey(Played) : AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter());
	}
	case ERow::Face:
		if (SettingsPlayedMade(*UShinobiCharacterSave::Load()) != INDEX_NONE)
		{
			return 0;
		}
		return FMath::Max(0, AShinobiPlayerCharacter::FindFaceIndex(AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter()), GetSavedFace()));
	case ERow::Outfit:
		return AShinobiPlayerCharacter::FindOutfitIndex(GetSavedOutfit());
	case ERow::Quality:
		return GetQualityPreset(); // (Custom: none of them)
	case ERow::LightingQuality:
		return FMath::Clamp(S->GetGlobalIlluminationQuality(), 0, int32(UE_ARRAY_COUNT(LightingNames)) - 1);
	case ERow::MegaLights:
		return ShinobiGraphics::IsMegaLights() ? 1 : 0;
	case ERow::Upscaler:
		return int32(ShinobiGraphics::GetUpscaler());
	case ERow::RenderScale:
		return ShinobiGraphics::GetUpscalingQuality();
	case ERow::AntiAliasing:
		return int32(ShinobiGraphics::GetAntiAliasing());
	case ERow::Resolution:
		return ResolutionList().IndexOfByKey(ShownResolution());
	case ERow::DynamicResolution:
		return S->IsDynamicResolutionEnabled() ? 1 : 0;
	case ERow::RayTracing:
		return ShinobiGraphics::IsRayTracing() ? 1 : 0;
	case ERow::RayReconstruction:
		return ShinobiGraphics::IsRayReconstruction() ? 1 : 0;
	case ERow::FrameGeneration:
		return ShinobiGraphics::GetFrameGen();
	case ERow::Sharpness:
		return ShinobiGraphics::GetSharpness();
	case ERow::LowLatency:
		return ShinobiGraphics::GetReflexMode();
	case ERow::WindowMode:
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(WindowModes); ++Index)
		{
			if (WindowModes[Index] == S->GetFullscreenMode())
			{
				return Index;
			}
		}
		return INDEX_NONE;
	case ERow::FrameCap:
		return Find(FrameCaps, S->GetFrameRateLimit());
	case ERow::VSync:
		return S->IsVSyncEnabled() && !ShinobiGraphics::IsFrameGenOn() ? 1 : 0;
	case ERow::Outline:
		return ShinobiLook::IsOutlineEnabled() ? 1 : 0;
	case ERow::Snow:
		return int32(GetSavedSnow());
	default:
		return INDEX_NONE;
	}
}

bool ShinobiSettings::UsesDropdown(ERow Row)
{
	return Row == ERow::AntiAliasing || RowOptions(Row).Num() > 2; // (anti-aliasing: so a locked DLAA shows, greyed out, in its list)
}

void ShinobiSettings::Step(ERow Row, UWorld* World, int32 Delta)
{
	const int32 Count = RowOptions(Row).Num();
	if (Count < 2 || !RowLock(Row).IsEmpty())
	{
		return; // nothing to change to (a character with one face), or locked
	}
	const int32 Current = RowIndex(Row);
	// A value off the list (Custom quality, a render scale set elsewhere) steps to the first one, or back to the last.
	int32 Next = Current == INDEX_NONE ? (Delta >= 0 ? 0 : Count - 1) : SettingsWrap(Current + Delta, Count);
	// Locked options are stepped over (DLSS without an RTX card, 4X frame generation on an RTX 40...).
	const int32 Direction = Delta >= 0 ? 1 : -1;
	for (int32 Tries = 0; Tries < Count && !OptionLock(Row, Next).IsEmpty(); ++Tries)
	{
		Next = SettingsWrap(Next + Direction, Count);
	}
	if (Next != Current && OptionLock(Row, Next).IsEmpty())
	{
		SetRowIndex(Row, World, Next);
	}
}

void ShinobiSettings::SetRowIndex(ERow Row, UWorld* World, int32 Index)
{
	if (!RowOptions(Row).IsValidIndex(Index) || !OptionLock(Row, Index).IsEmpty())
	{
		return; // (OptionLock covers a locked row too)
	}
	UGameUserSettings* S = Settings();
	switch (Row)
	{
	case ERow::Character:
	{
		AShinobiPlayerController* PC = World ? World->GetFirstPlayerController<AShinobiPlayerController>() : nullptr;
		if (Index < SettingsRosterCount)
		{
			const FString Picked = AShinobiPlayerCharacter::Roster[Index].Name;
			SetSavedCharacter(Picked); // (no made character is played from now on)
			if (PC)
			{
				PC->ShinobiCharacter(Picked); // in a game (not the main menu), switch now too
			}
			return;
		}
		// A made character: played from now on, like its card in the SHINOBI tab.
		UShinobiCharacterSave* Made = UShinobiCharacterSave::Load();
		const TArray<int32> MadeSlots = Made->GetMadeSlots();
		const int32 SlotIndex = MadeSlots.IsValidIndex(Index - SettingsRosterCount) ? MadeSlots[Index - SettingsRosterCount] : INDEX_NONE;
		if (SlotIndex == INDEX_NONE)
		{
			return;
		}
		Made->Active = SlotIndex;
		Made->Save();
		if (PC)
		{
			PC->ServerPickAppearance(Made->Slots[SlotIndex].Look);
		}
		return;
	}
	case ERow::Face:
	{
		if (SettingsPlayedMade(*UShinobiCharacterSave::Load()) != INDEX_NONE)
		{
			return; // (a made character's face is its own)
		}
		const FShinobiFaceEntry* Face = AShinobiPlayerCharacter::FindFace(AShinobiPlayerCharacter::FindRosterIndex(GetSavedCharacter()), Index);
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
		const FString Picked = AShinobiPlayerCharacter::Outfits[Index].Name;
		GConfig->SetString(ConfigSection, OutfitKey, *Picked, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		if (AShinobiPlayerController* PC = World ? World->GetFirstPlayerController<AShinobiPlayerController>() : nullptr)
		{
			PC->ShinobiOutfit(Picked);
		}
		return;
	}
	case ERow::Quality:
		S->SetOverallScalabilityLevel(Index);
		break;
	case ERow::LightingQuality:
		S->SetGlobalIlluminationQuality(Index); // (Medium: Lumen Lite in 5.8, for both)
		S->SetReflectionQuality(Index);
		break;
	case ERow::MegaLights:
		ShinobiGraphics::SetMegaLights(Index == 1);
		return;
	case ERow::Upscaler:
		ShinobiGraphics::SetUpscaler(ShinobiGraphics::EUpscaler(Index));
		return;
	case ERow::RenderScale:
		ShinobiGraphics::SetUpscalingQuality(Index);
		return;
	case ERow::AntiAliasing:
		ShinobiGraphics::SetAntiAliasing(ShinobiGraphics::EAntiAliasing(Index));
		return;
	case ERow::Resolution:
	{
		const TArray<FIntPoint> Resolutions = ResolutionList();
		S->SetScreenResolution(Resolutions[Index]);
		S->ApplyResolutionSettings(false);
		break;
	}
	case ERow::DynamicResolution:
		S->SetDynamicResolutionEnabled(Index == 1);
		break;
	case ERow::RayTracing:
		ShinobiGraphics::SetRayTracing(Index == 1);
		return;
	case ERow::RayReconstruction:
		ShinobiGraphics::SetRayReconstruction(Index == 1);
		return;
	case ERow::FrameGeneration:
		ShinobiGraphics::SetFrameGen(Index); // (VSync off, Reflex on)
		return;
	case ERow::Sharpness:
		ShinobiGraphics::SetSharpness(Index);
		return;
	case ERow::LowLatency:
		ShinobiGraphics::SetReflexMode(Index);
		return;
	case ERow::WindowMode:
		S->SetFullscreenMode(WindowModes[Index]);
		if (WindowModes[Index] == EWindowMode::WindowedFullscreen)
		{
			S->SetScreenResolution(S->GetDesktopResolution()); // (borderless covers the screen)
		}
		S->ApplyResolutionSettings(false);
		break;
	case ERow::FrameCap:
		S->SetFrameRateLimit(FrameCaps[Index]);
		break;
	case ERow::VSync:
		S->SetVSyncEnabled(Index == 1); // (locked off while frame generation is on: RowLock)
		break;
	case ERow::Outline:
		if ((Index == 1) != ShinobiLook::IsOutlineEnabled())
		{
			ShinobiLook::SetOutlineEnabled(World, Index == 1);
		}
		return;
	case ERow::Snow:
	{
		const EShinobiSnow Next = EShinobiSnow(Index);
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
	ShinobiGraphics::ApplySaved(); // (the dynamic resolution budget follows the frame cap, DLAA the upscaling quality)
}
