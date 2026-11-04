#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShinobiGraphics.generated.h"

/**
 * Upscaler, sharpness, frame generation, NVIDIA Reflex and ray-tracing choices for the settings menu (ShinobiSettings
 * rows), on top of what UGameUserSettings already keeps (quality preset, render scale, dynamic resolution, frame cap, VSync).
 *
 * NVIDIA's plugins (DLSS: Super Resolution / DLAA / Ray Reconstruction; Streamline: Frame Generation and Reflex) are
 * optional. They are listed in ShinobiCoop.uproject as Optional, Win64 only, and this module has no link dependency on them.
 * Everything goes through their console variables and their Blueprint function libraries (UDLSSLibrary,
 * UStreamlineLibraryDLSSG, UStreamlineLibraryReflex), found by name at runtime. Without them every row is still listed, but
 * the features are locked with the reason (FCaps::*Lock). The console command Shinobi.GraphicsReport logs what was found:
 * plugins, modules, console variables and every function of those libraries.
 *
 * The choices are saved per machine in GameUserSettings.ini ([/Script/ShinobiCoop.ShinobiSettings], like Snow) and applied
 * again at startup by UShinobiGraphicsSubsystem, because UGameUserSettings doesn't keep these console variables. A saved choice
 * the machine no longer supports (another GPU, the plugin removed) falls back to TSR / off.
 */
namespace ShinobiGraphics
{
	/** Which temporal upscaler (or native anti-aliasing at 100 %) draws the scene. */
	enum class EUpscaler : uint8 { TSR, DLSS };

	/** What this machine can do, and why not. Probed by Caps() / RefreshCaps(), or set by tests (SetCapsOverride). */
	struct FCaps
	{
		/** The DLSS plugin is loaded and the GPU runs DLSS Super Resolution (any GeForce RTX). */
		bool bDLSS = false;
		/** ...and DLSS Ray Reconstruction. */
		bool bRayReconstruction = false;
		/** Streamline Reflex is available (most GeForce cards). */
		bool bReflex = false;
		/** Hardware ray tracing is on for this run (DX12 SM6 or Vulkan with an RT-capable GPU, r.RayTracing in the project). */
		bool bRayTracing = false;
		/** Every frame generation mode the plugin has, by its own enum names (e.g. "Off", "On2X", "On3X", "On4X"). "Off" is
		 * first. Without the plugin: Off, On2X, On3X, On4X (shown, locked). */
		TArray<FString> FrameGenAllModes;
		/** The ones this GPU runs ("Off" alone when none). */
		TArray<FString> FrameGenModes;

		/** Why a feature is locked, for the player ("" when it isn't). */
		FString DLSSLock;
		FString RayReconstructionLock;
		FString ReflexLock;
		FString RayTracingLock;
		FString FrameGenLock;

		bool HasFrameGen() const { return FrameGenModes.Num() > 1; }
	};

	SHINOBICOOP_API const FCaps& Caps();
	/** Probes the machine again (the settings panel does when it's built: plugins load late, drivers change). */
	SHINOBICOOP_API void RefreshCaps();
	/** Tests: pretend the machine can do Caps (nullptr: probe the real one again). Doesn't apply anything. */
	SHINOBICOOP_API void SetCapsOverride(const FCaps* Caps);

	/** The upscaler in use: the saved pick when this machine supports it, else TSR. */
	SHINOBICOOP_API EUpscaler GetUpscaler();
	SHINOBICOOP_API void SetUpscaler(EUpscaler Upscaler);

	/** Sharpening after upscaling (r.Tonemapper.Sharpen): 0 off, 1 low, 2 medium, 3 high. */
	SHINOBICOOP_API int32 GetSharpness();
	SHINOBICOOP_API void SetSharpness(int32 Level);
	constexpr int32 SharpnessLevels = 4;

	/** Lumen traces hardware rays (r.Lumen.HardwareRayTracing) instead of software distance fields. Off when unsupported. */
	SHINOBICOOP_API bool IsRayTracing();
	SHINOBICOOP_API void SetRayTracing(bool bOn);

	/** DLSS Ray Reconstruction denoises the ray-traced lighting. Only in effect with DLSS and ray tracing both on. */
	SHINOBICOOP_API bool IsRayReconstruction();
	/** Saves the pick; turning it on also picks DLSS and ray tracing, which it needs. */
	SHINOBICOOP_API void SetRayReconstruction(bool bOn);

	/** Frame generation options as shown, one per FCaps::FrameGenAllModes: "Off", "2X", "3X", "4X"... "Dynamic". */
	SHINOBICOOP_API TArray<FString> FrameGenOptions();
	/** Why FrameGenOptions()[Index] can't be picked on this machine ("" when it can). */
	SHINOBICOOP_API FString FrameGenOptionLock(int32 Index);
	/** Index into FrameGenOptions in effect (0: off). */
	SHINOBICOOP_API int32 GetFrameGen();
	/** Turning frame generation on also turns VSync off (frame generation paces its own frames) and forces Reflex on. A locked
	 * option does nothing. */
	SHINOBICOOP_API void SetFrameGen(int32 Index);
	SHINOBICOOP_API bool IsFrameGenOn();

	/** NVIDIA Reflex Low Latency in effect: 0 off, 1 on, 2 on + boost. At least 1 while frame generation is on. */
	SHINOBICOOP_API int32 GetReflexMode();
	SHINOBICOOP_API void SetReflexMode(int32 Mode);
	constexpr int32 ReflexModes = 3;

	/**
	 * Writes every console variable from the saved picks: the upscaler (and DLAA at 100 %), sharpness, Ray Reconstruction,
	 * hardware Lumen, frame generation, Reflex, and the dynamic resolution budget (frame cap, upscaling quality). Call after any
	 * of these or UGameUserSettings::ApplyNonResolutionSettings, which resets some of them. Missing console variables are skipped.
	 */
	SHINOBICOOP_API void ApplySaved();

	/** The label for a frame generation enum name: "Off", "2X", "3X", "Dynamic"... */
	SHINOBICOOP_API FString FrameGenLabel(const FString& ModeName);

	/** A readable report of the NVIDIA plugins, their modules, console variables and library functions, and what was probed.
	 * The Shinobi.GraphicsReport console command logs it. */
	SHINOBICOOP_API FString Report();
}

/** Applies the saved graphics picks (ShinobiGraphics::ApplySaved) once the game instance starts; not on a dedicated server. */
UCLASS()
class SHINOBICOOP_API UShinobiGraphicsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	FDelegateHandle PostEngineInitHandle;
};
