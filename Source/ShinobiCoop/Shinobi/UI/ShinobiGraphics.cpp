#include "Shinobi/UI/ShinobiGraphics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "RenderUtils.h"
#include "Shinobi/UI/ShinobiSettings.h"
#include "Shinobi/VFX/ShinobiVfx.h"
#include "Components/LightComponent.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogShinobiGraphics, Log, All);

namespace
{
	const TCHAR* GraphicsSection = TEXT("/Script/ShinobiCoop.ShinobiSettings");
	const TCHAR* UpscalerKey = TEXT("Upscaler");
	const TCHAR* UpscalingQualityKey = TEXT("UpscalingQuality");
	const TCHAR* AntiAliasingKey = TEXT("AntiAliasing");
	const TCHAR* SharpnessKey = TEXT("Sharpness");
	const TCHAR* RayTracingKey = TEXT("bRayTracing");
	const TCHAR* MegaLightsKey = TEXT("bMegaLights");
	const TCHAR* RayReconstructionKey = TEXT("bRayReconstruction");
	const TCHAR* FrameGenKey = TEXT("FrameGeneration");
	const TCHAR* ReflexModeKey = TEXT("ReflexMode");
	const TCHAR* OldLowLatencyKey = TEXT("bLowLatency"); // (before Reflex had On + Boost)

	// NVIDIA's plugins (their .uplugin names) and Blueprint libraries (UDLSSLibrary, UStreamlineLibraryDLSSG, UStreamlineLibraryReflex).
	const TCHAR* const NvidiaPlugins[] = { TEXT("DLSS"), TEXT("StreamlineCore"), TEXT("StreamlineDLSSG"), TEXT("StreamlineReflex") };
	const TCHAR* DLSSLibrary = TEXT("DLSSLibrary");
	const TCHAR* DLSSGLibrary = TEXT("StreamlineLibraryDLSSG");
	const TCHAR* ReflexLibrary = TEXT("StreamlineLibraryReflex");
	const TCHAR* const NvidiaLibraries[] = { DLSSLibrary, DLSSGLibrary, ReflexLibrary };

	// Their console variables, and the engine's.
	const TCHAR* CVarDLSS = TEXT("r.NGX.DLSS.Enable");
	const TCHAR* CVarDLAA = TEXT("r.NGX.DLAA.Enable"); // (older plugins; newer ones have UDLSSLibrary::EnableDLAA)
	const TCHAR* CVarDLSSDenoiser = TEXT("r.NGX.DLSS.DenoiserMode"); // 1: Ray Reconstruction
	const TCHAR* CVarDLSSG = TEXT("r.Streamline.DLSSG.Enable");
	const TCHAR* CVarReflex = TEXT("t.Streamline.Reflex.Enable");
	const TCHAR* CVarReflexMode = TEXT("t.Streamline.Reflex.Mode"); // 0 off, 1 low latency, 2 low latency + boost
	const TCHAR* CVarLumenHWRT = TEXT("r.Lumen.HardwareRayTracing");
	const TCHAR* CVarSharpen = TEXT("r.Tonemapper.Sharpen");
	const TCHAR* CVarMegaLights = TEXT("r.MegaLights.EnableForProject");
	const TCHAR* const ReportedCVars[] = { CVarDLSS, CVarDLAA, CVarDLSSDenoiser, CVarDLSSG, CVarReflex, CVarReflexMode, CVarLumenHWRT, CVarSharpen, CVarMegaLights,
		TEXT("r.Lumen.FinalGatherMethod"), TEXT("sg.GlobalIlluminationQuality"), TEXT("sg.ReflectionQuality"), TEXT("r.Velocity.EnableVertexDeformation"),
		TEXT("r.ScreenPercentage"), TEXT("r.AntiAliasingMethod"), TEXT("r.RayTracing"), TEXT("r.DynamicRes.OperationMode") };

	/** Upscaling quality: Quality, Balanced, Performance, Ultra Performance (the vendors' per-axis ratios). */
	const float QualityPercents[ShinobiGraphics::UpscalingQualities] = { 66.7f, 58.f, 50.f, 33.3f };

	/** Sharpness Off / Low / Medium / High. */
	const float SharpenAmounts[ShinobiGraphics::SharpnessLevels] = { 0.f, 0.4f, 0.8f, 1.2f };

	/** Dynamic resolution aims at the frame cap, or at this with no cap. */
	constexpr float DynamicResDefaultFps = 60.f;
	/** ...and goes no lower than this (or the upscaling quality's own scale when that is lower). */
	constexpr float DynamicResMinPercent = 50.f;

	// Player-facing reasons a feature is locked.
	const TCHAR* LockNotInBuild = TEXT("Not available in this build: NVIDIA's plugin isn't loaded (Shinobi.GraphicsReport in the console says why).");
	const TCHAR* LockNeedsRtx = TEXT("Requires an NVIDIA GeForce RTX graphics card.");
	const TCHAR* LockNeedsGeForce = TEXT("Requires an NVIDIA GeForce graphics card.");
	const TCHAR* LockFrameGenNeedsRtx40 = TEXT("Requires a GeForce RTX 40 Series graphics card or newer.");
	const TCHAR* LockMultiFrameGenNeedsRtx50 = TEXT("Requires a GeForce RTX 50 Series graphics card.");
	const TCHAR* LockNeedsRayTracingGpu = TEXT("Requires a graphics card with hardware ray tracing, running DirectX 12.");

	TOptional<ShinobiGraphics::FCaps> CapsOverride;
	TOptional<ShinobiGraphics::FCaps> ProbedCaps;
	/** Set once the PostEngineInit plugins (DLSS, Streamline) are loaded; before that a probe would find none of them. */
	bool bPluginsLoaded = false;
	/** The full report is logged with the first probe of a session. */
	bool bReportLogged = false;

	bool CanProbe()
	{
		return bPluginsLoaded || GIsRunning;
	}

	IConsoleVariable* FindCVar(const TCHAR* Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name);
	}

	// At code priority: game-setting priority is below the project's DefaultEngine.ini (r.Lumen.HardwareRayTracing=False there
	// silently won over it) and below what plugins set from code. Only the console (and the command line) win over this.
	void SetCVar(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = FindCVar(Name); CVar && CVar->GetInt() != Value)
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}

	void SetCVarFloat(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = FindCVar(Name); CVar && !FMath::IsNearlyEqual(CVar->GetFloat(), Value))
		{
			CVar->Set(Value, ECVF_SetByCode);
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

	int32 GetSavedInt(const TCHAR* Key, int32 Default)
	{
		int32 Value = Default;
		GConfig->GetInt(GraphicsSection, Key, Value, GGameUserSettingsIni);
		return Value;
	}

	void SaveInt(const TCHAR* Key, int32 Value)
	{
		GConfig->SetInt(GraphicsSection, Key, Value, GGameUserSettingsIni);
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

	UClass* FindLibrary(const TCHAR* ClassName)
	{
		return FindFirstObject<UClass>(ClassName, EFindFirstObjectOptions::NativeFirst);
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
		UClass* Class = FindLibrary(ClassName);
		UFunction* Function = Class ? Class->FindFunctionByName(FName(FunctionName)) : nullptr;
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

	/** Calls a library function taking one bool (UDLSSLibrary::EnableDLSS etc.); false when it isn't there. */
	bool CallWithBool(const TCHAR* ClassName, const TCHAR* FunctionName, bool bValue)
	{
		return CallLibrary(ClassName, FunctionName, [bValue](FProperty& Property, void* Value)
		{
			if (FBoolProperty* Bool = CastField<FBoolProperty>(&Property))
			{
				Bool->SetPropertyValue(Value, bValue);
				return true;
			}
			return FillWorldContext(Property, Value);
		}, IgnoreReturn);
	}

	/** Calls a library function taking one enum (SetDLSSGMode, SetReflexMode); Pick chooses the value from the enum
	 * (INDEX_NONE: don't call). False when the function isn't there or nothing was picked. */
	bool CallWithEnum(const TCHAR* ClassName, const TCHAR* FunctionName, TFunctionRef<int64(const UEnum&)> Pick)
	{
		return CallLibrary(ClassName, FunctionName, [&Pick](FProperty& Property, void* Value)
		{
			UEnum* Enum = nullptr;
			FNumericProperty* Numeric = nullptr;
			if (EnumOf(Property, Enum, Numeric))
			{
				const int64 EnumValue = Pick(*Enum);
				if (EnumValue == INDEX_NONE)
				{
					return false;
				}
				Numeric->SetIntPropertyValue(Value, EnumValue);
				return true;
			}
			return FillWorldContext(Property, Value);
		}, IgnoreReturn);
	}

	/** A parameterless bool function, e.g. UDLSSLibrary::IsDLSSSupported; unset when it isn't there. */
	TOptional<bool> CallBool(const TCHAR* ClassName, const TCHAR* FunctionName)
	{
		TOptional<bool> Result;
		CallLibrary(ClassName, FunctionName, FillWorldContext, [&Result](FProperty& Return, const void* Value)
		{
			if (const FBoolProperty* Bool = CastField<FBoolProperty>(&Return))
			{
				Result = Bool->GetPropertyValue(Value);
			}
		});
		return Result;
	}

	/** A parameterless function returning an enum, e.g. UDLSSLibrary::QueryDLSSSupport: the value's name ("" when missing). */
	FString CallEnumName(const TCHAR* ClassName, const TCHAR* FunctionName)
	{
		FString Result;
		CallLibrary(ClassName, FunctionName, FillWorldContext, [&Result](FProperty& Return, const void* Value)
		{
			UEnum* Enum = nullptr;
			FNumericProperty* Numeric = nullptr;
			if (EnumOf(Return, Enum, Numeric))
			{
				Result = Enum->GetNameStringByValue(Numeric->GetSignedIntPropertyValue(Value));
			}
		});
		return Result;
	}

	/**
	 * The player-facing reason for a plugin's support query result (UDLSSSupport / EStreamlineFeatureSupport value names:
	 * Supported, NotSupportedIncompatibleHardware, NotSupportedDriverOutOfDate, NotSupportedHardwareSchedulingDisabled...).
	 * Hardware is the text for an unsuitable GPU. "" when supported.
	 */
	FString SupportLock(const FString& Query, const TCHAR* Hardware)
	{
		if (Query == TEXT("Supported"))
		{
			return FString();
		}
		if (Query.Contains(TEXT("Scheduling")))
		{
			return TEXT("Turn on Hardware-accelerated GPU scheduling (Windows Settings > System > Display > Graphics), then restart your PC.");
		}
		if (Query.Contains(TEXT("Driver")))
		{
			return TEXT("Update your NVIDIA graphics driver to use this.");
		}
		if (Query.Contains(TEXT("OperatingSystem")))
		{
			return TEXT("Update Windows to use this.");
		}
		if (Query.Contains(TEXT("Capture")))
		{
			return TEXT("Not available while a graphics capture tool (RenderDoc, PIX...) is attached.");
		}
		if (Query.Contains(TEXT("RHI")))
		{
			return TEXT("Requires DirectX 12.");
		}
		if (Query.Contains(TEXT("Platform")))
		{
			return TEXT("Not available on this platform.");
		}
		if (Query.Contains(TEXT("Hardware")) || Query.IsEmpty())
		{
			return Hardware;
		}
		return FString::Printf(TEXT("Not supported on this PC (%s)."), *Query);
	}

	/** Support of a library's feature: its Is...Supported(), else (no such function) the GPU's brand. Lock gets the reason. */
	bool ProbeFeature(const TCHAR* Library, const TCHAR* CVar, const TCHAR* IsSupported, const TCHAR* Query, const TCHAR* Hardware, bool bNeedsRtx, FString& Lock)
	{
		if (!FindLibrary(Library) || !FindCVar(CVar))
		{
			Lock = LockNotInBuild;
			return false;
		}
		const TOptional<bool> Supported = CallBool(Library, IsSupported);
		if (!Supported.IsSet())
		{
			// (a plugin version without that function: judge by the GPU, and the plugin works it out itself)
			const FString Gpu = FPlatformMisc::GetPrimaryGPUBrand();
			const bool bLooksSupported = Gpu.Contains(TEXT("NVIDIA")) && (!bNeedsRtx || Gpu.Contains(TEXT("RTX")));
			Lock = bLooksSupported ? FString() : FString(Hardware);
			return bLooksSupported;
		}
		Lock = Supported.GetValue() ? FString() : SupportLock(CallEnumName(Library, Query), Hardware);
		return Supported.GetValue();
	}

	/** Sorts frame generation modes: off, then by multiplier, the plugin's automatic / dynamic mode last. */
	int32 FrameGenOrder(const FString& Mode)
	{
		const FString Label = ShinobiGraphics::FrameGenLabel(Mode);
		return Label == TEXT("Off") ? 0 : Label == TEXT("Dynamic") ? 100 : FCString::Atoi(*Label);
	}

	/** The frame generation mode enum (EStreamlineDLSSGMode), from SetDLSSGMode's parameter. */
	UEnum* FindFrameGenEnum()
	{
		UClass* Class = FindLibrary(DLSSGLibrary);
		UFunction* Function = Class ? Class->FindFunctionByName(FName(TEXT("SetDLSSGMode"))) : nullptr;
		if (!Function)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			UEnum* Enum = nullptr;
			FNumericProperty* Numeric = nullptr;
			if (!It->HasAnyPropertyFlags(CPF_ReturnParm) && EnumOf(**It, Enum, Numeric))
			{
				return Enum;
			}
		}
		return nullptr;
	}

	void SortModes(TArray<FString>& Modes)
	{
		Modes.RemoveAll([](const FString& Mode) { return Mode.IsEmpty() || Mode.EndsWith(TEXT("MAX")); });
		if (!Modes.ContainsByPredicate([](const FString& Mode) { return ShinobiGraphics::FrameGenLabel(Mode) == TEXT("Off"); }))
		{
			Modes.Insert(TEXT("Off"), 0);
		}
		Modes.StableSort([](const FString& A, const FString& B) { return FrameGenOrder(A) < FrameGenOrder(B); });
	}

	void ProbeFrameGen(ShinobiGraphics::FCaps& Caps)
	{
		// Every mode the plugin has (shown, the ones this GPU can't run locked).
		if (UEnum* Enum = FindFrameGenEnum())
		{
			for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
			{
#if WITH_METADATA
				if (Enum->HasMetaData(TEXT("Hidden"), Index))
				{
					continue;
				}
#endif
				Caps.FrameGenAllModes.Add(Enum->GetNameStringByIndex(Index));
			}
		}
		else
		{
			Caps.FrameGenAllModes = FindCVar(CVarDLSSG) ? TArray<FString>{ TEXT("Off"), TEXT("On") } : TArray<FString>{ TEXT("Off"), TEXT("On2X"), TEXT("On3X"), TEXT("On4X") };
		}
		// The legacy "Auto" mode reads "Dynamic" like Dynamic Multi Frame Generation ("OnDynamic"); list only the latter.
		if (Caps.FrameGenAllModes.ContainsByPredicate([](const FString& Mode) { return Mode != TEXT("Auto") && ShinobiGraphics::FrameGenLabel(Mode) == TEXT("Dynamic"); }))
		{
			Caps.FrameGenAllModes.Remove(TEXT("Auto"));
		}
		SortModes(Caps.FrameGenAllModes);
		Caps.FrameGenModes = { Caps.FrameGenAllModes[0] };

		if (!ProbeFeature(DLSSGLibrary, CVarDLSSG, TEXT("IsDLSSGSupported"), TEXT("QueryDLSSGSupport"), LockFrameGenNeedsRtx40, true, Caps.FrameGenLock))
		{
			return;
		}
		// The modes this GPU runs (multi frame generation: RTX 50).
		TArray<FString> Supported;
		CallLibrary(DLSSGLibrary, TEXT("GetSupportedDLSSGModes"), FillWorldContext, [&Supported](FProperty& Return, const void* Value)
		{
			UEnum* Enum = nullptr;
			FNumericProperty* Numeric = nullptr;
			const FArrayProperty* Array = CastField<FArrayProperty>(&Return);
			if (Array && EnumOf(*Array->Inner, Enum, Numeric))
			{
				FScriptArrayHelper Helper(Array, Value);
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					Supported.AddUnique(Enum->GetNameStringByValue(Numeric->GetSignedIntPropertyValue(Helper.GetRawPtr(Index))));
				}
			}
		});
		for (const FString& Mode : Caps.FrameGenAllModes)
		{
			// (no list from the plugin: plain 2x only)
			const bool bRuns = Supported.Num() > 0 ? Supported.Contains(Mode) : FrameGenOrder(Mode) == 2;
			if (bRuns && FrameGenOrder(Mode) != 0)
			{
				Caps.FrameGenModes.Add(Mode);
			}
		}
	}

	ShinobiGraphics::FCaps Probe()
	{
		ShinobiGraphics::FCaps Caps;
		Caps.bDLSS = ProbeFeature(DLSSLibrary, CVarDLSS, TEXT("IsDLSSSupported"), TEXT("QueryDLSSSupport"), LockNeedsRtx, true, Caps.DLSSLock);
		if (!Caps.bDLSS)
		{
			Caps.RayReconstructionLock = Caps.DLSSLock;
		}
		else
		{
			Caps.bRayReconstruction = ProbeFeature(DLSSLibrary, CVarDLSSDenoiser, TEXT("IsDLSSRRSupported"), TEXT("QueryDLSSRRSupport"), LockNeedsRtx, true, Caps.RayReconstructionLock);
		}
		Caps.bReflex = ProbeFeature(ReflexLibrary, CVarReflex, TEXT("IsReflexSupported"), TEXT("QueryReflexSupport"), LockNeedsGeForce, false, Caps.ReflexLock);
		Caps.bRayTracing = IsRayTracingEnabled() && FindCVar(CVarLumenHWRT);
		Caps.RayTracingLock = Caps.bRayTracing ? FString() : FString(LockNeedsRayTracingGpu);
		Caps.bMegaLights = Caps.bRayTracing && FindCVar(CVarMegaLights);
		Caps.MegaLightsLock = Caps.bMegaLights ? FString() : !Caps.bRayTracing ? FString(LockNeedsRayTracingGpu) : FString(TEXT("Not available in this build."));
		ProbeFrameGen(Caps);
		if (Caps.bRayReconstruction && !Caps.bRayTracing)
		{
			Caps.bRayReconstruction = false;
			Caps.RayReconstructionLock = Caps.RayTracingLock;
		}
		return Caps;
	}

	FString PropertyText(const FProperty& Property)
	{
		return FString::Printf(TEXT("%s %s"), *Property.GetCPPType(), *Property.GetName());
	}

	FString ReportFor(const ShinobiGraphics::FCaps& Caps)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("GPU: %s | RHI: %s | hardware ray tracing enabled: %d"), *FPlatformMisc::GetPrimaryGPUBrand(), *FApp::GetGraphicsRHI(), IsRayTracingEnabled() ? 1 : 0));
		for (const TCHAR* Name : NvidiaPlugins)
		{
			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Name);
			if (!Plugin.IsValid())
			{
				Lines.Add(FString::Printf(TEXT("Plugin %s: NOT FOUND (expected Plugins/%s/%s.uplugin)"), Name, Name, Name));
				continue;
			}
			TArray<FString> Modules;
			for (const FModuleDescriptor& Module : Plugin->GetDescriptor().Modules)
			{
				Modules.Add(FString::Printf(TEXT("%s %s"), *Module.Name.ToString(), FModuleManager::Get().IsModuleLoaded(Module.Name) ? TEXT("loaded") : TEXT("NOT LOADED")));
			}
			Lines.Add(FString::Printf(TEXT("Plugin %s %s: enabled %d, mounted %d, at %s | modules: %s"), Name, *Plugin->GetDescriptor().VersionName,
				Plugin->IsEnabled() ? 1 : 0, Plugin->IsMounted() ? 1 : 0, *Plugin->GetBaseDir(), *FString::Join(Modules, TEXT(", "))));
		}
		for (const TCHAR* Name : ReportedCVars)
		{
			const IConsoleVariable* CVar = FindCVar(Name);
			Lines.Add(FString::Printf(TEXT("CVar %s = %s"), Name, CVar ? *CVar->GetString() : TEXT("(MISSING)")));
		}
		for (const TCHAR* Name : NvidiaLibraries)
		{
			UClass* Class = FindLibrary(Name);
			if (!Class)
			{
				Lines.Add(FString::Printf(TEXT("Library U%s: MISSING"), Name));
				continue;
			}
			Lines.Add(FString::Printf(TEXT("Library %s:"), *Class->GetPathName()));
			for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				TArray<FString> Params;
				FString Return = TEXT("void");
				for (TFieldIterator<FProperty> Param(*It); Param && Param->HasAnyPropertyFlags(CPF_Parm); ++Param)
				{
					if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						Return = Param->GetCPPType();
					}
					else
					{
						Params.Add(PropertyText(**Param));
					}
				}
				Lines.Add(FString::Printf(TEXT("    %s %s(%s)%s"), *Return, *It->GetName(), *FString::Join(Params, TEXT(", ")), It->HasAnyFunctionFlags(FUNC_Static) ? TEXT("") : TEXT(" [not static]")));
			}
		}
		const auto Feature = [&Lines](const TCHAR* Name, bool bOn, const FString& Lock)
		{
			Lines.Add(FString::Printf(TEXT("=> %s: %s%s"), Name, bOn ? TEXT("available") : TEXT("locked"), Lock.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Lock)));
		};
		Feature(TEXT("DLSS Super Resolution"), Caps.bDLSS, Caps.DLSSLock);
		Feature(TEXT("DLSS Ray Reconstruction"), Caps.bRayReconstruction, Caps.RayReconstructionLock);
		Feature(TEXT("NVIDIA Reflex"), Caps.bReflex, Caps.ReflexLock);
		Feature(TEXT("Ray tracing"), Caps.bRayTracing, Caps.RayTracingLock);
		Feature(TEXT("MegaLights"), Caps.bMegaLights, Caps.MegaLightsLock);
		Feature(TEXT("DLSS Frame Generation"), Caps.HasFrameGen(), Caps.FrameGenLock);
		Lines.Add(FString::Printf(TEXT("=> frame generation modes: all [%s], this PC [%s]"), *FString::Join(Caps.FrameGenAllModes, TEXT(", ")), *FString::Join(Caps.FrameGenModes, TEXT(", "))));
		// What the plugins say is running now (after the menu's picks were applied).
		const auto Bool = [](const TCHAR* Library, const TCHAR* Function)
		{
			const TOptional<bool> Value = CallBool(Library, Function);
			return Value.IsSet() ? FString(Value.GetValue() ? TEXT("1") : TEXT("0")) : FString(TEXT("?"));
		};
		const auto Enum = [](const TCHAR* Library, const TCHAR* Function)
		{
			const FString Value = CallEnumName(Library, Function);
			return Value.IsEmpty() ? FString(TEXT("?")) : Value;
		};
		Lines.Add(FString::Printf(TEXT("=> in effect: DLSS %s, DLAA %s, Ray Reconstruction %s, DLSS mode %s, frame generation %s, Reflex %s, screen percentage %s"),
			*Bool(DLSSLibrary, TEXT("IsDLSSEnabled")), *Bool(DLSSLibrary, TEXT("IsDLAAEnabled")), *Bool(DLSSLibrary, TEXT("IsDLSSRREnabled")),
			*Enum(DLSSLibrary, TEXT("GetDLSSMode")), *Enum(DLSSGLibrary, TEXT("GetDLSSGMode")), *Enum(ReflexLibrary, TEXT("GetReflexMode")),
			FindCVar(TEXT("r.ScreenPercentage")) ? *FindCVar(TEXT("r.ScreenPercentage"))->GetString() : TEXT("?")));
		return FString::Join(Lines, TEXT("\n"));
	}

	void LogReport(const FString& Report)
	{
		TArray<FString> Lines;
		Report.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			UE_LOG(LogShinobiGraphics, Display, TEXT("%s"), *Line);
		}
	}

	FAutoConsoleCommand GraphicsReportCommand(
		TEXT("Shinobi.GraphicsReport"),
		TEXT("Probes the GPU and NVIDIA's DLSS / Streamline plugins again and logs what was found: plugins, modules, console variables, library functions and what the settings menu locks."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ShinobiGraphics::RefreshCaps();
			LogReport(ShinobiGraphics::Report());
		}));

	/** Sets the plugin's frame generation mode by enum name (UStreamlineLibraryDLSSG::SetDLSSGMode), else its console variable. */
	void ApplyFrameGen(const FString& Mode)
	{
		if (!CallWithEnum(DLSSGLibrary, TEXT("SetDLSSGMode"), [&Mode](const UEnum& Enum) { return Enum.GetValueByNameString(Mode); }))
		{
			SetCVar(CVarDLSSG, FrameGenOrder(Mode) == 0 ? 0 : 1);
		}
	}

	/** 0 off, 1 on, 2 on + boost, for a Reflex mode enum name (EStreamlineReflexMode: Off, Enabled, Boost...). */
	int32 ReflexModeLevel(const FString& Name)
	{
		return Name.Contains(TEXT("Boost")) ? 2 : (Name.Contains(TEXT("Off")) || Name.Contains(TEXT("Disabled"))) ? 0 : 1;
	}

	/** Sets Reflex (UStreamlineLibraryReflex::SetReflexMode), else its console variables. */
	void ApplyReflex(int32 Level)
	{
		const bool bSet = CallWithEnum(ReflexLibrary, TEXT("SetReflexMode"), [Level](const UEnum& Enum) -> int64
		{
			for (int32 Index = 0; Index < Enum.NumEnums(); ++Index)
			{
				const FString Name = Enum.GetNameStringByIndex(Index);
				if (!Name.EndsWith(TEXT("MAX")) && ReflexModeLevel(Name) == Level)
				{
					return Enum.GetValueByIndex(Index);
				}
			}
			return INDEX_NONE;
		});
		if (!bSet)
		{
			SetCVar(CVarReflex, Level > 0 ? 1 : 0);
			if (Level > 0)
			{
				SetCVar(CVarReflexMode, Level);
			}
		}
	}

	/** Turns a DLSS feature on or off through the plugin (UDLSSLibrary::EnableDLSS etc.), else its console variable. */
	void ApplyDLSSFeature(const TCHAR* Function, const TCHAR* CVar, bool bOn)
	{
		if (!CallWithBool(DLSSLibrary, Function, bOn))
		{
			SetCVar(CVar, bOn ? 1 : 0);
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
		if (!bReportLogged)
		{
			bReportLogged = true;
			LogReport(ReportFor(ProbedCaps.GetValue()));
		}
	}
	return ProbedCaps.GetValue();
}

void ShinobiGraphics::RefreshCaps()
{
	if (CanProbe())
	{
		ProbedCaps.Reset();
	}
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

FString ShinobiGraphics::Report()
{
	return ReportFor(Caps());
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
	return Digits.IsEmpty() ? FString(TEXT("2X")) : Digits + TEXT("X"); // "On" (no multiplier): plain frame generation
}

namespace
{
	/** The saved upscaler, or what the first run picks: Off at a 100 % render scale (today's look), else DLSS / TSR. */
	ShinobiGraphics::EUpscaler SavedUpscaler()
	{
		const FString Saved = GetSavedString(UpscalerKey);
		if (Saved == TEXT("Off"))
		{
			return ShinobiGraphics::EUpscaler::Off;
		}
		if (Saved == TEXT("TSR"))
		{
			return ShinobiGraphics::EUpscaler::TSR;
		}
		if (Saved == TEXT("DLSS"))
		{
			return ShinobiGraphics::EUpscaler::DLSS;
		}
		if (ShinobiSettings::GetRenderScale() >= 99.5f)
		{
			return ShinobiGraphics::EUpscaler::Off;
		}
		return ShinobiGraphics::Caps().bDLSS ? ShinobiGraphics::EUpscaler::DLSS : ShinobiGraphics::EUpscaler::TSR;
	}

	const TCHAR* UpscalerKeyValue(ShinobiGraphics::EUpscaler Upscaler)
	{
		return Upscaler == ShinobiGraphics::EUpscaler::Off ? TEXT("Off") : Upscaler == ShinobiGraphics::EUpscaler::TSR ? TEXT("TSR") : TEXT("DLSS");
	}
}

ShinobiGraphics::EUpscaler ShinobiGraphics::GetUpscaler()
{
	const EUpscaler Saved = SavedUpscaler();
	return Saved == EUpscaler::DLSS && !Caps().bDLSS ? EUpscaler::TSR : Saved;
}

void ShinobiGraphics::SetUpscaler(EUpscaler Upscaler)
{
	if (Upscaler == EUpscaler::DLSS && !Caps().bDLSS)
	{
		return;
	}
	SaveString(UpscalerKey, UpscalerKeyValue(Upscaler));
	ApplySaved();
}

int32 ShinobiGraphics::GetUpscalingQuality()
{
	int32 Quality = INDEX_NONE;
	if (!GConfig->GetInt(GraphicsSection, UpscalingQualityKey, Quality, GGameUserSettingsIni))
	{
		// First run: the preset nearest the render scale (the menu used to set it directly), Quality at 100 %.
		const float Scale = ShinobiSettings::GetRenderScale();
		Quality = 0;
		for (int32 Index = 1; Index < UpscalingQualities && Scale < 99.5f; ++Index)
		{
			if (FMath::Abs(QualityPercents[Index] - Scale) < FMath::Abs(QualityPercents[Quality] - Scale))
			{
				Quality = Index;
			}
		}
	}
	return FMath::Clamp(Quality, 0, UpscalingQualities - 1);
}

void ShinobiGraphics::SetUpscalingQuality(int32 Quality)
{
	SaveInt(UpscalingQualityKey, FMath::Clamp(Quality, 0, UpscalingQualities - 1));
	ApplySaved();
}

float ShinobiGraphics::UpscalingQualityPercent(int32 Quality)
{
	return QualityPercents[FMath::Clamp(Quality, 0, UpscalingQualities - 1)];
}

ShinobiGraphics::EAntiAliasing ShinobiGraphics::GetAntiAliasing()
{
	const FString Saved = GetSavedString(AntiAliasingKey);
	const bool bDLAA = Saved.IsEmpty() ? Caps().bDLSS : Saved == TEXT("DLAA"); // (DLAA by default where it runs)
	return bDLAA && Caps().bDLSS ? EAntiAliasing::DLAA : EAntiAliasing::TSR;
}

void ShinobiGraphics::SetAntiAliasing(EAntiAliasing AntiAliasing)
{
	if (AntiAliasing == EAntiAliasing::DLAA && !Caps().bDLSS)
	{
		return;
	}
	SaveString(AntiAliasingKey, AntiAliasing == EAntiAliasing::DLAA ? TEXT("DLAA") : TEXT("TSR"));
	ApplySaved();
}

bool ShinobiGraphics::IsDLSSActive()
{
	const EUpscaler Upscaler = GetUpscaler();
	return Upscaler == EUpscaler::DLSS || (Upscaler == EUpscaler::Off && GetAntiAliasing() == EAntiAliasing::DLAA);
}

float ShinobiGraphics::GetTargetScreenPercentage()
{
	return GetUpscaler() == EUpscaler::Off ? 100.f : UpscalingQualityPercent(GetUpscalingQuality());
}

int32 ShinobiGraphics::GetSharpness()
{
	return FMath::Clamp(GetSavedInt(SharpnessKey, 0), 0, SharpnessLevels - 1);
}

void ShinobiGraphics::SetSharpness(int32 Level)
{
	SaveInt(SharpnessKey, FMath::Clamp(Level, 0, SharpnessLevels - 1));
	ApplySaved();
}

bool ShinobiGraphics::IsRayTracing()
{
	return Caps().bRayTracing && GetSavedBool(RayTracingKey, false);
}

void ShinobiGraphics::SetRayTracing(bool bOn)
{
	if (bOn && !Caps().bRayTracing)
	{
		return;
	}
	SaveBool(RayTracingKey, bOn);
	ApplySaved();
}

bool ShinobiGraphics::IsMegaLights()
{
	return Caps().bMegaLights && GetSavedBool(MegaLightsKey, false);
}

void ShinobiGraphics::SetMegaLights(bool bOn)
{
	if (bOn && !Caps().bMegaLights)
	{
		return;
	}
	SaveBool(MegaLightsKey, bOn);
	ApplySaved();
}

bool ShinobiGraphics::IsRayReconstruction()
{
	return Caps().bRayReconstruction && GetSavedBool(RayReconstructionKey, false) && IsDLSSActive() && IsRayTracing();
}

void ShinobiGraphics::SetRayReconstruction(bool bOn)
{
	if (bOn && !Caps().bRayReconstruction)
	{
		return;
	}
	SaveBool(RayReconstructionKey, bOn);
	if (bOn)
	{
		// It denoises inside DLSS, and only ray-traced lighting: DLSS (DLAA with upscaling off) and ray tracing come on with it.
		if (GetUpscaler() == EUpscaler::Off)
		{
			SaveString(AntiAliasingKey, TEXT("DLAA"));
		}
		else
		{
			SaveString(UpscalerKey, TEXT("DLSS"));
		}
		SaveBool(RayTracingKey, true);
	}
	ApplySaved();
}

TArray<FString> ShinobiGraphics::FrameGenOptions()
{
	TArray<FString> Options;
	for (const FString& Mode : Caps().FrameGenAllModes)
	{
		Options.Add(FrameGenLabel(Mode));
	}
	if (Options.IsEmpty())
	{
		Options.Add(TEXT("Off"));
	}
	return Options;
}

FString ShinobiGraphics::FrameGenOptionLock(int32 Index)
{
	const FCaps& C = Caps();
	if (!C.FrameGenAllModes.IsValidIndex(Index) || Index == 0 || C.FrameGenModes.Contains(C.FrameGenAllModes[Index]))
	{
		return FString();
	}
	if (!C.FrameGenLock.IsEmpty())
	{
		return C.FrameGenLock;
	}
	return FrameGenOrder(C.FrameGenAllModes[Index]) == 2 ? FString(TEXT("Not supported on this PC.")) : FString(LockMultiFrameGenNeedsRtx50);
}

int32 ShinobiGraphics::GetFrameGen()
{
	const FCaps& C = Caps();
	const FString Saved = GetSavedString(FrameGenKey);
	return C.FrameGenModes.Contains(Saved) ? FMath::Max(0, C.FrameGenAllModes.IndexOfByKey(Saved)) : 0;
}

bool ShinobiGraphics::IsFrameGenOn()
{
	return GetFrameGen() > 0;
}

void ShinobiGraphics::SetFrameGen(int32 Index)
{
	const TArray<FString>& Modes = Caps().FrameGenAllModes;
	if (!Modes.IsValidIndex(Index) || !FrameGenOptionLock(Index).IsEmpty())
	{
		return;
	}
	SaveString(FrameGenKey, Modes[Index]);
	ApplySaved(); // (turns VSync off when it's on)
}

int32 ShinobiGraphics::GetReflexMode()
{
	if (!Caps().bReflex)
	{
		return 0;
	}
	const int32 Saved = FMath::Clamp(GetSavedInt(ReflexModeKey, GetSavedBool(OldLowLatencyKey, true) ? 1 : 0), 0, ReflexModes - 1);
	return IsFrameGenOn() ? FMath::Max(Saved, 1) : Saved;
}

void ShinobiGraphics::SetReflexMode(int32 Mode)
{
	if (!Caps().bReflex)
	{
		return;
	}
	SaveInt(ReflexModeKey, FMath::Clamp(Mode, 0, ReflexModes - 1));
	ApplySaved();
}

void ShinobiGraphics::ApplySaved()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	// The first run's derived picks are kept, so they don't drift as the render scale changes under them.
	if (GetSavedString(UpscalerKey).IsEmpty())
	{
		SaveString(UpscalerKey, UpscalerKeyValue(SavedUpscaler()));
	}
	if (GetSavedInt(UpscalingQualityKey, INDEX_NONE) == INDEX_NONE)
	{
		SaveInt(UpscalingQualityKey, GetUpscalingQuality());
	}

	const bool bFrameGen = IsFrameGenOn();
	const float ScreenPercentage = GetTargetScreenPercentage();
	bool bApplySettings = false;
	if (bFrameGen && Settings->IsVSyncEnabled())
	{
		// Frame generation paces the generated frames itself (with Reflex); engine VSync on top adds latency and judder.
		Settings->SetVSyncEnabled(false);
		bApplySettings = true;
	}
	if (!FMath::IsNearlyEqual(ShinobiSettings::GetRenderScale(), ScreenPercentage, 0.1f))
	{
		ShinobiSettings::SetRenderScale(ScreenPercentage); // (kept in GameUserSettings.ini too)
		bApplySettings = true;
	}
	if (bApplySettings)
	{
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}

	// Reflex first: frame generation needs it.
	ApplyReflex(GetReflexMode());

	// DLSS: upscaling, or DLAA at native resolution; Ray Reconstruction inside it. TSR when it's off.
	const bool bDLSS = IsDLSSActive();
	ApplyDLSSFeature(TEXT("EnableDLSS"), CVarDLSS, bDLSS);
	ApplyDLSSFeature(TEXT("EnableDLAA"), CVarDLAA, bDLSS && GetUpscaler() == EUpscaler::Off);
	ApplyDLSSFeature(TEXT("EnableDLSSRR"), CVarDLSSDenoiser, IsRayReconstruction());
	// (The scene's resolution comes from GameUserSettings, above: r.ScreenPercentage stays at scalability priority, which the
	// Graphics quality presets write too. Set from code it would lock them out.)
	SetCVarFloat(CVarSharpen, SharpenAmounts[GetSharpness()]);

	// Hardware ray tracing for Lumen (the project builds the ray-tracing shaders: r.RayTracing in DefaultEngine.ini).
	SetCVar(CVarLumenHWRT, IsRayTracing() ? 1 : 0);

	// MegaLights, and the effect lights already in the world casting shadows with it (new ones do at BeginPlay).
	const bool bMegaLights = IsMegaLights();
	SetCVar(CVarMegaLights, bMegaLights ? 1 : 0);
	for (TObjectIterator<ULightComponent> It; It; ++It)
	{
		const UWorld* World = It->GetWorld();
		if (World && World->IsGameWorld() && It->ComponentTags.Contains(ShinobiVfx::EffectLightTag) && It->CastShadows != bMegaLights)
		{
			It->SetCastShadows(bMegaLights);
		}
	}

	if (Caps().HasFrameGen())
	{
		ApplyFrameGen(Caps().FrameGenAllModes[GetFrameGen()]);
	}

	// Dynamic resolution: aim at the frame cap (60 FPS uncapped), from the picked resolution down to 50 %.
	if (Settings->IsDynamicResolutionEnabled())
	{
		const float Cap = Settings->GetFrameRateLimit();
		SetCVarFloat(TEXT("r.DynamicRes.FrameTimeBudget"), 1000.f / (Cap > 0.f ? Cap : DynamicResDefaultFps));
		SetCVarFloat(TEXT("r.DynamicRes.MaxScreenPercentage"), ScreenPercentage);
		SetCVarFloat(TEXT("r.DynamicRes.MinScreenPercentage"), FMath::Min(ScreenPercentage, DynamicResMinPercent));
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
		ShinobiGraphics::RefreshCaps();
		ShinobiGraphics::ApplySaved();
	});
}

void UShinobiGraphicsSubsystem::Deinitialize()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
	Super::Deinitialize();
}
