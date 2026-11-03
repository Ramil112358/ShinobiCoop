#include "Shinobi/UI/ShinobiGraphics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "RenderUtils.h"
#include "Shinobi/UI/ShinobiSettings.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogShinobiGraphics, Log, All);

namespace
{
	const TCHAR* GraphicsSection = TEXT("/Script/ShinobiCoop.ShinobiSettings");
	const TCHAR* UpscalerKey = TEXT("Upscaler");
	const TCHAR* RayTracingKey = TEXT("bRayTracing");
	const TCHAR* RayReconstructionKey = TEXT("bRayReconstruction");
	const TCHAR* FrameGenKey = TEXT("FrameGeneration");
	const TCHAR* LowLatencyKey = TEXT("bLowLatency");

	// NVIDIA's plugin Blueprint libraries (UDLSSLibrary, UStreamlineLibraryDLSSG, UStreamlineLibraryReflex), by class name.
	const TCHAR* DLSSLibrary = TEXT("DLSSLibrary");
	const TCHAR* DLSSGLibrary = TEXT("StreamlineLibraryDLSSG");
	const TCHAR* ReflexLibrary = TEXT("StreamlineLibraryReflex");

	// Their console variables, and the engine's.
	const TCHAR* CVarDLSS = TEXT("r.NGX.DLSS.Enable");
	const TCHAR* CVarDLAA = TEXT("r.NGX.DLAA.Enable");
	const TCHAR* CVarDLSSDenoiser = TEXT("r.NGX.DLSS.DenoiserMode"); // 1: Ray Reconstruction
	const TCHAR* CVarDLSSG = TEXT("r.Streamline.DLSSG.Enable");
	const TCHAR* CVarReflex = TEXT("t.Streamline.Reflex.Enable");
	const TCHAR* CVarLumenHWRT = TEXT("r.Lumen.HardwareRayTracing");

	/** Dynamic resolution aims at the frame cap, or at this with no cap. */
	constexpr float DynamicResDefaultFps = 60.f;
	/** ...and goes no lower than this (or the upscaling quality's own scale when that is lower). */
	constexpr float DynamicResMinPercent = 50.f;

	TOptional<ShinobiGraphics::FCaps> CapsOverride;
	TOptional<ShinobiGraphics::FCaps> ProbedCaps;
	/** Set once the PostEngineInit plugins (DLSS, Streamline) are loaded; before that a probe would find none of them. */
	bool bPluginsLoaded = false;

	bool CanProbe()
	{
		return bPluginsLoaded || GIsRunning;
	}

	IConsoleVariable* FindCVar(const TCHAR* Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name);
	}

	void SetCVar(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = FindCVar(Name); CVar && CVar->GetInt() != Value)
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}

	void SetCVarFloat(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = FindCVar(Name); CVar && !FMath::IsNearlyEqual(CVar->GetFloat(), Value))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}

	bool GetSavedBool(const TCHAR* Key, bool bDefault)
	{
		bool bValue = bDefault;
		GConfig->GetBool(GraphicsSection, Key, bValue, GGameUserSettingsIni);
		return bValue;
	}

	void SaveBool(const TCHAR* Key, bool bValue)
	{
		GConfig->SetBool(GraphicsSection, Key, bValue, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	FString GetSavedString(const TCHAR* Key)
	{
		FString Value;
		GConfig->GetString(GraphicsSection, Key, Value, GGameUserSettingsIni);
		return Value;
	}

	void SaveString(const TCHAR* Key, const FString& Value)
	{
		GConfig->SetString(GraphicsSection, Key, *Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	UWorld* AnyGameWorld()
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

	/** An enum parameter (enum class or TEnumAsByte): its UEnum and the numeric property holding its value. */
	bool EnumOf(FProperty& Property, UEnum*& Enum, FNumericProperty*& Numeric)
	{
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
		{
			Enum = EnumProperty->GetEnum();
			Numeric = EnumProperty->GetUnderlyingProperty();
		}
		else if (FByteProperty* ByteProperty = CastField<FByteProperty>(&Property))
		{
			Enum = ByteProperty->Enum;
			Numeric = ByteProperty;
		}
		return Enum && Numeric;
	}

	/**
	 * Calls a static UFUNCTION of a plugin's Blueprint library by name (no link dependency on the plugin). Fill sets each
	 * input parameter and returns false for one it can't fill, which cancels the call. Read gets the return value.
	 * Returns whether the call was made: false when the plugin, the function or a parameter is missing.
	 */
	bool CallLibrary(const TCHAR* ClassName, const TCHAR* FunctionName, TFunctionRef<bool(FProperty&, void*)> Fill, TFunctionRef<void(FProperty&, const void*)> Read)
	{
		UClass* Class = FindFirstObject<UClass>(ClassName, EFindFirstObjectOptions::NativeFirst);
		UFunction* Function = Class ? Class->FindFunctionByName(FunctionName) : nullptr;
		if (!Function || !Function->HasAnyFunctionFlags(FUNC_Static))
		{
			return false;
		}
		uint8* Params = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(Function->ParmsSize, 1), Function->GetMinAlignment()));
		FMemory::Memzero(Params, Function->ParmsSize);
		Function->InitializeStruct(Params);
		bool bFilled = true;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_ReturnParm) && !Fill(**It, It->ContainerPtrToValuePtr<void>(Params)))
			{
				bFilled = false;
				break;
			}
		}
		if (bFilled)
		{
			Class->GetDefaultObject()->ProcessEvent(Function, Params);
			if (FProperty* Return = Function->GetReturnProperty())
			{
				Read(*Return, Return->ContainerPtrToValuePtr<void>(Params));
			}
		}
		Function->DestroyStruct(Params);
		FMemory::Free(Params);
		return bFilled;
	}

	/** Fills a world-context object parameter; any other parameter can't be filled. */
	bool FillWorldContext(FProperty& Property, void* Value)
	{
		if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(&Property))
		{
			Object->SetObjectPropertyValue(Value, AnyGameWorld());
			return true;
		}
		return false;
	}

	void IgnoreReturn(FProperty&, const void*)
	{
	}

	/** A parameterless bool function, e.g. UDLSSLibrary::IsDLSSSupported; false when it isn't there. */
	bool CallBool(const TCHAR* ClassName, const TCHAR* FunctionName)
	{
		bool bResult = false;
		CallLibrary(ClassName, FunctionName, FillWorldContext, [&bResult](FProperty& Return, const void* Value)
		{
			if (const FBoolProperty* Bool = CastField<FBoolProperty>(&Return))
			{
				bResult = Bool->GetPropertyValue(Value);
			}
		});
		return bResult;
	}

	/** Sorts frame generation modes: off, then by multiplier, the plugin's automatic mode last. */
	int32 FrameGenOrder(const FString& Mode)
	{
		const FString Label = ShinobiGraphics::FrameGenLabel(Mode);
		return Label == TEXT("Off") ? 0 : Label == TEXT("Dynamic") ? 100 : FCString::Atoi(*Label);
	}

	TArray<FString> ProbeFrameGenModes()
	{
		TArray<FString> Modes;
		if (!CallBool(DLSSGLibrary, TEXT("IsDLSSGSupported")))
		{
			return Modes;
		}
		// The modes this GPU runs, as the plugin's enum names (multi frame generation: On2X, On3X, On4X, Auto...).
		CallLibrary(DLSSGLibrary, TEXT("GetSupportedDLSSGModes"), FillWorldContext, [&Modes](FProperty& Return, const void* Value)
		{
			UEnum* Enum = nullptr;
			FNumericProperty* Numeric = nullptr;
			const FArrayProperty* Array = CastField<FArrayProperty>(&Return);
			if (Array && EnumOf(*Array->Inner, Enum, Numeric))
			{
				FScriptArrayHelper Helper(Array, Value);
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					Modes.AddUnique(Enum->GetNameStringByValue(Numeric->GetSignedIntPropertyValue(Helper.GetRawPtr(Index))));
				}
			}
		});
		Modes.RemoveAll([](const FString& Mode) { return Mode.IsEmpty() || Mode.EndsWith(TEXT("MAX")); });
		if (Modes.Num() <= 1 && FindCVar(CVarDLSSG))
		{
			Modes = { TEXT("Off"), TEXT("On") }; // a plugin without the list: plain 2x through its console variable
		}
		if (!Modes.ContainsByPredicate([](const FString& Mode) { return ShinobiGraphics::FrameGenLabel(Mode) == TEXT("Off"); }))
		{
			Modes.Insert(TEXT("Off"), 0);
		}
		Modes.StableSort([](const FString& A, const FString& B) { return FrameGenOrder(A) < FrameGenOrder(B); });
		return Modes;
	}

	ShinobiGraphics::FCaps Probe()
	{
		ShinobiGraphics::FCaps Caps;
		Caps.bDLSS = FindCVar(CVarDLSS) && CallBool(DLSSLibrary, TEXT("IsDLSSSupported"));
		Caps.bRayReconstruction = Caps.bDLSS && FindCVar(CVarDLSSDenoiser) && CallBool(DLSSLibrary, TEXT("IsDLSSRRSupported"));
		Caps.bReflex = FindCVar(CVarReflex) && CallBool(ReflexLibrary, TEXT("IsReflexSupported"));
		Caps.bRayTracing = IsRayTracingEnabled() && FindCVar(CVarLumenHWRT);
		Caps.FrameGenModes = ProbeFrameGenModes();
		UE_LOG(LogShinobiGraphics, Log, TEXT("Graphics: DLSS %d, Ray Reconstruction %d, Reflex %d, hardware ray tracing %d, frame generation [%s]"),
			Caps.bDLSS, Caps.bRayReconstruction, Caps.bReflex, Caps.bRayTracing, *FString::Join(Caps.FrameGenModes, TEXT(", ")));
		return Caps;
	}

	/** Sets the plugin's frame generation mode by enum name (UStreamlineLibraryDLSSG::SetDLSSGMode), else its console variable. */
	void ApplyFrameGen(const FString& Mode)
	{
		const bool bSet = CallLibrary(DLSSGLibrary, TEXT("SetDLSSGMode"), [&Mode](FProperty& Property, void* Value)
		{
			UEnum* Enum = nullptr;
			FNumericProperty* Numeric = nullptr;
			if (EnumOf(Property, Enum, Numeric))
			{
				const int64 EnumValue = Enum->GetValueByNameString(Mode);
				if (EnumValue == INDEX_NONE)
				{
					return false;
				}
				Numeric->SetIntPropertyValue(Value, EnumValue);
				return true;
			}
			return FillWorldContext(Property, Value);
		}, IgnoreReturn);
		if (!bSet)
		{
			SetCVar(CVarDLSSG, ShinobiGraphics::FrameGenLabel(Mode) == TEXT("Off") ? 0 : 1);
		}
	}
}

const ShinobiGraphics::FCaps& ShinobiGraphics::Caps()
{
	if (CapsOverride.IsSet())
	{
		return CapsOverride.GetValue();
	}
	if (!ProbedCaps.IsSet())
	{
		if (!CanProbe())
		{
			static const FCaps NotYet; // (not kept: probed for real once the engine has loaded every plugin)
			return NotYet;
		}
		ProbedCaps = Probe();
	}
	return ProbedCaps.GetValue();
}

void ShinobiGraphics::SetCapsOverride(const FCaps* Override)
{
	if (Override)
	{
		CapsOverride = *Override;
	}
	else
	{
		CapsOverride.Reset();
		ProbedCaps.Reset();
	}
}

FString ShinobiGraphics::FrameGenLabel(const FString& ModeName)
{
	if (ModeName.IsEmpty() || ModeName.Equals(TEXT("Off"), ESearchCase::IgnoreCase) || ModeName.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase))
	{
		return TEXT("Off");
	}
	if (ModeName.Contains(TEXT("Auto")) || ModeName.Contains(TEXT("Dynamic")))
	{
		return TEXT("Dynamic");
	}
	FString Digits;
	for (const TCHAR Char : ModeName)
	{
		if (FChar::IsDigit(Char))
		{
			Digits.AppendChar(Char);
		}
	}
	return Digits.IsEmpty() ? FString(TEXT("2x")) : Digits + TEXT("x"); // "On" (no multiplier): plain frame generation
}

ShinobiGraphics::EUpscaler ShinobiGraphics::GetUpscaler()
{
	if (!Caps().bDLSS)
	{
		return EUpscaler::TSR;
	}
	return GetSavedString(UpscalerKey) == TEXT("TSR") ? EUpscaler::TSR : EUpscaler::DLSS; // DLSS by default where it runs
}

void ShinobiGraphics::SetUpscaler(EUpscaler Upscaler)
{
	SaveString(UpscalerKey, Upscaler == EUpscaler::DLSS ? TEXT("DLSS") : TEXT("TSR"));
	ApplySaved();
}

bool ShinobiGraphics::IsRayTracing()
{
	return Caps().bRayTracing && GetSavedBool(RayTracingKey, false);
}

void ShinobiGraphics::SetRayTracing(bool bOn)
{
	SaveBool(RayTracingKey, bOn);
	ApplySaved();
}

bool ShinobiGraphics::IsRayReconstruction()
{
	return Caps().bRayReconstruction && GetSavedBool(RayReconstructionKey, false) && GetUpscaler() == EUpscaler::DLSS && IsRayTracing();
}

void ShinobiGraphics::SetRayReconstruction(bool bOn)
{
	SaveBool(RayReconstructionKey, bOn);
	if (bOn)
	{
		SaveString(UpscalerKey, TEXT("DLSS")); // (it denoises inside DLSS, and only ray-traced lighting)
		SaveBool(RayTracingKey, true);
	}
	ApplySaved();
}

TArray<FString> ShinobiGraphics::FrameGenOptions()
{
	TArray<FString> Options;
	for (const FString& Mode : Caps().FrameGenModes)
	{
		Options.Add(FrameGenLabel(Mode));
	}
	if (Options.IsEmpty())
	{
		Options.Add(TEXT("Off"));
	}
	return Options;
}

int32 ShinobiGraphics::GetFrameGen()
{
	return FMath::Max(0, Caps().FrameGenModes.IndexOfByKey(GetSavedString(FrameGenKey)));
}

bool ShinobiGraphics::IsFrameGenOn()
{
	return GetFrameGen() > 0;
}

void ShinobiGraphics::SetFrameGen(int32 Index)
{
	const TArray<FString>& Modes = Caps().FrameGenModes;
	if (!Modes.IsValidIndex(Index))
	{
		return;
	}
	SaveString(FrameGenKey, Modes[Index]);
	ApplySaved(); // (turns VSync off when it's on)
}

bool ShinobiGraphics::IsLowLatency()
{
	return Caps().bReflex && (GetSavedBool(LowLatencyKey, true) || IsFrameGenOn());
}

void ShinobiGraphics::SetLowLatency(bool bOn)
{
	SaveBool(LowLatencyKey, bOn);
	ApplySaved();
}

void ShinobiGraphics::ApplySaved()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	const bool bFrameGen = IsFrameGenOn();
	if (bFrameGen && Settings->IsVSyncEnabled())
	{
		// Frame generation paces the generated frames itself (with Reflex); engine VSync on top adds latency and judder.
		Settings->SetVSyncEnabled(false);
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}

	// Reflex first: frame generation needs it.
	SetCVar(CVarReflex, IsLowLatency() ? 1 : 0);

	// The upscaler. DLSS follows the engine's screen percentage (the Upscaling quality row); at 100 % it's DLAA.
	const bool bDLSS = GetUpscaler() == EUpscaler::DLSS;
	const float RenderScale = ShinobiSettings::GetRenderScale();
	SetCVar(CVarDLSS, bDLSS ? 1 : 0);
	SetCVar(CVarDLAA, bDLSS && RenderScale >= 99.5f ? 1 : 0);
	SetCVar(CVarDLSSDenoiser, IsRayReconstruction() ? 1 : 0);

	// Hardware ray tracing for Lumen (the project builds the ray-tracing shaders: r.RayTracing in DefaultEngine.ini).
	SetCVar(CVarLumenHWRT, IsRayTracing() ? 1 : 0);

	if (!Caps().FrameGenModes.IsEmpty())
	{
		ApplyFrameGen(Caps().FrameGenModes[GetFrameGen()]);
	}

	// Dynamic resolution: aim at the frame cap (60 FPS uncapped), from the upscaling quality's scale down to 50 %.
	if (Settings->IsDynamicResolutionEnabled())
	{
		const float Cap = Settings->GetFrameRateLimit();
		SetCVarFloat(TEXT("r.DynamicRes.FrameTimeBudget"), 1000.f / (Cap > 0.f ? Cap : DynamicResDefaultFps));
		SetCVarFloat(TEXT("r.DynamicRes.MaxScreenPercentage"), RenderScale);
		SetCVarFloat(TEXT("r.DynamicRes.MinScreenPercentage"), FMath::Min(RenderScale, DynamicResMinPercent));
	}
}

bool UShinobiGraphicsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UShinobiGraphicsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (CanProbe())
	{
		ShinobiGraphics::ApplySaved(); // (PIE, or a game instance made later)
		return;
	}
	// A packaged game makes its game instance before the PostEngineInit plugins load: apply once they have.
	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([]()
	{
		bPluginsLoaded = true;
		ShinobiGraphics::ApplySaved();
	});
}

void UShinobiGraphicsSubsystem::Deinitialize()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
	Super::Deinitialize();
}
