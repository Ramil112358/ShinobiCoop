# ShinobiCoop M0 + M1 (Setup + Core Combat) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A UE 5.8 C++ project where two players (listen server) can move, sprint, double jump, dodge, melee-combo and chakra-charge, see health/chakra/stamina bars, and hit training dummies, with all state replicated through the Gameplay Ability System.

**Architecture:** Third Person C++ template as the base; all gameplay code lives in `Source/ShinobiCoop/Shinobi/`. GAS drives everything: one `UShinobiAttributeSet`, C++-defined gameplay effects (`UShinobiEffect` subclasses), and `UShinobiGameplayAbility` subclasses bound to Enhanced Input actions that are built in C++ at runtime. Automation tests spin up a throwaway `UWorld` (same pattern as the engine's `GameplayEffectsTest`) and exercise real abilities and effects.

**Tech Stack:** Unreal Engine 5.8.2 (C++), GameplayAbilities plugin, Enhanced Input, UMG built in C++, Unreal Python (editor commandlet) for asset generation, PowerShell scripts for build/test/smoke.

Spec: `docs/superpowers/specs/2026-09-23-combat-slice-design.md` (milestones M0 and M1).

---

## Conventions (read first)

- Project root: `C:\Users\ahmad\ShinobiCoop` (all paths below are relative to it).
- Engine root: `C:\Program Files\Epic Games\UE_5.8`.
- **Close the Unreal Editor before running `Scripts/build.ps1`.** Build.bat refuses to build while Live Coding holds the editor DLLs.
- Run `Scripts/build.ps1`, `Scripts/run_tests.ps1` and `Scripts/run_python.ps1` with the shell tool timeout set to 600000 ms (`run_tests.ps1`/`run_python.ps1` time out themselves at 540 s; `build.ps1` has no timeout of its own). If a run needs longer (a full rebuild, first-time shader/DDC warm-up), run it in the background or pass a larger `-TimeoutSec`.
- A C++ "failing test" step usually fails by **not compiling** (missing header/type). That is the expected red state.
- Commit messages end with the trailer:
  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  ```
- New `.h` files that declare UCLASS/USTRUCT/UENUM include their `*.generated.h` last.
- Shadowing a member name with a local (e.g. a local called `Slot` inside a widget) is a compile error in UE. Use distinct local names.

## File structure

| File | Responsibility |
|---|---|
| `.gitignore`, `.gitattributes` | Ignore build outputs; LFS for `.uasset`/`.umap` |
| `Scripts/build.ps1` | Build `ShinobiCoopEditor` Win64 Development |
| `Scripts/run_tests.ps1` | Run `ShinobiCoop.*` automation tests headless, print pass/fail, non-zero exit on failure |
| `Scripts/run_python.ps1` | Run an Unreal Python script through the editor commandlet |
| `Scripts/setup_m1.py` | Create melee/dodge montages; point `Lvl_ThirdPerson` GameMode override at `ShinobiGameMode` |
| `Scripts/smoke_m1.ps1` | Launch listen server + client processes, assert client sees replicated damage |
| `Source/ShinobiCoop/Shinobi/ShinobiTags.h/.cpp` | Native gameplay tags |
| `Source/ShinobiCoop/Shinobi/ShinobiAttributeSet.h/.cpp` | Health/Chakra/Stamina/Substitution attributes, clamping, damage meta attribute, out-of-health event |
| `Source/ShinobiCoop/Shinobi/ShinobiEffects.h/.cpp` | `UShinobiEffect` helper base + all M1 gameplay effects |
| `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h/.cpp` | Replicated sprint flag (saved-move compressed flag) |
| `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.h/.cpp` | Character + ASC + attributes, team, startup abilities/effects |
| `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h/.cpp` | Ability base: input ID, damage helper, hostile cone query |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Sprint.h/.cpp` | Hold to sprint, drains stamina |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.h/.cpp` | Dash with stamina cost and i-frames |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.h/.cpp` | Hold to refill chakra, movement locked |
| `Source/ShinobiCoop/Shinobi/Abilities/GA_Melee.h/.cpp` | 3-hit taijutsu combo, chakra on hit |
| `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.h/.cpp` | Builds input actions + mapping context in code |
| `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h/.cpp` | Camera, input binding, player team |
| `Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.h/.cpp` | Hittable dummy with floating HP text, auto-reset |
| `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h/.cpp` | HUD creation, dev exec commands, smoke-test hook |
| `Source/ShinobiCoop/Shinobi/ShinobiGameMode.h/.cpp` | Default pawn/controller, spawns training dummies |
| `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h/.cpp` | HP/chakra/stamina bars built in C++ |
| `Source/ShinobiCoop/Tests/ShinobiTestWorld.h` | Test world + helpers |
| `Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp` | Attribute/effect tests |
| `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp` | Ability tests |
| `Source/ShinobiCoop/Tests/ShinobiInputTests.cpp` | Input mapping/modifier data tests (no world needed) |

---

### Task 0: Visual Studio toolchain (user action)

**Files:** none

> **Done 2026-09-24:** Visual Studio 2026 Community 18.10.2, MSVC 14.51.36231 (above UE 5.8 minimum, not banned; not on the "preferred" list, so UBT may print a version notice). The editor already compiled the template with it.

- [x] **Step 1: Install Visual Studio**

The user installs **Visual Studio 2022 Community 17.14 or newer** (or Visual Studio 2026) with:
- Workloads: "Game development with C++", "Desktop development with C++"
- Individual components: "MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)", "Windows 11 SDK (10.0.22621.0)", ".NET Framework 4.6.2 targeting pack", "C++ Clang Compiler for Windows" (optional), "IDE support for Unreal Engine"

- [x] **Step 2: Verify the MSVC version meets UE 5.8**

Run (PowerShell):
```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
Get-ChildItem "$(& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products * -property installationPath)\VC\Tools\MSVC" | Select-Object Name
```
Expected: a folder `14.44.35211` or newer (VS 2022), or `14.50.35723` or newer (VS 2026). Versions `14.44.0–14.44.35210` and `14.50.0–14.50.35722` are banned by UE 5.8.

---

### Task 1: Create the project from the template (user action)

**Files:** creates `ShinobiCoop.uproject`, `Source/`, `Config/`, `Content/` inside the existing repo folder.

> **Done 2026-09-24:** project created at `C:\Users\ahmad\ShinobiCoop`; the repo (`.git`, `docs/`) was moved into it.

- [x] **Step 1: Create the project in the Unreal Project Browser**

The user launches Unreal Engine 5.8 → Games → **Third Person** → **C++** → Project Location `C:\Users\ahmad` → Project Name `ShinobiCoop` → Create.

- [x] **Step 2: Close the editor and Visual Studio once the project has opened**

- [x] **Step 3: Verify files exist**

Run:
```powershell
Test-Path C:\Users\ahmad\ShinobiCoop\ShinobiCoop.uproject
Get-ChildItem C:\Users\ahmad\ShinobiCoop\Source\ShinobiCoop -Name
```
Expected: `True`, and a listing including `ShinobiCoop.Build.cs`, `ShinobiCoop.h`, `ShinobiCoopCharacter.cpp`, `Variant_Combat`, `Variant_Platforming`, `Variant_SideScrolling`.

---

### Task 2: Repo hygiene and removing unused template variants

**Files:**
- Create: `.gitignore`, `.gitattributes`
- Delete: `Source/ShinobiCoop/Variant_Platforming/`, `Source/ShinobiCoop/Variant_SideScrolling/`, `Content/Variant_Platforming/`, `Content/Variant_SideScrolling/`, and their World Partition folders `Content/__ExternalActors__/Variant_{Platforming,SideScrolling}/`, `Content/__ExternalObjects__/Variant_{Platforming,SideScrolling}/`
- Modify: `Source/ShinobiCoop/ShinobiCoop.Build.cs`

> **Done 2026-09-24** (`0c92371`, follow-up `chore: widen LFS coverage...`). As executed, the World Partition folders above were also removed, and review follow-ups replaced the `.gitignore`/`.gitattributes` below with anchored ignore rules, `* text=auto`, and LFS coverage for more binary types (see the files in the repo).

- [ ] **Step 1: Write `.gitignore`**

```gitignore
Binaries/
Intermediate/
Saved/
DerivedDataCache/
.vs/
.idea/
*.sln
*.slnx
Backup/
UpgradeLog.htm
*.suo
*.opensdf
*.sdf
*.VC.db
*.VC.opendb
Plugins/*/Binaries/
Plugins/*/Intermediate/
__pycache__/
```

- [ ] **Step 2: Write `.gitattributes`**

```gitattributes
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap filter=lfs diff=lfs merge=lfs -text
*.png filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.wav filter=lfs diff=lfs merge=lfs -text
```

- [ ] **Step 3: Delete the Platforming and SideScrolling variants**

```powershell
Set-Location C:\Users\ahmad\ShinobiCoop
Remove-Item -Recurse -Force Source\ShinobiCoop\Variant_Platforming, Source\ShinobiCoop\Variant_SideScrolling, Content\Variant_Platforming, Content\Variant_SideScrolling
```

- [ ] **Step 4: Replace `Source/ShinobiCoop/ShinobiCoop.Build.cs` entirely**

```csharp
// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShinobiCoop : ModuleRules
{
	public ShinobiCoop(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks"
		});

		PublicIncludePaths.AddRange(new string[] {
			"ShinobiCoop",
			"ShinobiCoop/Variant_Combat",
			"ShinobiCoop/Variant_Combat/AI",
			"ShinobiCoop/Variant_Combat/Animation",
			"ShinobiCoop/Variant_Combat/Gameplay",
			"ShinobiCoop/Variant_Combat/Interfaces",
			"ShinobiCoop/Variant_Combat/UI"
		});
	}
}
```

- [ ] **Step 5: Enable LFS and commit**

```powershell
git lfs install
git add -A
git commit -m "chore: add UE5.8 Third Person C++ project, drop unused variants

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
Expected: commit succeeds; `git lfs ls-files | Measure-Object` shows several hundred `.uasset` entries.

---

### Task 3: Plugins, build and test scripts, first build

**Files:**
- Modify: `ShinobiCoop.uproject`
- Create: `Scripts/build.ps1`, `Scripts/run_tests.ps1`, `Scripts/run_python.ps1`

- [ ] **Step 1: Add plugins to `ShinobiCoop.uproject`**

Replace the `"Plugins"` array with:
```json
	"Plugins": [
		{
			"Name": "ModelingToolsEditorMode",
			"Enabled": true,
			"TargetAllowList": [
				"Editor"
			]
		},
		{
			"Name": "StateTree",
			"Enabled": true
		},
		{
			"Name": "GameplayStateTree",
			"Enabled": true
		},
		{
			"Name": "GameplayAbilities",
			"Enabled": true
		},
		{
			"Name": "PythonScriptPlugin",
			"Enabled": true,
			"TargetAllowList": [
				"Editor"
			]
		},
		{
			"Name": "EditorScriptingUtilities",
			"Enabled": true,
			"TargetAllowList": [
				"Editor"
			]
		}
	]
```

- [ ] **Step 2: Write `Scripts/build.ps1`**

```powershell
# Builds the editor target. Close the Unreal Editor first (Live Coding locks the DLLs).
param([string]$Config = "Development")
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Project = (Resolve-Path (Join-Path $PSScriptRoot "..\ShinobiCoop.uproject")).Path
& "$UE\Engine\Build\BatchFiles\Build.bat" ShinobiCoopEditor Win64 $Config "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }
Write-Host "BUILD OK"
```

- [ ] **Step 3: Write `Scripts/run_tests.ps1`**

```powershell
# Runs automation tests headless and prints one line per test. Exit code 1 on any failure.
param([string]$Filter = "ShinobiCoop", [int]$TimeoutSec = 540)
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Log = Join-Path $LogDir "Tests-$Timestamp-$PID.log"
$StdOut = "$Log.stdout.txt"
$StdErr = "$Log.stderr.txt"

# UE expects -Key="value with spaces"; passing one pre-quoted argument string (instead of a
# PowerShell array) makes sure the embedded quotes reach UnrealEditor-Cmd.exe intact, since
# PowerShell 5.1 mangles quoting when native-process args are passed as an array.
$ArgLine = "`"$Project`" -ExecCmds=`"Automation RunTests $Filter;Quit`" -unattended -nopause -nullrhi -nosplash -nosound -notraceserver -abslog=`"$Log`""

$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $ArgLine -NoNewWindow -PassThru -RedirectStandardOutput $StdOut -RedirectStandardError $StdErr
$null = $p.Handle # PS 5.1 needs the handle touched before ExitCode is readable after the process exits
if (-not $p.WaitForExit($TimeoutSec * 1000)) {
    taskkill /PID $p.Id /T /F | Out-Null
    Write-Host "TIMEOUT after $TimeoutSec s waiting for UnrealEditor-Cmd.exe. Tail of log:"
    if (Test-Path $Log) { Get-Content $Log -Tail 40 }
    Write-Host "Log: $Log"
    exit 1
}
$ExitCode = $p.ExitCode

if (-not (Test-Path $Log)) {
    Write-Host "Log file not found (editor exit code $ExitCode). Tail of stdout:"
    if (Test-Path $StdOut) { Get-Content $StdOut -Tail 40 }
    Write-Host "Log: $Log"
    exit 1
}

$geLines = Select-String -Path $Log -Pattern "LogGameplayEffects: (Warning|Error)|Ensure condition failed|should be added to GEComponents"
if ($geLines) {
    Write-Host "--- GAS warnings / ensures ---"
    $geLines | ForEach-Object { Write-Host $_.Line }
}

$results = Select-String -Path $Log -Pattern "Test Completed\. Result=\{([^}]+)\}.*Path=\{([^}]*)\}"
if (-not $results) { Write-Host "NO TESTS RAN (editor exit code $ExitCode). Tail of log:"; Get-Content $Log -Tail 40; Write-Host "Log: $Log"; exit 1 }
$failed = 0
foreach ($r in $results) {
    $outcome = $r.Matches[0].Groups[1].Value
    $name = $r.Matches[0].Groups[2].Value
    Write-Host ("{0,-8} {1}" -f $outcome, $name)
    if ($outcome -ne "Success") { $failed++ }
}
if ($failed -gt 0) {
    Select-String -Path $Log -Pattern "LogAutomationController: Error" | ForEach-Object { Write-Host $_.Line }
    Write-Host "$failed test(s) failed"
    Write-Host "Log: $Log"
    exit 1
}

# All reported results were Success - but a crash mid-run can look the same (the crashed test,
# and everything queued after it, just never produces a result line). Cross-check the editor's
# own exit code, its own "test run complete" marker, and that we saw as many results as tests found.
$foundMatch = Select-String -Path $Log -Pattern "Found (\d+) automation tests based on" | Select-Object -Last 1
$expectedCount = if ($foundMatch) { [int]$foundMatch.Matches[0].Groups[1].Value } else { -1 }
$completeOk = [bool](Select-String -Path $Log -Pattern "\*\*\*\* TEST COMPLETE\. EXIT CODE: 0 \*\*\*\*" -Quiet)
if ($ExitCode -ne 0 -or -not $completeOk -or $results.Count -ne $expectedCount) {
    Write-Host "Run did not complete cleanly: editor exit code=$ExitCode, TEST COMPLETE EXIT CODE 0 found=$completeOk, result lines=$($results.Count), tests found=$expectedCount."
    Write-Host "Tail of log:"
    Get-Content $Log -Tail 40
    Write-Host "Log: $Log"
    exit 1
}

Write-Host "ALL $($results.Count) TESTS PASSED"
Write-Host "Log: $Log"
```

- [ ] **Step 4: Write `Scripts/run_python.ps1`**

```powershell
# Runs an Unreal Python script through the editor's Python commandlet.
param(
    [Parameter(Mandatory)][string]$Script,
    [int]$TimeoutSec = 540
)
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$ScriptPath = (Resolve-Path $Script).Path
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Log = Join-Path $LogDir "Python-$Timestamp-$PID.log"
$StdOut = "$Log.stdout.txt"
$StdErr = "$Log.stderr.txt"

# UE expects -Key="value with spaces"; passing one pre-quoted argument string (instead of a
# PowerShell array) makes sure the embedded quotes reach UnrealEditor-Cmd.exe intact, since
# PowerShell 5.1 mangles quoting when native-process args are passed as an array.
$ArgLine = "`"$Project`" -run=pythonscript -script=`"$ScriptPath`" -unattended -nopause -nosplash -notraceserver -abslog=`"$Log`""

$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $ArgLine -NoNewWindow -PassThru -RedirectStandardOutput $StdOut -RedirectStandardError $StdErr
$null = $p.Handle # PS 5.1 needs the handle touched before ExitCode is readable after the process exits
$TimedOut = -not $p.WaitForExit($TimeoutSec * 1000)
if ($TimedOut) { taskkill /PID $p.Id /T /F | Out-Null }
$ExitCode = if ($TimedOut) { -1 } else { $p.ExitCode }

if ($TimedOut -or $ExitCode -ne 0) {
    if ($TimedOut) { Write-Host "TIMEOUT after $TimeoutSec s running $ScriptPath." }
    Write-Host "Tail of log:"
    if (Test-Path $Log) { Get-Content $Log -Tail 40 }
    Select-String -Path $Log -Pattern "Error:" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_.Line }
    Write-Host "Log: $Log"
    throw "Python script failed (exit $ExitCode)"
}

Select-String -Path $Log -Pattern "\[setup_|LogPython: Error|LogPython: Warning" | ForEach-Object { Write-Host $_.Line }
Write-Host "Log: $Log"
```

- [ ] **Step 5: Build**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: ends with `BUILD OK`. (First build compiles the template code; several minutes.)

- [ ] **Step 6: Verify the test runner works (no tests yet)**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1`
Expected: `NO TESTS RAN (editor exit code <nonzero>).` printed near the end of a short stdout, a `Log: <path>` line, and script exit code 1. This proves the editor boots headless with the new plugins; the next task adds tests.

- [ ] **Step 7: Commit**

```powershell
git add ShinobiCoop.uproject Scripts
git commit -m "build: enable GAS and Python plugins, add build/test scripts

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Tags, attribute set, base character (first tests)

**Files:**
- Create: `Source/ShinobiCoop/Tests/ShinobiTestWorld.h`
- Create: `Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp`
- Create: `Source/ShinobiCoop/Shinobi/ShinobiTags.h`, `ShinobiTags.cpp`
- Create: `Source/ShinobiCoop/Shinobi/ShinobiAttributeSet.h`, `ShinobiAttributeSet.cpp`
- Create: `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h`, `ShinobiMovementComponent.cpp`
- Create: `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.h`, `ShinobiCharacterBase.cpp`
- Create: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`, `ShinobiEffects.cpp` (only `UShinobiEffect`, `UGE_Damage`, `UGE_ChakraGain` in this task)
- Create: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h`, `ShinobiGameplayAbility.cpp` (needed by the character's `StartupAbilities` type)

- [ ] **Step 1: Write the test world helper `Source/ShinobiCoop/Tests/ShinobiTestWorld.h`**

```cpp
#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"

constexpr EAutomationTestFlags ShinobiTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

/** Throwaway game world for GAS integration tests. Mirrors the engine's GameplayEffectsTest setup. */
struct FShinobiTestWorld
{
	UWorld* World = nullptr;

	FShinobiTestWorld()
		: InitialFrameCounter(GFrameCounter)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		// There is no GameMode in this throwaway world, so UWorld::BeginPlay() never calls
		// GameMode->StartPlay(), which is what normally reaches AWorldSettings::NotifyBeginPlay().
		// Without it World::HasBegunPlay() stays false and spawned actors never get DispatchBeginPlay.
		World->GetWorldSettings()->NotifyBeginPlay();
	}

	FShinobiTestWorld(const FShinobiTestWorld&) = delete;
	FShinobiTestWorld& operator=(const FShinobiTestWorld&) = delete;

	~FShinobiTestWorld()
	{
		GFrameCounter = InitialFrameCounter;
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	template <class T>
	T* Spawn(const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(Location, Rotation, Params);
	}

	/** Advances the world in 50 ms steps so timers and periodic effects fire. */
	void Tick(float Seconds)
	{
		while (Seconds > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Seconds, 0.05f);
			World->Tick(LEVELTICK_All, Step);
			Seconds -= Step;
			++GFrameCounter;
		}
	}

private:
	uint64 InitialFrameCounter;
};

namespace ShinobiTest
{
	inline float Get(const AShinobiCharacterBase* Character, const FGameplayAttribute& Attribute)
	{
		return Character->GetAbilitySystemComponent()->GetNumericAttribute(Attribute);
	}

	inline void Set(AShinobiCharacterBase* Character, const FGameplayAttribute& Attribute, float Value)
	{
		Character->GetAbilitySystemComponent()->SetNumericAttributeBase(Attribute, Value);
	}

	/** Applies an instant effect whose single modifier is SetByCaller(DataTag). */
	inline void ApplySetByCaller(AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Magnitude)
	{
		UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(DataTag, Magnitude);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	inline void Damage(AShinobiCharacterBase* Target, float Amount)
	{
		ApplySetByCaller(Target, UGE_Damage::StaticClass(), ShinobiTags::Data_Damage, Amount);
	}

	inline void ApplyEffect(AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> EffectClass)
	{
		UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	inline void Press(AShinobiCharacterBase* Character, EShinobiAbilityInputID InputID)
	{
		Character->GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(InputID));
	}

	inline void Release(AShinobiCharacterBase* Character, EShinobiAbilityInputID InputID)
	{
		Character->GetAbilitySystemComponent()->AbilityLocalInputReleased(static_cast<int32>(InputID));
	}

	inline bool IsActive(AShinobiCharacterBase* Character, TSubclassOf<UGameplayAbility> AbilityClass)
	{
		const FGameplayAbilitySpec* Spec = Character->GetAbilitySystemComponent()->FindAbilitySpecFromClass(AbilityClass);
		return Spec && Spec->IsActive();
	}

	inline bool HasTag(const AShinobiCharacterBase* Character, const FGameplayTag& Tag)
	{
		return Character->GetAbilitySystemComponent()->HasMatchingGameplayTag(Tag);
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: Write the failing tests `Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp`**

```cpp
#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAttributeDefaultsTest, "ShinobiCoop.Core.AttributeDefaults", ShinobiTestFlags)
bool FShinobiAttributeDefaultsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	TestTrue(TEXT("BeginPlay routed"), C->HasActorBegunPlay());
	TestEqual(TEXT("Health"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestEqual(TEXT("MaxHealth"), ShinobiTest::Get(C, UShinobiAttributeSet::GetMaxHealthAttribute()), 500.f);
	TestEqual(TEXT("Chakra"), ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	TestEqual(TEXT("Stamina"), ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute()), 100.f);
	TestEqual(TEXT("SubstitutionCharges"), ShinobiTest::Get(C, UShinobiAttributeSet::GetSubstitutionChargesAttribute()), 3.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDamageClampsTest, "ShinobiCoop.Core.DamageReducesHealthAndClampsAtZero", ShinobiTestFlags)
bool FShinobiDamageClampsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Damage(C, 120.f);
	TestEqual(TEXT("Health after 120 damage"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 380.f);
	TestEqual(TEXT("IncomingDamage reset"), ShinobiTest::Get(C, UShinobiAttributeSet::GetIncomingDamageAttribute()), 0.f);
	ShinobiTest::Damage(C, 1000.f);
	TestEqual(TEXT("Health clamps at 0"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutOfHealthOnceTest, "ShinobiCoop.Core.OutOfHealthFiresOncePerDeath", ShinobiTestFlags)
bool FShinobiOutOfHealthOnceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	int32 Deaths = 0;
	C->GetAttributes()->OnOutOfHealth.AddLambda([&Deaths]() { ++Deaths; });

	ShinobiTest::Damage(C, 600.f);
	ShinobiTest::Damage(C, 10.f);
	TestEqual(TEXT("One death after overkill + extra hit"), Deaths, 1);

	ShinobiTest::Set(C, UShinobiAttributeSet::GetHealthAttribute(), 500.f);
	ShinobiTest::Damage(C, 600.f);
	TestEqual(TEXT("Second death after reset"), Deaths, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraGainClampsTest, "ShinobiCoop.Core.ChakraGainClampsToMax", ShinobiTestFlags)
bool FShinobiChakraGainClampsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Set(C, UShinobiAttributeSet::GetChakraAttribute(), 98.f);
	ShinobiTest::ApplySetByCaller(C, UGE_ChakraGain::StaticClass(), ShinobiTags::Data_ChakraGain, 5.f);
	TestEqual(TEXT("Chakra clamped"), ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 3: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL, error like `cannot open include file 'Shinobi/ShinobiCharacterBase.h'`.

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/ShinobiTags.h`**

```cpp
#pragma once

#include "NativeGameplayTags.h"

namespace ShinobiTags
{
	// SetByCaller magnitudes
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_ChakraGain);

	// Character states
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_StaminaRegenBlocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ChakraCharging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);

	// Ability identity (asset tags)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ChakraCharge);
}
```

- [ ] **Step 5: Write `Source/ShinobiCoop/Shinobi/ShinobiTags.cpp`**

```cpp
#include "Shinobi/ShinobiTags.h"

namespace ShinobiTags
{
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_ChakraGain, "Data.ChakraGain");

	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_StaminaRegenBlocked, "State.StaminaRegenBlocked");
	UE_DEFINE_GAMEPLAY_TAG(State_Sprinting, "State.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_ChakraCharging, "State.ChakraCharging");
	UE_DEFINE_GAMEPLAY_TAG(State_Dodging, "State.Dodging");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Melee, "Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge, "Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Sprint, "Ability.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ChakraCharge, "Ability.ChakraCharge");
}
```

- [ ] **Step 6: Write `Source/ShinobiCoop/Shinobi/ShinobiAttributeSet.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ShinobiAttributeSet.generated.h"

DECLARE_MULTICAST_DELEGATE(FShinobiOutOfHealthDelegate);

/** Health, chakra, stamina and substitution charges shared by players and enemies. */
UCLASS()
class SHINOBICOOP_API UShinobiAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UShinobiAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/**
	 * Server only. Fires each time Health goes from above 0 to 0.
	 * Broadcast from inside PostAttributeChange, i.e. mid gameplay-effect-execute: handlers must
	 * not change Health synchronously from here. Defer (timer / next tick) if a handler needs to.
	 */
	FShinobiOutOfHealthDelegate OnOutOfHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Shinobi")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Shinobi")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Chakra, Category = "Shinobi")
	FGameplayAttributeData Chakra;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, Chakra)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxChakra, Category = "Shinobi")
	FGameplayAttributeData MaxChakra;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, MaxChakra)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Shinobi")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Shinobi")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SubstitutionCharges, Category = "Shinobi")
	FGameplayAttributeData SubstitutionCharges;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, SubstitutionCharges)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSubstitutionCharges, Category = "Shinobi")
	FGameplayAttributeData MaxSubstitutionCharges;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, MaxSubstitutionCharges)

	/** Meta attribute: damage waiting to be applied to Health. Server only, never replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "Shinobi")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UShinobiAttributeSet, IncomingDamage)

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Chakra(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxChakra(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SubstitutionCharges(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxSubstitutionCharges(const FGameplayAttributeData& OldValue);

private:
	void ClampToMax(const FGameplayAttribute& Attribute, float& NewValue) const;
};
```

- [ ] **Step 7: Write `Source/ShinobiCoop/Shinobi/ShinobiAttributeSet.cpp`**

```cpp
#include "Shinobi/ShinobiAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UShinobiAttributeSet::UShinobiAttributeSet()
{
	InitHealth(500.f);
	InitMaxHealth(500.f);
	InitChakra(100.f);
	InitMaxChakra(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitSubstitutionCharges(3.f);
	InitMaxSubstitutionCharges(3.f);
	InitIncomingDamage(0.f);
}

void UShinobiAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, Chakra, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, MaxChakra, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, SubstitutionCharges, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UShinobiAttributeSet, MaxSubstitutionCharges, COND_None, REPNOTIFY_Always);
}

void UShinobiAttributeSet::ClampToMax(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetChakraAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxChakra());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetSubstitutionChargesAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxSubstitutionCharges());
	}
}

void UShinobiAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampToMax(Attribute, NewValue);
}

void UShinobiAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampToMax(Attribute, NewValue);
}

void UShinobiAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	const AActor* Owner = GetOwningActor();
	const bool bAuthority = Owner && Owner->HasAuthority();

	// When a Max attribute shrinks, force the corresponding current value back down to the new
	// cap. ApplyModToAttribute does not recurse into Pre/PostGameplayEffectExecute, so this is
	// safe to call from here, and it internally no-ops on non-authority.
	if (bAuthority)
	{
		if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
		{
			GetOwningAbilitySystemComponent()->ApplyModToAttribute(GetHealthAttribute(), EGameplayModOp::Override, NewValue);
		}
		else if (Attribute == GetMaxChakraAttribute() && GetChakra() > NewValue)
		{
			GetOwningAbilitySystemComponent()->ApplyModToAttribute(GetChakraAttribute(), EGameplayModOp::Override, NewValue);
		}
		else if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
		{
			GetOwningAbilitySystemComponent()->ApplyModToAttribute(GetStaminaAttribute(), EGameplayModOp::Override, NewValue);
		}
		else if (Attribute == GetMaxSubstitutionChargesAttribute() && GetSubstitutionCharges() > NewValue)
		{
			GetOwningAbilitySystemComponent()->ApplyModToAttribute(GetSubstitutionChargesAttribute(), EGameplayModOp::Override, NewValue);
		}
	}

	if (Attribute == GetHealthAttribute() && OldValue > 0.f && NewValue <= 0.f && bAuthority)
	{
		OnOutOfHealth.Broadcast();
	}
}

void UShinobiAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage > 0.f)
		{
			SetHealth(GetHealth() - Damage); // clamped to [0, MaxHealth] in PreAttributeBaseChange
		}
	}
}

void UShinobiAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, Health, OldValue); }
void UShinobiAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, MaxHealth, OldValue); }
void UShinobiAttributeSet::OnRep_Chakra(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, Chakra, OldValue); }
void UShinobiAttributeSet::OnRep_MaxChakra(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, MaxChakra, OldValue); }
void UShinobiAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, Stamina, OldValue); }
void UShinobiAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, MaxStamina, OldValue); }
void UShinobiAttributeSet::OnRep_SubstitutionCharges(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, SubstitutionCharges, OldValue); }
void UShinobiAttributeSet::OnRep_MaxSubstitutionCharges(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UShinobiAttributeSet, MaxSubstitutionCharges, OldValue); }
```

- [ ] **Step 8: Write `Source/ShinobiCoop/Shinobi/ShinobiEffects.h` (Task 4 subset)**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShinobiEffects.generated.h"

class UTargetTagsGameplayEffectComponent;
class UTargetTagRequirementsGameplayEffectComponent;

/**
 * Base for gameplay effects defined in C++.
 * The helpers create GE components as default subobjects, so call them only from constructors.
 */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiEffect : public UGameplayEffect
{
	GENERATED_BODY()

protected:
	void AddModifier(const FGameplayAttribute& Attribute, float Magnitude);
	void AddSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag);
	void SetDuration(float Seconds);
	void SetInfinitePeriodic(float PeriodSeconds);
	/** Target gets this tag while the effect is active. */
	void GrantTag(const FGameplayTag& Tag);
	/** Effect is not applied at all if the target has this tag. */
	void IgnoreApplicationIfTargetHas(const FGameplayTag& Tag);
	/** Effect stays applied but does nothing while the target has this tag. */
	void InhibitWhileTargetHas(const FGameplayTag& Tag);

private:
	UTargetTagRequirementsGameplayEffectComponent& Requirements();

	UPROPERTY()
	TObjectPtr<UTargetTagsGameplayEffectComponent> TargetTagsComponent;

	UPROPERTY()
	TObjectPtr<UTargetTagRequirementsGameplayEffectComponent> RequirementsComponent;
};

/** Instant damage. Magnitude: SetByCaller Data.Damage. Ignored while the target is invulnerable. */
UCLASS()
class SHINOBICOOP_API UGE_Damage : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_Damage();
};

/** Instant chakra gain. Magnitude: SetByCaller Data.ChakraGain. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraGain : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraGain();
};
```

- [ ] **Step 9: Write `Source/ShinobiCoop/Shinobi/ShinobiEffects.cpp` (Task 4 subset)**

```cpp
#include "Shinobi/ShinobiEffects.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiTags.h"

void UShinobiEffect::AddModifier(const FGameplayAttribute& Attribute, float Magnitude)
{
	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = Attribute;
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
}

void UShinobiEffect::AddSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
{
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = DataTag;
	FGameplayModifierInfo& Mod = Modifiers.AddDefaulted_GetRef();
	Mod.Attribute = Attribute;
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

void UShinobiEffect::SetDuration(float Seconds)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Seconds));
}

void UShinobiEffect::SetInfinitePeriodic(float PeriodSeconds)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(PeriodSeconds);
}

void UShinobiEffect::GrantTag(const FGameplayTag& Tag)
{
	if (!TargetTagsComponent)
	{
		TargetTagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
		GEComponents.Add(TargetTagsComponent);
	}
	FInheritedTagContainer Tags = TargetTagsComponent->GetConfiguredTargetTagChanges();
	Tags.AddTag(Tag);
	TargetTagsComponent->SetAndApplyTargetTagChanges(Tags);
}

UTargetTagRequirementsGameplayEffectComponent& UShinobiEffect::Requirements()
{
	if (!RequirementsComponent)
	{
		RequirementsComponent = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TagRequirements"));
		GEComponents.Add(RequirementsComponent);
	}
	return *RequirementsComponent;
}

void UShinobiEffect::IgnoreApplicationIfTargetHas(const FGameplayTag& Tag)
{
	Requirements().ApplicationTagRequirements.IgnoreTags.AddTag(Tag);
}

void UShinobiEffect::InhibitWhileTargetHas(const FGameplayTag& Tag)
{
	Requirements().OngoingTagRequirements.IgnoreTags.AddTag(Tag);
}

UGE_Damage::UGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCallerModifier(UShinobiAttributeSet::GetIncomingDamageAttribute(), ShinobiTags::Data_Damage);
	IgnoreApplicationIfTargetHas(ShinobiTags::State_Invulnerable);
}

UGE_ChakraGain::UGE_ChakraGain()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddSetByCallerModifier(UShinobiAttributeSet::GetChakraAttribute(), ShinobiTags::Data_ChakraGain);
}
```

- [ ] **Step 10: Write `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShinobiMovementComponent.generated.h"

/** Adds a sprint flag that travels in saved moves, so the server agrees with the client (no rubber-banding). */
UCLASS()
class SHINOBICOOP_API UShinobiMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UShinobiMovementComponent();

	void SetSprinting(bool bNewSprinting) { bWantsToSprint = bNewSprinting; }
	bool IsSprinting() const { return bWantsToSprint; }

	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SprintSpeed = 900.f;

	uint8 bWantsToSprint : 1;
};
```

- [ ] **Step 11: Write `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp`**

```cpp
#include "Shinobi/ShinobiMovementComponent.h"
#include "GameFramework/Character.h"

namespace
{
	class FShinobiSavedMove : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		uint8 bSavedWantsToSprint : 1;

		virtual void Clear() override
		{
			Super::Clear();
			bSavedWantsToSprint = false;
		}

		virtual uint8 GetCompressedFlags() const override
		{
			uint8 Result = Super::GetCompressedFlags();
			if (bSavedWantsToSprint)
			{
				Result |= FLAG_Custom_0;
			}
			return Result;
		}

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
		{
			if (bSavedWantsToSprint != static_cast<FShinobiSavedMove*>(NewMove.Get())->bSavedWantsToSprint)
			{
				return false;
			}
			return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
		}

		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
		{
			Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
			if (const UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(C->GetCharacterMovement()))
			{
				bSavedWantsToSprint = Movement->bWantsToSprint;
			}
		}

		virtual void PrepMoveFor(ACharacter* C) override
		{
			Super::PrepMoveFor(C);
			if (UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(C->GetCharacterMovement()))
			{
				Movement->bWantsToSprint = bSavedWantsToSprint;
			}
		}
	};

	class FShinobiPredictionData : public FNetworkPredictionData_Client_Character
	{
	public:
		explicit FShinobiPredictionData(const UCharacterMovementComponent& ClientMovement)
			: FNetworkPredictionData_Client_Character(ClientMovement)
		{
		}

		virtual FSavedMovePtr AllocateNewMove() override
		{
			return FSavedMovePtr(new FShinobiSavedMove());
		}
	};
}

UShinobiMovementComponent::UShinobiMovementComponent()
{
	bWantsToSprint = false;
	MaxWalkSpeed = 600.f;
	// Uncontrolled characters (dummies, knocked-back pawns, test actors) still need physics and root motion.
	bRunPhysicsWithNoController = true;
}

float UShinobiMovementComponent::GetMaxSpeed() const
{
	if (bWantsToSprint && MovementMode == MOVE_Walking)
	{
		return SprintSpeed;
	}
	return Super::GetMaxSpeed();
}

void UShinobiMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UShinobiMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UShinobiMovementComponent* MutableThis = const_cast<UShinobiMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FShinobiPredictionData(*this);
	}
	return ClientPredictionData;
}

bool UShinobiMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	// Super replays unacked moves via PrepMoveFor/UpdateFromCompressedFlags, which overwrites
	// bWantsToSprint with each replayed move's flag in turn. The engine does the same
	// save/restore for bWantsToCrouch around this call (CharacterMovementComponent.cpp), so we
	// mirror that here: the last replayed move would otherwise win instead of the current input.
	const bool bRealWantsToSprint = bWantsToSprint;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealWantsToSprint;
	return bResult;
}
```

- [ ] **Step 12: Write `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ShinobiGameplayAbility.generated.h"

class AShinobiCharacterBase;
class UShinobiMovementComponent;

/** Input slots abilities bind to. Values are passed to the ASC as InputID. */
UENUM(BlueprintType)
enum class EShinobiAbilityInputID : uint8
{
	None,
	Melee,
	Dodge,
	Sprint,
	ChakraCharge
};

/** Base for all Shinobi abilities: instanced per actor, predicted on the owning client. */
UCLASS(Abstract)
class SHINOBICOOP_API UShinobiGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UShinobiGameplayAbility();

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	EShinobiAbilityInputID InputID = EShinobiAbilityInputID::None;

protected:
	AShinobiCharacterBase* GetShinobiCharacter() const;
	UShinobiMovementComponent* GetShinobiMovement() const;

	/** Applies an effect class (no SetByCaller) to the owner. */
	FActiveGameplayEffectHandle ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass) const;

	/** Server only. Applies GE_Damage with Amount from the owner to Target. Returns whether the hit
	 * actually landed (false if not authority, no target, or the effect was blocked, e.g. by invulnerability). */
	bool ApplyDamage(AShinobiCharacterBase* Target, float Amount) const;

	/** Hostile characters within Range (2D) whose direction is within HalfAngleDegrees of the owner's forward. */
	TArray<AShinobiCharacterBase*> FindHostilesInCone(float Range, float HalfAngleDegrees) const;
};
```

- [ ] **Step 13: Write `Source/ShinobiCoop/Shinobi/Abilities/ShinobiGameplayAbility.cpp`**

```cpp
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"

UShinobiGameplayAbility::UShinobiGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

AShinobiCharacterBase* UShinobiGameplayAbility::GetShinobiCharacter() const
{
	return Cast<AShinobiCharacterBase>(GetAvatarActorFromActorInfo());
}

UShinobiMovementComponent* UShinobiGameplayAbility::GetShinobiMovement() const
{
	const AShinobiCharacterBase* Character = GetShinobiCharacter();
	return Character ? Cast<UShinobiMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

FActiveGameplayEffectHandle UShinobiGameplayAbility::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass) const
{
	if (!ensure(EffectClass))
	{
		return FActiveGameplayEffectHandle();
	}
	return ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, EffectClass->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel());
}

bool UShinobiGameplayAbility::ApplyDamage(AShinobiCharacterBase* Target, float Amount) const
{
	// Predicted (LocalPredicted) abilities run this on the owning client too; without this guard
	// the client would apply damage to Target locally in addition to the server's authoritative copy.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}
	UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();
	if (!Source || !Target)
	{
		return false;
	}
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UGE_Damage::StaticClass(), GetAbilityLevel());
	Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Damage, Amount);
	// For an instant effect, WasSuccessfullyApplied() is true once it executes (GetInstantExecutedHandle());
	// it comes back false (default-constructed handle) if a GE component blocked application, e.g. GE_Damage's
	// own IgnoreApplicationIfTargetHas(State.Invulnerable) requirement.
	const FActiveGameplayEffectHandle Handle = Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target->GetAbilitySystemComponent());
	return Handle.WasSuccessfullyApplied();
}

TArray<AShinobiCharacterBase*> UShinobiGameplayAbility::FindHostilesInCone(float Range, float HalfAngleDegrees) const
{
	TArray<AShinobiCharacterBase*> Result;
	const AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self)
	{
		return Result;
	}
	const FVector Origin = Self->GetActorLocation();
	const FVector Forward = Self->GetActorForwardVector().GetSafeNormal2D();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));

	// ponytail: linear scan over all characters; fine for dozens, switch to an overlap query if counts grow
	for (TActorIterator<AShinobiCharacterBase> It(Self->GetWorld()); It; ++It)
	{
		AShinobiCharacterBase* Other = *It;
		if (!Self->IsHostileTo(Other))
		{
			continue;
		}
		const FVector ToOther = (Other->GetActorLocation() - Origin) * FVector(1.f, 1.f, 0.f);
		const float Distance = ToOther.Size();
		if (Distance > Range)
		{
			continue;
		}
		if (Distance > KINDA_SMALL_NUMBER && FVector::DotProduct(ToOther / Distance, Forward) < MinDot)
		{
			continue;
		}
		Result.Add(Other);
	}
	return Result;
}
```

- [ ] **Step 14: Write `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ShinobiCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UShinobiAttributeSet;
class UShinobiGameplayAbility;

UENUM(BlueprintType)
enum class EShinobiTeam : uint8
{
	Players,
	Enemies
};

/** Character with an ability system, shared by players, enemies and dummies. */
UCLASS()
class SHINOBICOOP_API AShinobiCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AShinobiCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UShinobiAttributeSet* GetAttributes() const { return Attributes; }

	EShinobiTeam GetTeam() const { return Team; }
	bool IsHostileTo(const AShinobiCharacterBase* Other) const { return Other && Other != this && Other->Team != Team; }

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	/** Server only. Called each time Health reaches 0. */
	virtual void HandleOutOfHealth();

	/** Constructor-only: Manny mesh + combat anim blueprint from the template content. */
	void UseMannequinMesh();

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UShinobiAttributeSet> Attributes;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	EShinobiTeam Team = EShinobiTeam::Enemies;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSubclassOf<UShinobiGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

private:
	void GrantStartupAbilitiesAndEffects();
	bool bStartupGranted = false;
};
```

- [ ] **Step 15: Write `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.cpp`**

```cpp
#include "Shinobi/ShinobiCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"

AShinobiCharacterBase::AShinobiCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShinobiMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	Attributes = CreateDefaultSubobject<UShinobiAttributeSet>(TEXT("Attributes"));
}

UAbilitySystemComponent* AShinobiCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AShinobiCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem->InitAbilityActorInfo(this, this);
	if (HasAuthority())
	{
		Attributes->OnOutOfHealth.AddUObject(this, &AShinobiCharacterBase::HandleOutOfHealth);
		GrantStartupAbilitiesAndEffects();
	}
}

void AShinobiCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AbilitySystem->RefreshAbilityActorInfo();
}

void AShinobiCharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	AbilitySystem->RefreshAbilityActorInfo();
}

void AShinobiCharacterBase::HandleOutOfHealth()
{
	UE_LOG(LogShinobiCoop, Log, TEXT("%s is out of health"), *GetName());
}

void AShinobiCharacterBase::UseMannequinMesh()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> MannyAnim(TEXT("/Game/Variant_Combat/Anims/ABP_Manny_Combat"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(MannyMesh.Object);
	}
	if (MannyAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(MannyAnim.Class);
	}
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
}

void AShinobiCharacterBase::GrantStartupAbilitiesAndEffects()
{
	if (bStartupGranted)
	{
		return;
	}
	bStartupGranted = true;

	for (const TSubclassOf<UShinobiGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!ensureMsgf(AbilityClass, TEXT("%s has an empty StartupAbilities entry"), *GetName()))
		{
			continue;
		}
		const UShinobiGameplayAbility* Ability = AbilityClass->GetDefaultObject<UShinobiGameplayAbility>();
		// Input ID 0 is a valid ASC input slot; None must map to INDEX_NONE so a stray
		// AbilityLocalInputPressed(0) doesn't activate every unbound ability.
		const int32 InputID = (Ability->InputID == EShinobiAbilityInputID::None) ? INDEX_NONE : static_cast<int32>(Ability->InputID);
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, InputID, this));
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : StartupEffects)
	{
		if (!ensureMsgf(EffectClass, TEXT("%s has an empty StartupEffects entry"), *GetName()))
		{
			continue;
		}
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.f, AbilitySystem->MakeEffectContext());
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}
```

- [ ] **Step 16: Build and run tests**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected:
```
Success  ShinobiCoop.Core.AttributeDefaults
Success  ShinobiCoop.Core.ChakraGainClampsToMax
Success  ShinobiCoop.Core.DamageReducesHealthAndClampsAtZero
Success  ShinobiCoop.Core.OutOfHealthFiresOncePerDeath
ALL 4 TESTS PASSED
```
If `run_tests.ps1` reports a GE component warning in the log (`should be added to GEComponents`), `GrantTag`/`Requirements` is not adding the component to `GEComponents`; fix before continuing.

- [ ] **Step 17: Commit**

```powershell
git add Source/ShinobiCoop/Shinobi Source/ShinobiCoop/Tests
git commit -m "feat: GAS attribute set, damage effect and base character

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Invulnerability and regeneration effects

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h`, `ShinobiEffects.cpp` (append classes)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCharacterBase.cpp` (startup regen effects)
- Modify: `Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp` (append tests)

- [ ] **Step 1: Append failing tests to `ShinobiAttributeTests.cpp` (inside the `#if`, before the final `#endif`)**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMaxShrinkClampsTest, "ShinobiCoop.Core.LoweringMaxClampsCurrent", ShinobiTestFlags)
bool FShinobiMaxShrinkClampsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Set(C, UShinobiAttributeSet::GetMaxHealthAttribute(), 300.f);
	TestEqual(TEXT("Health clamped to lowered max"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 300.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiInvulnerableTest, "ShinobiCoop.Core.InvulnerableIgnoresDamage", ShinobiTestFlags)
bool FShinobiInvulnerableTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::ApplyEffect(C, UGE_DodgeIFrames::StaticClass());
	ShinobiTest::Damage(C, 50.f);
	TestEqual(TEXT("No damage during i-frames"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	W.Tick(0.4f);
	TestFalse(TEXT("I-frames expired"), ShinobiTest::HasTag(C, ShinobiTags::State_Invulnerable));
	ShinobiTest::Damage(C, 50.f);
	TestEqual(TEXT("Damage after i-frames"), ShinobiTest::Get(C, UShinobiAttributeSet::GetHealthAttribute()), 450.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraRegenTest, "ShinobiCoop.Core.ChakraRegensTwoPerSecond", ShinobiTestFlags)
bool FShinobiChakraRegenTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	W.Tick(0.1f); // Let the deferred on-application tick from the startup UGE_ChakraRegen happen first.
	ShinobiTest::Set(C, UShinobiAttributeSet::GetChakraAttribute(), 0.f);
	W.Tick(1.f);
	const float Chakra = ShinobiTest::Get(C, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("Chakra ~2 after 1s (got %.2f)"), Chakra), FMath::IsNearlyEqual(Chakra, 2.f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStaminaRegenDelayTest, "ShinobiCoop.Core.StaminaRegenWaitsForDelay", ShinobiTestFlags)
bool FShinobiStaminaRegenDelayTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* C = W.Spawn<AShinobiCharacterBase>();
	ShinobiTest::Set(C, UShinobiAttributeSet::GetStaminaAttribute(), 50.f);
	ShinobiTest::ApplyEffect(C, UGE_StaminaRegenDelay::StaticClass());
	W.Tick(0.9f);
	TestEqual(TEXT("No regen during delay"), ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute()), 50.f);
	W.Tick(0.6f);
	const float Stamina = ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute());
	TestTrue(FString::Printf(TEXT("Regen resumed (got %.2f)"), Stamina), Stamina > 50.f);

	// The block should last 1 s from the LATEST application, not the first: each application is its own
	// instance, so re-applying mid-block keeps the tag granted past when the first instance would have expired.
	ShinobiTest::Set(C, UShinobiAttributeSet::GetStaminaAttribute(), 50.f);
	ShinobiTest::ApplyEffect(C, UGE_StaminaRegenDelay::StaticClass());
	W.Tick(0.5f);
	ShinobiTest::ApplyEffect(C, UGE_StaminaRegenDelay::StaticClass());
	W.Tick(0.9f);
	TestEqual(TEXT("Still blocked past the first instance's expiry"), ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute()), 50.f);
	W.Tick(0.6f);
	const float StaminaAfterReapply = ShinobiTest::Get(C, UShinobiAttributeSet::GetStaminaAttribute());
	TestTrue(FString::Printf(TEXT("Regen resumed after latest delay (got %.2f)"), StaminaAfterReapply), StaminaAfterReapply > 50.f);
	return true;
}
```

- [ ] **Step 2: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL, `'UGE_DodgeIFrames': undeclared identifier`.

- [ ] **Step 3: Append effect declarations to `ShinobiEffects.h`**

```cpp
/** Passive chakra regeneration: +2/s. Applied at spawn. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraRegen : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraRegen();
};

/** Passive stamina regeneration: +20/s, paused while State.StaminaRegenBlocked. Applied at spawn. */
UCLASS()
class SHINOBICOOP_API UGE_StaminaRegen : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_StaminaRegen();
};

/** Apply after spending stamina; each application is its own instance, so the block lasts 1 s from the latest one. */
UCLASS()
class SHINOBICOOP_API UGE_StaminaRegenDelay : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_StaminaRegenDelay();
};

/** While sprinting: -10 stamina/s, blocks regen, grants State.Sprinting. */
UCLASS()
class SHINOBICOOP_API UGE_SprintDrain : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_SprintDrain();
};

/** Dodge cost: -25 stamina (used as the ability's cost, so dodge is blocked below 25). */
UCLASS()
class SHINOBICOOP_API UGE_DodgeCost : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_DodgeCost();
};

/** Dodge invulnerability window: 0.2 s of State.Invulnerable. */
UCLASS()
class SHINOBICOOP_API UGE_DodgeIFrames : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_DodgeIFrames();
};

/** While charging chakra: +25 chakra/s. */
UCLASS()
class SHINOBICOOP_API UGE_ChakraCharge : public UShinobiEffect
{
	GENERATED_BODY()
public:
	UGE_ChakraCharge();
};
```

- [ ] **Step 4: Append effect definitions to `ShinobiEffects.cpp`**

```cpp
// Rates are applied every 0.25 s, so per-period values are a quarter of the per-second rate.
UGE_ChakraRegen::UGE_ChakraRegen()
{
	SetInfinitePeriodic(0.25f);
	AddModifier(UShinobiAttributeSet::GetChakraAttribute(), 0.5f);
}

UGE_StaminaRegen::UGE_StaminaRegen()
{
	SetInfinitePeriodic(0.25f);
	AddModifier(UShinobiAttributeSet::GetStaminaAttribute(), 5.f);
	InhibitWhileTargetHas(ShinobiTags::State_StaminaRegenBlocked);
	// Resume immediately when the block is lifted instead of waiting up to one more period.
	// bExecutePeriodicEffectOnApplication defaults to true, which would also fire immediately when this
	// effect is first applied at spawn; ExecuteAndResetPeriod already provides that first free tick via the
	// initial un-inhibit (every new active effect starts inhibited and is un-inhibited on the same apply), so
	// leaving both on would double-execute at t=0.
	PeriodicInhibitionPolicy = EGameplayEffectPeriodInhibitionRemovedPolicy::ExecuteAndResetPeriod;
	bExecutePeriodicEffectOnApplication = false;
}

UGE_StaminaRegenDelay::UGE_StaminaRegenDelay()
{
	SetDuration(1.f);
	GrantTag(ShinobiTags::State_StaminaRegenBlocked);
}

UGE_SprintDrain::UGE_SprintDrain()
{
	SetInfinitePeriodic(0.25f);
	AddModifier(UShinobiAttributeSet::GetStaminaAttribute(), -2.5f);
	GrantTag(ShinobiTags::State_StaminaRegenBlocked);
	GrantTag(ShinobiTags::State_Sprinting);
}

UGE_DodgeCost::UGE_DodgeCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddModifier(UShinobiAttributeSet::GetStaminaAttribute(), -25.f);
}

UGE_DodgeIFrames::UGE_DodgeIFrames()
{
	SetDuration(0.2f);
	GrantTag(ShinobiTags::State_Invulnerable);
}

UGE_ChakraCharge::UGE_ChakraCharge()
{
	SetInfinitePeriodic(0.25f);
	AddModifier(UShinobiAttributeSet::GetChakraAttribute(), 6.25f);
	// Without this, bExecutePeriodicEffectOnApplication's default (true) grants a free +6.25 on every
	// activation, so tapping the ability repeatedly beats holding it. Gains should only come from a full period.
	bExecutePeriodicEffectOnApplication = false;
}
```

- [ ] **Step 5: Apply the regen effects at spawn**

In `ShinobiCharacterBase.cpp`, add to the end of the constructor body:
```cpp
	StartupEffects = { UGE_ChakraRegen::StaticClass(), UGE_StaminaRegen::StaticClass() };
```

- [ ] **Step 6: Build and run tests**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 8 TESTS PASSED` (4 earlier + `LoweringMaxClampsCurrent`, `InvulnerableIgnoresDamage`, `ChakraRegensTwoPerSecond`, `StaminaRegenWaitsForDelay`).

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop
git commit -m "feat: regen, i-frame, sprint and dodge gameplay effects

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Sprint ability

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Sprint.h`, `GA_Sprint.cpp`
- Create: `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp`
- Create (minimal, finished in Task 10): `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, `ShinobiPlayerCharacter.cpp`

- [ ] **Step 1: Write failing tests `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp`**

```cpp
#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Abilities/GA_Sprint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintTest, "ShinobiCoop.Abilities.SprintDrainsStaminaThenRegenAfterDelay", ShinobiTestFlags)
bool FShinobiSprintTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	// Max speed is not asserted: test actors have no floor, so they are falling and sprint speed only applies when walking.
	TestTrue(TEXT("Movement sprinting"), Move->IsSprinting());

	W.Tick(1.f);
	const float AfterSprint = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	TestTrue(FString::Printf(TEXT("Stamina drained ~10 (got %.2f)"), AfterSprint), AfterSprint >= 86.f && AfterSprint <= 91.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Sprint ended on release"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestFalse(TEXT("Movement not sprinting"), Move->IsSprinting());

	W.Tick(0.8f);
	TestEqual(TEXT("Regen delayed after sprint"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), AfterSprint);
	W.Tick(0.5f);
	TestTrue(TEXT("Regen resumed"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()) > AfterSprint);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintExhaustTest, "ShinobiCoop.Abilities.SprintEndsAtZeroStamina", ShinobiTestFlags)
bool FShinobiSprintExhaustTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 5.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	W.Tick(1.f);
	TestFalse(TEXT("Sprint ended when exhausted"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestEqual(TEXT("Stamina empty"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 0.f);
	TestFalse(TEXT("Movement not sprinting after exhaustion"), Move->IsSprinting());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Can't start sprint at 0 stamina"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintCancelTest, "ShinobiCoop.Abilities.SprintCancelClearsFlagAndDelaysRegen", ShinobiTestFlags)
bool FShinobiSprintCancelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	W.Tick(0.3f);

	FGameplayTagContainer Tags(ShinobiTags::Ability_Sprint);
	P->GetAbilitySystemComponent()->CancelAbilities(&Tags);

	TestFalse(TEXT("Sprint cancelled"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestFalse(TEXT("Movement not sprinting after cancel"), Move->IsSprinting());

	const float S = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	W.Tick(0.8f);
	TestEqual(TEXT("Regen delayed after cancel"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), S);
	W.Tick(0.5f);
	TestTrue(TEXT("Regen resumed after cancel"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()) > S);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL, `cannot open include file 'Shinobi/ShinobiPlayerCharacter.h'`.

- [ ] **Step 3: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Sprint.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Sprint.generated.h"

/** Hold to sprint. Drains stamina; ends on release or at 0 stamina, then delays stamina regen. */
UCLASS()
class SHINOBICOOP_API UGA_Sprint : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Sprint();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnStaminaThreshold(bool bMatchesComparison, float CurrentValue);

	void SetSprinting(bool bSprinting) const;

	FActiveGameplayEffectHandle DrainHandle;
	bool bSprintStarted = false;
};
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Sprint.cpp`**

```cpp
#include "Shinobi/Abilities/GA_Sprint.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeThreshold.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"

UGA_Sprint::UGA_Sprint()
{
	InputID = EShinobiAbilityInputID::Sprint;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Sprint));
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
}

bool UGA_Sprint::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	// Gating here (rather than in ActivateAbility) means a client at 0 stamina never sends the
	// activation, and a server-only 0 (lag) is rejected via ClientActivateAbilityFailed instead of
	// charging the drain effect and stamina-regen delay for an ability that never really started.
	return ASC && ASC->GetNumericAttribute(UShinobiAttributeSet::GetStaminaAttribute()) > 0.f;
}

void UGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bSprintStarted = true;
	SetSprinting(true);
	if (HasAuthority(&ActivationInfo))
	{
		DrainHandle = ApplyEffectToSelf(UGE_SprintDrain::StaticClass());
	}

	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	Release->OnRelease.AddDynamic(this, &UGA_Sprint::OnInputReleased);
	Release->ReadyForActivation();

	// Broadcasts immediately with the current comparison result, then whenever it flips.
	UAbilityTask_WaitAttributeChangeThreshold* Exhausted = UAbilityTask_WaitAttributeChangeThreshold::WaitForAttributeChangeThreshold(
		this, UShinobiAttributeSet::GetStaminaAttribute(), EWaitAttributeChangeComparison::LessThanOrEqualTo, 0.f, false);
	Exhausted->OnChange.AddDynamic(this, &UGA_Sprint::OnStaminaThreshold);
	Exhausted->ReadyForActivation();
}

void UGA_Sprint::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Sprint::OnStaminaThreshold(bool bMatchesComparison, float CurrentValue)
{
	if (bMatchesComparison)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bSprintStarted)
	{
		bSprintStarted = false;
		SetSprinting(false);
		if (HasAuthority(&ActivationInfo))
		{
			// Apply the regen delay before removing the drain: both grant State.StaminaRegenBlocked,
			// and a granted tag is a count, so applying first keeps the count >= 1 the whole time.
			// Doing it the other way briefly drops the count to 0, un-inhibiting UGE_StaminaRegen for a frame.
			ApplyEffectToSelf(UGE_StaminaRegenDelay::StaticClass());
			if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
			{
				ASC->RemoveActiveGameplayEffect(DrainHandle);
			}
			DrainHandle.Invalidate();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Sprint::SetSprinting(bool bSprinting) const
{
	if (UShinobiMovementComponent* Movement = GetShinobiMovement())
	{
		Movement->SetSprinting(bSprinting);
	}
}
```

- [ ] **Step 5: Write a minimal `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`** (camera and input arrive in Task 10)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "ShinobiPlayerCharacter.generated.h"

/** Player-controlled shinobi. */
UCLASS()
class SHINOBICOOP_API AShinobiPlayerCharacter : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer);
};
```

- [ ] **Step 6: Write a minimal `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`**

```cpp
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/Abilities/GA_Sprint.h"

AShinobiPlayerCharacter::AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	StartupAbilities = { UGA_Sprint::StaticClass() };
}
```

- [ ] **Step 7: Build and run tests**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 11 TESTS PASSED` (10 earlier + `SprintCancelClearsFlagAndDelaysRegen`).

- [ ] **Step 8: Commit**

```powershell
git add Source/ShinobiCoop
git commit -m "feat: replicated sprint ability with stamina drain

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Dodge ability

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.h`, `GA_Dodge.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp` (grant ability)
- Modify: `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp` (append test)

- [ ] **Step 1: Append failing test to `ShinobiAbilityTests.cpp`**

Add `#include "Shinobi/Abilities/GA_Dodge.h"` next to the other includes (inside the `#if`), then append before the final `#endif`:
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDodgeTest, "ShinobiCoop.Abilities.DodgeCostsStaminaAndGrantsIFrames", ShinobiTestFlags)
bool FShinobiDodgeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FVector Start = P->GetActorLocation();

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active before dodge"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	const float Before = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Dodge active"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("Dodge cancels sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestEqual(TEXT("Dodge cost 25"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), Before - 25.f);
	TestTrue(TEXT("Invulnerable"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));
	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("I-frames block damage"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);

	W.Tick(0.15f);
	TestTrue(TEXT("Still invulnerable mid-dodge"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));
	W.Tick(0.25f);
	TestFalse(TEXT("Dodge finished"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("I-frames over"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));

	const FVector Displacement = (P->GetActorLocation() - Start) * FVector(1.f, 1.f, 0.f);
	TestTrue(FString::Printf(TEXT("Backstep ~600 on -X (got %s)"), *Displacement.ToString()), Displacement.X < -550.f && Displacement.X > -700.f);

	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("Damage after dodge"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 450.f);

	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 20.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Dodge blocked below 25 stamina"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestEqual(TEXT("No stamina spent"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 20.f);

	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 25.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Dodge active at exactly 25 stamina"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestEqual(TEXT("Stamina exactly spent"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 0.f);
	return true;
}
```

(Dodge cancelling Sprint takes effect before `GA_Dodge::ActivateAbility` runs — `CancelAbilitiesWithTag` is applied in `UGameplayAbility::PreActivate`, ahead of `ActivateAbility`/`CommitAbility` — so `Before` already reflects Sprint's own instant drain tick; the dodge cost is asserted relative to it rather than against a hardcoded 100-stamina baseline. Observed backstep displacement in this test world: ~630 units on -X, comfortably inside the 550-700 tolerance band, which also covers the residual drift from the velocity-clamp tail after the root motion force's `OnFinish`.)

- [ ] **Step 2: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL, `cannot open include file 'Shinobi/Abilities/GA_Dodge.h'`.

- [ ] **Step 3: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Dodge.generated.h"

class UAnimMontage;

/** Quick dash in the movement direction (backstep with no input). Costs stamina; brief invulnerability. */
UCLASS()
class SHINOBICOOP_API UGA_Dodge : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Dodge();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float Distance = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi", meta = (ClampMin = "0.05"))
	float Duration = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TSoftObjectPtr<UAnimMontage> Montage;

private:
	UFUNCTION()
	void OnDodgeFinished();
};
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Dodge.cpp`**

```cpp
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Dodge::UGA_Dodge()
{
	InputID = EShinobiAbilityInputID::Dodge;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Dodge));
	ActivationOwnedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Melee);
	CostGameplayEffectClass = UGE_DodgeCost::StaticClass();
	Montage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Dodge.AM_Dodge")));
}

void UGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AShinobiCharacterBase* Character = GetShinobiCharacter();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		ApplyEffectToSelf(UGE_DodgeIFrames::StaticClass());
		ApplyEffectToSelf(UGE_StaminaRegenDelay::StaticClass());
	}

	FVector Direction = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = -Character->GetActorForwardVector().GetSafeNormal2D(); // backstep
	}

	if (UAnimMontage* Anim = Montage.LoadSynchronous())
	{
		// bStopWhenAbilityEnds = false: the 0.25 s dash ends the ability well before the ~0.97 s
		// animation finishes, and the animation is cosmetic only (root motion is disabled on the
		// duplicated sequence in Task 12), so let it play out instead of cutting it off.
		UAbilityTask_PlayMontageAndWait* Play = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Anim, 1.f, NAME_None, false);
		Play->ReadyForActivation();
	}

	UAbilityTask_ApplyRootMotionConstantForce* Push = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Dodge"), Direction, Distance / Duration, Duration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 200.f, true);
	Push->OnFinish.AddDynamic(this, &UGA_Dodge::OnDodgeFinished);
	Push->ReadyForActivation();
}

void UGA_Dodge::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```

- [ ] **Step 5: Grant the ability** — in `ShinobiPlayerCharacter.cpp` add `#include "Shinobi/Abilities/GA_Dodge.h"` and change the startup list to:

```cpp
	StartupAbilities = { UGA_Sprint::StaticClass(), UGA_Dodge::StaticClass() };
```

- [ ] **Step 6: Build and run tests**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 12 TESTS PASSED`. (The montage doesn't exist yet; `LoadSynchronous` returns null and the dodge still works.)

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop
git commit -m "feat: dodge ability with stamina cost and i-frames

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Chakra charge ability

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.h`, `GA_ChakraCharge.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp`

- [ ] **Step 1: Append failing test**

Add `#include "Shinobi/Abilities/GA_ChakraCharge.h"` with the other includes, then append:
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraChargeTest, "ShinobiCoop.Abilities.ChakraChargeRefillsWhileHeld", ShinobiTestFlags)
bool FShinobiChakraChargeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();

	// At full chakra (the default), pressing charge should do nothing: no ability, no tag.
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Can't charge at full chakra"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("No charging tag at full chakra"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);

	ShinobiTest::Set(P, UShinobiAttributeSet::GetChakraAttribute(), 0.f);

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charging tag"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	W.Tick(1.1f);
	const float Charged = ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("~27.5 chakra after 1.1s charge (got %.2f)"), Charged), Charged >= 26.5f && Charged <= 28.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge ended"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag removed"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	W.Tick(0.5f);
	TestTrue(TEXT("Only passive regen after release"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()) <= Charged + 1.5f);

	// Chakra is well below max here, so charging interacts normally with sprint/dodge.
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active before charge"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge cancels sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestTrue(TEXT("Charge active"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Sprint blocked while charging"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Dodge cancels charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag removed by dodge"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));

	// Let the dodge finish: GA_ChakraCharge blocks activation while State.Dodging is present, and
	// that tag stays granted for Dodge's full 0.25 s duration, not just until it cancels the charge.
	W.Tick(0.4f);
	TestFalse(TEXT("Dodge finished before next charge attempt"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));

	// Auto-end when charging pushes chakra up to the cap.
	ShinobiTest::Set(P, UShinobiAttributeSet::GetChakraAttribute(), 95.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charge started from 95"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	W.Tick(0.6f);
	TestFalse(TEXT("Charge auto-ends at full chakra"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestEqual(TEXT("Chakra capped at max"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	return true;
}
```

(The deterministic ~27 value observed at review time was actually 27.5 — 4 periodic charge ticks of +6.25 plus 5 periodic regen ticks of +0.5, since `UGE_ChakraRegen`'s own free application-time tick, granted at character spawn, doesn't fire until the first `W.Tick` call in this test and lands inside the 1.1 s window. The `[26.5, 28.0]` bound and message text both already account for this.)

- [ ] **Step 2: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL, `cannot open include file 'Shinobi/Abilities/GA_ChakraCharge.h'`.

- [ ] **Step 3: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_ChakraCharge.generated.h"

/** Hold to refill chakra quickly. Movement input is ignored while State.ChakraCharging is present. */
UCLASS()
class SHINOBICOOP_API UGA_ChakraCharge : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ChakraCharge();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnChakraFull(bool bMatchesComparison, float CurrentValue);

	FActiveGameplayEffectHandle ChargeHandle;
};
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_ChakraCharge.cpp`**

```cpp
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeThreshold.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_ChakraCharge::UGA_ChakraCharge()
{
	InputID = EShinobiAbilityInputID::ChakraCharge;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_ChakraCharge));
	ActivationOwnedTags.AddTag(ShinobiTags::State_ChakraCharging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
}

bool UGA_ChakraCharge::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	// Pressing the key at full chakra should do nothing at all: no sprint cancel, no movement lock.
	return ASC && ASC->GetNumericAttribute(UShinobiAttributeSet::GetChakraAttribute()) < ASC->GetNumericAttribute(UShinobiAttributeSet::GetMaxChakraAttribute());
}

void UGA_ChakraCharge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (HasAuthority(&ActivationInfo))
	{
		ChargeHandle = ApplyEffectToSelf(UGE_ChakraCharge::StaticClass());
	}
	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	Release->OnRelease.AddDynamic(this, &UGA_ChakraCharge::OnInputReleased);
	Release->ReadyForActivation();

	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	// MaxChakra is read once at activation; if Max can change mid-charge later, compare against the live value.
	const float MaxChakra = ASC ? ASC->GetNumericAttribute(UShinobiAttributeSet::GetMaxChakraAttribute()) : 0.f;
	// Broadcasts immediately with the current comparison result (false: CanActivateAbility guarantees
	// Chakra < MaxChakra here), then again whenever charging pushes Chakra up to the cap.
	UAbilityTask_WaitAttributeChangeThreshold* Full = UAbilityTask_WaitAttributeChangeThreshold::WaitForAttributeChangeThreshold(
		this, UShinobiAttributeSet::GetChakraAttribute(), EWaitAttributeChangeComparison::GreaterThanOrEqualTo, MaxChakra, false);
	Full->OnChange.AddDynamic(this, &UGA_ChakraCharge::OnChakraFull);
	Full->ReadyForActivation();
}

void UGA_ChakraCharge::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ChakraCharge::OnChakraFull(bool bMatchesComparison, float CurrentValue)
{
	if (bMatchesComparison)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_ChakraCharge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ChargeHandle.IsValid() && HasAuthority(&ActivationInfo))
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffect(ChargeHandle);
		}
		ChargeHandle.Invalidate();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

- [ ] **Step 5: Grant the ability** — in `ShinobiPlayerCharacter.cpp` add `#include "Shinobi/Abilities/GA_ChakraCharge.h"` and set:

```cpp
	StartupAbilities = { UGA_Sprint::StaticClass(), UGA_Dodge::StaticClass(), UGA_ChakraCharge::StaticClass() };
```

- [ ] **Step 6: Build and run tests**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 13 TESTS PASSED`.

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop
git commit -m "feat: chakra charge ability

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Melee combo ability

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Abilities/GA_Melee.h`, `GA_Melee.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiAbilityTests.cpp`

- [ ] **Step 1: Append failing test**

Add `#include "Shinobi/Abilities/GA_Melee.h"` with the other includes, then append:
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeleeComboTest, "ShinobiCoop.Abilities.MeleeComboHitsHostilesOnly", ShinobiTestFlags)
bool FShinobiMeleeComboTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Attacker = W.Spawn<AShinobiPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator); // faces +X
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 0.f, 0.f));
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(120.f, 40.f, 0.f));
	AShinobiCharacterBase* FarEnemy = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	ShinobiTest::Set(Attacker, UShinobiAttributeSet::GetChakraAttribute(), 50.f);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 1: 20 damage"), ShinobiTest::Get(Enemy, Health), 480.f);
	TestEqual(TEXT("Ally untouched"), ShinobiTest::Get(Ally, Health), 500.f);
	TestEqual(TEXT("Far enemy untouched"), ShinobiTest::Get(FarEnemy, Health), 500.f);
	const float Chakra = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("+5 chakra on hit (got %.2f)"), Chakra), Chakra >= 55.f && Chakra <= 56.5f);

	// Attacker chakra is 55.5, well below the 100 max, so this can only be blocked by GA_Melee's
	// BlockAbilitiesWithTag, not by GA_ChakraCharge's own full-chakra gate.
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge blocked mid-combo"), ShinobiTest::IsActive(Attacker, UGA_ChakraCharge::StaticClass()));
	ShinobiTest::Release(Attacker, EShinobiAbilityInputID::ChakraCharge);

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 2: 25 damage"), ShinobiTest::Get(Enemy, Health), 455.f);

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 3: 40 damage"), ShinobiTest::Get(Enemy, Health), 415.f);
	// LaunchCharacter only sets PendingLaunchVelocity; CharacterMovementComponent applies it to Velocity
	// on its next tick, which lands after this one (the hit frame's timer fires from FTimerManager::Tick,
	// which runs after this substep's tick groups, i.e. after this substep's movement tick already ran).
	W.Tick(0.1f);
	TestTrue(FString::Printf(TEXT("Finisher knockback (got %.2f)"), Enemy->GetVelocity().X), Enemy->GetVelocity().X > 100.f);

	W.Tick(0.5f);
	TestFalse(TEXT("Combo ended"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeleeWindowTest, "ShinobiCoop.Abilities.MeleeComboResetsAfterWindow", ShinobiTestFlags)
bool FShinobiMeleeWindowTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Attacker = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(1.f); // hit at 0.25, window closes at 0.75
	TestFalse(TEXT("Ended after window"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("New combo restarts at hit 1 (20 + 20)"), ShinobiTest::Get(Enemy, Health), 460.f);

	// Invulnerable target: the hit is blocked, so it doesn't count -- no chakra for the attacker.
	// Chakra must be well below max here, or a wrongly-counted +5 would just get clamped away and
	// this assertion would pass regardless of whether the landed-hit gating actually works.
	Enemy->GetAbilitySystemComponent()->AddLooseGameplayTag(ShinobiTags::State_Invulnerable);
	ShinobiTest::Set(Attacker, UShinobiAttributeSet::GetChakraAttribute(), 50.f);
	const float ChakraBeforeBlockedHit = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute());
	W.Tick(0.7f); // let the still-active combo's window close (it's at 0.3 of 0.75s; extra margin for timer jitter)
	TestFalse(TEXT("Second combo ended before third attempt"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Invulnerable target untouched"), ShinobiTest::Get(Enemy, Health), 460.f);
	const float ChakraGainedFromBlockedHit = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute()) - ChakraBeforeBlockedHit;
	TestTrue(FString::Printf(TEXT("No chakra from a blocked hit, only passive regen (got +%.2f)"), ChakraGainedFromBlockedHit), ChakraGainedFromBlockedHit < 5.f);
	Enemy->GetAbilitySystemComponent()->RemoveLooseGameplayTag(ShinobiTags::State_Invulnerable);
	return true;
}
```

(The finisher-knockback and window-close timings above have a `W.Tick(0.1f)`/`W.Tick(0.7f)` margin baked in for two observed timing quirks: `ACharacter::LaunchCharacter` only sets `PendingLaunchVelocity`, which `UCharacterMovementComponent` applies to `Velocity` on its *next* movement tick — and since `FTimerManager::Tick` (which fires `UAbilityTask_WaitDelay`'s callback) runs after that same substep's tick groups, the launch's effect on `GetVelocity()` is one `W.Tick` step behind the hit frame that triggered it. The same one-step-behind timer firing also means a `WaitDelay`'s expiry can land on the *last* sub-step of a `W.Tick` call rather than mid-way through it, so window-close checks need a bit more margin than the nominal `HitDelay + ComboWindow` sum.)

(The invulnerable-target section's `ShinobiTest::Set(..., 50.f)` matters: without resetting chakra away from its 100/100 max before that section, a wrongly-counted +5 chakra gain would just get clamped away by the Max attribute, and `ChakraGainedFromBlockedHit < 5.f` would pass whether or not the landed-hit gating in `OnHitFrame` actually works. Verified at review time by temporarily making `OnHitFrame` count every target instead of only landed ones: the test then failed with `got +7.00` (2.00 of passive `UGE_ChakraRegen` ticks over the ~1 s elapsed, plus the wrongly-counted +5.00 chakra gain), and with the gating restored it passes with `got +2.00`.)

- [ ] **Step 2: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL, `cannot open include file 'Shinobi/Abilities/GA_Melee.h'`.

- [ ] **Step 3: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Melee.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "GA_Melee.generated.h"

class UAnimMontage;
class UAbilityTask_WaitDelay;

/** Three-hit taijutsu combo. Pressing again inside the combo window continues; each landed hit grants chakra. */
UCLASS()
class SHINOBICOOP_API UGA_Melee : public UShinobiGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Melee();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<TSoftObjectPtr<UAnimMontage>> ComboMontages;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	TArray<float> ComboDamage;

	/** Seconds from swing start to the damage check. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float HitDelay = 0.25f;

	/** Seconds after a hit during which pressing again continues the combo. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float ComboWindow = 0.5f;

	/** Extra seconds added to ComboWindow for the server's copy of a remote player's ability: that
	 * copy's window-close timer is only a safety net (the owning client's own window-close normally
	 * ends the ability first, via ServerEndAbility), so give it slack for a combo press in flight. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float RemoteWindowGrace = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float Range = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float HalfAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float ChakraPerHit = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float FinisherKnockback = 600.f;

private:
	void StartSwing();

	UFUNCTION()
	void OnHitFrame();

	UFUNCTION()
	void OnComboInput(float TimeWaited);

	UFUNCTION()
	void OnComboWindowClosed();

	int32 ComboIndex = 0;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> WindowTask;
};
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/Abilities/GA_Melee.cpp`**

```cpp
#include "Shinobi/Abilities/GA_Melee.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Animation/AnimMontage.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"

UGA_Melee::UGA_Melee()
{
	InputID = EShinobiAbilityInputID::Melee;
	SetAssetTags(FGameplayTagContainer(ShinobiTags::Ability_Melee));
	ActivationBlockedTags.AddTag(ShinobiTags::State_Dodging);
	ActivationBlockedTags.AddTag(ShinobiTags::State_ChakraCharging);
	CancelAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_ChakraCharge);
	BlockAbilitiesWithTag.AddTag(ShinobiTags::Ability_Sprint);

	ComboDamage = { 20.f, 25.f, 40.f };
	ComboMontages = {
		TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_1.AM_Melee_1"))),
		TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_2.AM_Melee_2"))),
		TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Melee_3.AM_Melee_3"))),
	};
}

void UGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Players attack where the camera looks. AI turns itself.
	if (const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo()); Pawn && Pawn->IsPlayerControlled())
	{
		GetAvatarActorFromActorInfo()->SetActorRotation(FRotator(0.f, Pawn->GetControlRotation().Yaw, 0.f));
	}

	ComboIndex = 0;
	StartSwing();
}

void UGA_Melee::StartSwing()
{
	if (ComboMontages.IsValidIndex(ComboIndex))
	{
		if (UAnimMontage* Anim = ComboMontages[ComboIndex].LoadSynchronous())
		{
			UAbilityTask_PlayMontageAndWait* Play = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Anim);
			Play->ReadyForActivation();
		}
	}
	UAbilityTask_WaitDelay* HitFrame = UAbilityTask_WaitDelay::WaitDelay(this, HitDelay);
	HitFrame->OnFinish.AddDynamic(this, &UGA_Melee::OnHitFrame);
	HitFrame->ReadyForActivation();
}

void UGA_Melee::OnHitFrame()
{
	AShinobiCharacterBase* Self = GetShinobiCharacter();
	if (!Self)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	if (!ComboDamage.IsValidIndex(ComboIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const bool bFinisher = ComboIndex >= ComboDamage.Num() - 1;

	if (HasAuthority(&CurrentActivationInfo))
	{
		const TArray<AShinobiCharacterBase*> Targets = FindHostilesInCone(Range, HalfAngle);
		const FVector Origin = Self->GetActorLocation();
		int32 LandedHits = 0;
		for (AShinobiCharacterBase* Target : Targets)
		{
			if (!ApplyDamage(Target, ComboDamage[ComboIndex]))
			{
				// e.g. the target is invulnerable: no knockback, no chakra for a hit that didn't land.
				continue;
			}
			++LandedHits;
			if (bFinisher)
			{
				const FVector Push = (Target->GetActorLocation() - Origin).GetSafeNormal2D() * FinisherKnockback + FVector(0.f, 0.f, 200.f);
				Target->LaunchCharacter(Push, true, true);
			}
		}
		if (LandedHits > 0)
		{
			FGameplayEffectSpecHandle Gain = MakeOutgoingGameplayEffectSpec(UGE_ChakraGain::StaticClass(), GetAbilityLevel());
			Gain.Data->SetSetByCallerMagnitude(ShinobiTags::Data_ChakraGain, ChakraPerHit * LandedHits);
			ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Gain);
		}
	}

	// This ability is InstancedPerActor, so on a listen server a remote player's copy here is the
	// server's own separate instance (HasAuthority() true, IsLocallyControlled() false), distinct from
	// the owning client's predicted instance. See RemoteWindowGrace's doc comment for why that copy's
	// timer gets extra time.
	const bool bServerCopyOfRemote = HasAuthority(&CurrentActivationInfo) && !IsLocallyControlled();
	const float Window = bServerCopyOfRemote ? ComboWindow + RemoteWindowGrace : ComboWindow;

	// WindowTask must exist before WaitInputPress is activated below: if the server already received the
	// client's press, WaitInputPress::Activate() fires OnPress synchronously (CallReplicatedEventDelegateIfSet),
	// which calls OnComboInput() and ends WindowTask -- that has to be a real task, not a stale/null one.
	WindowTask = UAbilityTask_WaitDelay::WaitDelay(this, Window);
	WindowTask->OnFinish.AddDynamic(this, &UGA_Melee::OnComboWindowClosed);
	WindowTask->ReadyForActivation();

	if (!bFinisher)
	{
		UAbilityTask_WaitInputPress* Next = UAbilityTask_WaitInputPress::WaitInputPress(this);
		Next->OnPress.AddDynamic(this, &UGA_Melee::OnComboInput);
		Next->ReadyForActivation();
	}
}

void UGA_Melee::OnComboInput(float TimeWaited)
{
	if (WindowTask)
	{
		WindowTask->EndTask();
		WindowTask = nullptr;
	}
	++ComboIndex;
	StartSwing();
}

void UGA_Melee::OnComboWindowClosed()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```

- [ ] **Step 5: Grant the ability** — in `ShinobiPlayerCharacter.cpp` add `#include "Shinobi/Abilities/GA_Melee.h"` and set:

```cpp
	StartupAbilities = { UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass() };
```

- [ ] **Step 6: Build and run tests**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 15 TESTS PASSED`.

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop
git commit -m "feat: three-hit melee combo with chakra gain

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Player character camera + runtime input

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.h`, `ShinobiInputConfig.cpp`
- Replace: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`, `ShinobiPlayerCharacter.cpp`
- Create: `Source/ShinobiCoop/Tests/ShinobiInputTests.cpp`

Manually verifying feel (mouse/stick look, dead zones, correct WASD directions in-game) needs a local player, so that part is covered by the manual check in Task 14. But the mapping *data* itself (axis math, key counts, dead-zone presence) needs no world or local player, so it gets one automation test below. The existing tests guard against regressions elsewhere.

- [ ] **Step 1: Write `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "ShinobiInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/** Priority IMC_Shinobi is added at. An M2+ jutsu-modifier context must be added above this. */
constexpr int32 ShinobiInputPriority = 1;

/** All input actions and the mapping context, constructed in code so no input assets are needed. */
UCLASS()
class SHINOBICOOP_API UShinobiInputConfig : public UObject
{
	GENERATED_BODY()

public:
	/** Builds a fresh config; each call creates its own new UInputAction/UInputMappingContext objects. */
	static UShinobiInputConfig* Build(UObject* Outer);

	/** Process-wide singleton, rooted so it behaves like a static input asset. Re-adding the same
	 * UInputMappingContext object to a controller's subsystem replaces its existing entry instead of
	 * layering a duplicate, so every (re)possession must go through the same instance -- a fresh
	 * Build() per possession would let a stale, unbound context win priority ties after a re-possess. */
	static const UShinobiInputConfig& Get();

	UPROPERTY()
	TObjectPtr<UInputMappingContext> Context;

	UPROPERTY()
	TObjectPtr<UInputAction> Move;

	UPROPERTY()
	TObjectPtr<UInputAction> Look;

	UPROPERTY()
	TObjectPtr<UInputAction> Jump;

	UPROPERTY()
	TMap<EShinobiAbilityInputID, TObjectPtr<UInputAction>> AbilityActions;
};
```

- [ ] **Step 2: Write `Source/ShinobiCoop/Shinobi/ShinobiInputConfig.cpp`**

```cpp
#include "Shinobi/ShinobiInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, FName Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	template <class TModifier>
	TModifier* AddModifier(FEnhancedActionKeyMapping& Mapping, UObject* Outer)
	{
		TModifier* Modifier = NewObject<TModifier>(Outer);
		Mapping.Modifiers.Add(Modifier);
		return Modifier;
	}
}

UShinobiInputConfig* UShinobiInputConfig::Build(UObject* Outer)
{
	UShinobiInputConfig* Config = NewObject<UShinobiInputConfig>(Outer);
	UInputMappingContext* Ctx = NewObject<UInputMappingContext>(Config, TEXT("IMC_Shinobi"));
	Config->Context = Ctx;

	// Move: WASD + left stick. X = right, Y = forward.
	Config->Move = MakeAction(Config, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	AddModifier<UInputModifierSwizzleAxis>(Ctx->MapKey(Config->Move, EKeys::W), Ctx);
	{
		FEnhancedActionKeyMapping& Back = Ctx->MapKey(Config->Move, EKeys::S);
		AddModifier<UInputModifierSwizzleAxis>(Back, Ctx);
		AddModifier<UInputModifierNegate>(Back, Ctx);
	}
	AddModifier<UInputModifierNegate>(Ctx->MapKey(Config->Move, EKeys::A), Ctx);
	Ctx->MapKey(Config->Move, EKeys::D);
	AddModifier<UInputModifierDeadZone>(Ctx->MapKey(Config->Move, EKeys::Gamepad_Left2D), Ctx);

	// Look: mouse + right stick.
	// Y must be negated on both devices: this project has bEnableLegacyInputScales=True
	// (DefaultInput.ini), so AddControllerPitchInput/AddControllerYawInput apply the engine's
	// default APlayerController::InputPitchScale=-2.5 / InputYawScale=2.5 (BaseGame.ini) on top of
	// whatever we send. Mouse2D also picks up an automatic Scalar(0.07) sensitivity modifier from
	// BaseInput.ini's AxisConfig, since EnhancedInput only auto-applies AxisConfig to mouse keys, not
	// gamepad axes (see EnhancedInputSubsystemInterface::ApplyAxisPropertyModifiers). Flipping
	// bEnableLegacyInputScales to False would change the pitch sign and both look speeds, so the
	// negation here is load-bearing, not a preference; only the stick's Scalar magnitude below is a
	// calibration knob.
	Config->Look = MakeAction(Config, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	{
		UInputModifierNegate* InvertY = AddModifier<UInputModifierNegate>(Ctx->MapKey(Config->Look, EKeys::Mouse2D), Ctx);
		InvertY->bX = false;
		InvertY->bZ = false;
	}
	{
		FEnhancedActionKeyMapping& Stick = Ctx->MapKey(Config->Look, EKeys::Gamepad_Right2D);
		// Dead zone must run first, before the stick is negated/scaled, so it sees the raw axis value.
		AddModifier<UInputModifierDeadZone>(Stick, Ctx);
		UInputModifierNegate* InvertY = AddModifier<UInputModifierNegate>(Stick, Ctx);
		InvertY->bX = false;
		InvertY->bZ = false;
		AddModifier<UInputModifierScalar>(Stick, Ctx)->Scalar = FVector(60.0, 40.0, 1.0);
		AddModifier<UInputModifierScaleByDeltaTime>(Stick, Ctx);
	}

	Config->Jump = MakeAction(Config, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->Jump, EKeys::SpaceBar);
	Ctx->MapKey(Config->Jump, EKeys::Gamepad_FaceButton_Bottom);

	const auto AddAbility = [Config, Ctx](EShinobiAbilityInputID ID, FName Name, const FKey& Key, const FKey& PadKey)
	{
		UInputAction* Action = MakeAction(Config, Name, EInputActionValueType::Boolean);
		Ctx->MapKey(Action, Key);
		Ctx->MapKey(Action, PadKey);
		Config->AbilityActions.Add(ID, Action);
	};
	AddAbility(EShinobiAbilityInputID::Melee, TEXT("IA_Melee"), EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Left);
	AddAbility(EShinobiAbilityInputID::Dodge, TEXT("IA_Dodge"), EKeys::LeftControl, EKeys::Gamepad_FaceButton_Right);
	AddAbility(EShinobiAbilityInputID::Sprint, TEXT("IA_Sprint"), EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick);
	AddAbility(EShinobiAbilityInputID::ChakraCharge, TEXT("IA_ChakraCharge"), EKeys::C, EKeys::Gamepad_LeftShoulder);

	return Config;
}

// Cached for the process: after Live Coding changes to Build() or to this class's layout, restart the editor.
const UShinobiInputConfig& UShinobiInputConfig::Get()
{
	static UShinobiInputConfig* Instance = []
	{
		UShinobiInputConfig* Config = Build(GetTransientPackage());
		Config->AddToRoot(); // lives for the process, like an input asset
		return Config;
	}();
	return *Instance;
}
```

(Review fix: `SetupPlayerInputComponent` originally called `UShinobiInputConfig::Build(this)` per-pawn, storing the result in a per-pawn `UPROPERTY`. That rebuilds a brand-new `UInputMappingContext` object on every (re)possession — e.g. `GameModeBase::RestartPlayer` on the same pawn unpossesses then re-possesses it — and `AddMappingContext` was never paired with a `RemoveMappingContext` for the old one. `IEnhancedInputSubsystemInterface::AddMappingContext` (`EnhancedInputSubsystemInterface.cpp` ~260-271) keys `AppliedInputContextData` by the *IMC object pointer* (`TMap::Add` on an existing key just overwrites its priority), so re-adding the *same* object is idempotent, but two *different* IMC objects both being registered means the older one's now-orphaned key mappings can still resolve and race the newer one on priority ties, and it keeps the old pawn's input actions rooted through level travel. Fix: `UShinobiInputConfig::Get()` builds the config exactly once per process and roots it, exactly like a static input data asset; `SetupPlayerInputComponent` and the per-pawn `InputConfig` `UPROPERTY` were removed in favor of always fetching the same instance.)

- [ ] **Step 3: Replace `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "ShinobiPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
struct FInputActionValue;

/** Player-controlled shinobi: third-person camera, input bound to abilities. */
UCLASS()
class SHINOBICOOP_API AShinobiPlayerCharacter : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	/** Ignores the jump input while State.ChakraCharging is present; movement is locked while charging. */
	void JumpPressed();
	void AbilityInputPressed(EShinobiAbilityInputID InputID);
	void AbilityInputReleased(EShinobiAbilityInputID InputID);

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};
```

- [ ] **Step 4: Replace `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp`**

```cpp
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Shinobi/ShinobiInputConfig.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_Sprint.h"

AShinobiPlayerCharacter::AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	StartupAbilities = { UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass() };

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	JumpMaxCount = 2; // double jump

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
	Movement->JumpZVelocity = 600.f;
	Movement->AirControl = 0.35f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->BrakingDecelerationFalling = 1500.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.f;
	CameraBoom->SocketOffset = FVector(0.f, 60.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	UseMannequinMesh();
}

void AShinobiPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	const UShinobiInputConfig& InputConfig = UShinobiInputConfig::Get();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputConfig.Context, ShinobiInputPriority);
		}
	}

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(InputConfig.Move, ETriggerEvent::Triggered, this, &AShinobiPlayerCharacter::Move);
	Input->BindAction(InputConfig.Look, ETriggerEvent::Triggered, this, &AShinobiPlayerCharacter::Look);
	Input->BindAction(InputConfig.Jump, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::JumpPressed);
	Input->BindAction(InputConfig.Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	for (const TPair<EShinobiAbilityInputID, TObjectPtr<UInputAction>>& Pair : InputConfig.AbilityActions)
	{
		Input->BindAction(Pair.Value, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::AbilityInputPressed, Pair.Key);
		Input->BindAction(Pair.Value, ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::AbilityInputReleased, Pair.Key);
	}
}

void AShinobiPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller || AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_ChakraCharging))
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void AShinobiPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AShinobiPlayerCharacter::JumpPressed()
{
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_ChakraCharging))
	{
		return;
	}
	Jump();
}

void AShinobiPlayerCharacter::AbilityInputPressed(EShinobiAbilityInputID InputID)
{
	AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(InputID));
}

void AShinobiPlayerCharacter::AbilityInputReleased(EShinobiAbilityInputID InputID)
{
	AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(InputID));
}
```

- [ ] **Step 5: Write `Source/ShinobiCoop/Tests/ShinobiInputTests.cpp`**

No `FShinobiTestWorld`/local player needed: `UShinobiInputConfig::Build` returns plain data, so the test builds its own throwaway config and walks each mapping's `Modifiers` array directly.

```cpp
#include "Tests/ShinobiTestWorld.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Shinobi/ShinobiInputConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FEnhancedActionKeyMapping* FindMapping(const TArray<FEnhancedActionKeyMapping>& Mappings, const UInputAction* Action, FKey Key)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Mappings)
		{
			if (Mapping.Action == Action && Mapping.Key == Key)
			{
				return &Mapping;
			}
		}
		return nullptr;
	}

	/** Runs a mapping's modifiers in order, starting from an Axis2D value of (1, 0). */
	FVector2D RunModifiers(const FEnhancedActionKeyMapping& Mapping)
	{
		FInputActionValue Value(EInputActionValueType::Axis2D, FVector(1.f, 0.f, 0.f));
		for (const TObjectPtr<UInputModifier>& Modifier : Mapping.Modifiers)
		{
			Value = Modifier->ModifyRaw(nullptr, Value, 0.f);
		}
		return Value.Get<FVector2D>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiInputMappingsTest, "ShinobiCoop.Input.MappingsAreCorrect", ShinobiTestFlags)
bool FShinobiInputMappingsTest::RunTest(const FString& Parameters)
{
	UShinobiInputConfig* Config = UShinobiInputConfig::Build(GetTransientPackage());
	const TArray<FEnhancedActionKeyMapping>& Mappings = Config->Context->GetMappings();

	const FEnhancedActionKeyMapping* WMapping = FindMapping(Mappings, Config->Move, EKeys::W);
	const FEnhancedActionKeyMapping* SMapping = FindMapping(Mappings, Config->Move, EKeys::S);
	const FEnhancedActionKeyMapping* AMapping = FindMapping(Mappings, Config->Move, EKeys::A);
	const FEnhancedActionKeyMapping* DMapping = FindMapping(Mappings, Config->Move, EKeys::D);
	if (TestNotNull(TEXT("W mapping exists"), WMapping))
	{
		TestEqual(TEXT("W drives forward"), RunModifiers(*WMapping), FVector2D(0.f, 1.f));
	}
	if (TestNotNull(TEXT("S mapping exists"), SMapping))
	{
		TestEqual(TEXT("S drives backward"), RunModifiers(*SMapping), FVector2D(0.f, -1.f));
	}
	if (TestNotNull(TEXT("A mapping exists"), AMapping))
	{
		TestEqual(TEXT("A drives left"), RunModifiers(*AMapping), FVector2D(-1.f, 0.f));
	}
	if (TestNotNull(TEXT("D mapping exists"), DMapping))
	{
		TestEqual(TEXT("D drives right"), RunModifiers(*DMapping), FVector2D(1.f, 0.f));
	}

	for (const TPair<EShinobiAbilityInputID, TObjectPtr<UInputAction>>& Pair : Config->AbilityActions)
	{
		int32 KeyCount = 0;
		for (const FEnhancedActionKeyMapping& Mapping : Mappings)
		{
			if (Mapping.Action == Pair.Value)
			{
				++KeyCount;
			}
		}
		TestEqual(*FString::Printf(TEXT("Ability input %d is bound to exactly two keys"), static_cast<int32>(Pair.Key)), KeyCount, 2);
	}

	const FEnhancedActionKeyMapping* LeftStick = FindMapping(Mappings, Config->Move, EKeys::Gamepad_Left2D);
	if (TestNotNull(TEXT("Left stick mapping exists"), LeftStick))
	{
		TestTrue(TEXT("Left stick's first modifier is a dead zone"),
			LeftStick->Modifiers.Num() > 0 && LeftStick->Modifiers[0]->IsA<UInputModifierDeadZone>());
	}

	const FEnhancedActionKeyMapping* RightStick = FindMapping(Mappings, Config->Look, EKeys::Gamepad_Right2D);
	if (TestNotNull(TEXT("Right stick mapping exists"), RightStick))
	{
		TestTrue(TEXT("Right stick's first modifier is a dead zone"),
			RightStick->Modifiers.Num() > 0 && RightStick->Modifiers[0]->IsA<UInputModifierDeadZone>());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

(TDD note: written against a `ShinobiInputConfig.cpp` that mapped `Gamepad_Left2D`/`Gamepad_Right2D` with no `UInputModifierDeadZone`, this test's Move-mapping and ability-key-count assertions passed immediately, but both "first modifier is a dead zone" assertions failed — confirming Enhanced Input only auto-applies `BaseInput.ini`'s `AxisConfig` dead zones to mouse keys (`EnhancedInputSubsystemInterface::ApplyAxisPropertyModifiers` returns early for non-mouse keys unless `input.GlobalAxisConfigMode` is set to `All`), so raw stick drift was otherwise free to trigger Move/Look. Adding the two `AddModifier<UInputModifierDeadZone>` calls in Step 2 above — the right stick's dead zone ordered before its Negate/Scalar/ScaleByDeltaTime chain — turned the test green.)

- [ ] **Step 6: Build and run tests**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 16 TESTS PASSED`.

- [ ] **Step 7: Commit**

```powershell
git add Source/ShinobiCoop
git commit -m "feat: player camera and runtime-built enhanced input

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Training dummy, player controller, game mode, HUD

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.h`, `.cpp`
- Create: `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h`, `.cpp`
- Create: `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h`, `.cpp`
- Create: `Source/ShinobiCoop/Shinobi/ShinobiGameMode.h`, `.cpp`
- Modify: `Config/DefaultEngine.ini`
- Modify: `Source/ShinobiCoop/Tests/ShinobiAttributeTests.cpp` (dummy reset test)

- [ ] **Step 1: Append failing dummy test to `ShinobiAttributeTests.cpp`**

Add `#include "Shinobi/ShinobiTrainingDummy.h"` after the first include (inside the `#if`), then append:
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDummyResetTest, "ShinobiCoop.Core.TrainingDummyResetsAfterDefeat", ShinobiTestFlags)
bool FShinobiDummyResetTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiTrainingDummy* Dummy = W.Spawn<AShinobiTrainingDummy>();
	ShinobiTest::Damage(Dummy, 1000.f);
	TestEqual(TEXT("Dummy defeated"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 0.f);
	W.Tick(1.f);
	TestEqual(TEXT("Still defeated before the reset delay"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 0.f);
	W.Tick(1.f);
	TestEqual(TEXT("Dummy reset to full"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 500.f);

	// The reset timer must re-arm: a second defeat should reset again, not just fire once ever.
	ShinobiTest::Damage(Dummy, 1000.f);
	TestEqual(TEXT("Dummy defeated again"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 0.f);
	W.Tick(2.f);
	TestEqual(TEXT("Dummy reset to full again"), ShinobiTest::Get(Dummy, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}
```

- [ ] **Step 2: Build to see it fail**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\build.ps1`
Expected: FAIL, `cannot open include file 'Shinobi/ShinobiTrainingDummy.h'`.

- [ ] **Step 3: Write `Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "ShinobiTrainingDummy.generated.h"

class UTextRenderComponent;

/** Enemy-team punching bag. Shows its health above its head and resets 1.5 s after being defeated. */
UCLASS()
class SHINOBICOOP_API AShinobiTrainingDummy : public AShinobiCharacterBase
{
	GENERATED_BODY()

public:
	AShinobiTrainingDummy(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void HandleOutOfHealth() override;

private:
	void ResetHealth();

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UTextRenderComponent> HealthText;

	FTimerHandle ResetTimer;

	/** Last values written to HealthText. Seeded from Attributes in the constructor, so Tick only calls
	 * SetText (which unconditionally marks render state dirty) when the rounded HP or MaxHP actually
	 * changes rather than on every frame. */
	int32 DisplayedHealth = -1;
	int32 DisplayedMaxHealth = -1;
};
```

- [ ] **Step 4: Write `Source/ShinobiCoop/Shinobi/ShinobiTrainingDummy.cpp`**

```cpp
#include "Shinobi/ShinobiTrainingDummy.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiAttributeSet.h"

AShinobiTrainingDummy::AShinobiTrainingDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Team = EShinobiTeam::Enemies;
	AutoPossessAI = EAutoPossessAI::Disabled;

	HealthText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthText"));
	HealthText->SetupAttachment(RootComponent);
	HealthText->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HealthText->SetHorizontalAlignment(EHTA_Center);
	HealthText->SetWorldSize(28.f);
	HealthText->SetTextRenderColor(FColor::Red);

	UseMannequinMesh();

	// Attributes is constructed by AShinobiCharacterBase's constructor, which has already run at this
	// point, so its defaults are readable here. Avoids showing the widget's default "Text" for a frame.
	DisplayedHealth = FMath::RoundToInt(Attributes->GetHealth());
	DisplayedMaxHealth = FMath::RoundToInt(Attributes->GetMaxHealth());
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d"), DisplayedHealth, DisplayedMaxHealth)));
}

void AShinobiTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ponytail: per-frame poll with change detection; switch to an attribute-change delegate if dummies multiply
	const int32 CurrentHealth = FMath::RoundToInt(Attributes->GetHealth());
	const int32 CurrentMaxHealth = FMath::RoundToInt(Attributes->GetMaxHealth());
	if (CurrentHealth != DisplayedHealth || CurrentMaxHealth != DisplayedMaxHealth)
	{
		DisplayedHealth = CurrentHealth;
		DisplayedMaxHealth = CurrentMaxHealth;
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d"), DisplayedHealth, DisplayedMaxHealth)));
	}

	// TextRender geometry lies flat facing its local +X axis, so it's mirrored/culled when viewed from
	// behind. Face the local camera (yaw only) on machines that have one. GetFirstPlayerController()
	// would return any PC in the world -- on a listen server that includes remote clients' PCs, which
	// all have a camera manager but aren't local -- so use the game instance's local-player lookup
	// instead, which is null on a dedicated server and in the test world.
	const UGameInstance* GI = GetGameInstance();
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	const APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager : nullptr;
	if (CameraManager)
	{
		const FVector CameraLocation = CameraManager->GetCameraLocation();
		const FVector TextLocation = HealthText->GetComponentLocation();
		HealthText->SetWorldRotation(FRotator(0.f, (CameraLocation - TextLocation).Rotation().Yaw, 0.f));
	}
}

void AShinobiTrainingDummy::HandleOutOfHealth()
{
	Super::HandleOutOfHealth();
	GetWorldTimerManager().SetTimer(ResetTimer, this, &AShinobiTrainingDummy::ResetHealth, 1.5f, false);
}

void AShinobiTrainingDummy::ResetHealth()
{
	Attributes->SetHealth(Attributes->GetMaxHealth());
}
```

- [ ] **Step 5: Write `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiHUDWidget.generated.h"

class UProgressBar;
class UVerticalBox;

/** Health / chakra / stamina bars. Widget tree is built in code; values are polled each tick. */
UCLASS()
class SHINOBICOOP_API UShinobiHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UProgressBar* AddBar(UVerticalBox* Box, FName Name, const FLinearColor& Color, float Height);

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> ChakraBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> StaminaBar;
};
```

- [ ] **Step 6: Write `Source/ShinobiCoop/Shinobi/UI/ShinobiHUDWidget.cpp`**

```cpp
#include "Shinobi/UI/ShinobiHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"

namespace
{
	const FLinearColor HealthColor(0.2f, 0.85f, 0.3f);
	const FLinearColor LowHealthColor(0.9f, 0.15f, 0.1f);
	const FLinearColor ChakraColor(0.2f, 0.5f, 1.f);
	const FLinearColor StaminaColor(1.f, 0.8f, 0.2f);
}

TSharedRef<SWidget> UShinobiHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		// Bars are read-only; never eat cursor/click input meant for the game or other widgets.
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);

		UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Bars"));
		UCanvasPanelSlot* BarsSlot = Root->AddChildToCanvas(Bars);
		BarsSlot->SetAnchors(FAnchors(0.f, 1.f));
		BarsSlot->SetAlignment(FVector2D(0.f, 1.f));
		BarsSlot->SetPosition(FVector2D(40.f, -40.f));
		BarsSlot->SetAutoSize(true);

		HealthBar = AddBar(Bars, TEXT("HealthBar"), HealthColor, 22.f);
		ChakraBar = AddBar(Bars, TEXT("ChakraBar"), ChakraColor, 16.f);
		StaminaBar = AddBar(Bars, TEXT("StaminaBar"), StaminaColor, 10.f);
	}
	return Super::RebuildWidget();
}

UProgressBar* UShinobiHUDWidget::AddBar(UVerticalBox* Box, FName Name, const FLinearColor& Color, float Height)
{
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	// Without an explicit width, SProgressBar::ComputeDesiredSize falls back to the 20x12 marquee image
	// (the runtime UMG style's Common/ProgressBar_Marquee brush, set up in UMGCoreStyle.cpp), so the
	// auto-sized canvas slot above would otherwise collapse to ~20 px wide.
	SizeBox->SetWidthOverride(380.f);
	SizeBox->SetHeightOverride(Height);
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
	Bar->SetFillColorAndOpacity(Color);
	SizeBox->AddChild(Bar);
	UVerticalBoxSlot* BarSlot = Box->AddChildToVerticalBox(SizeBox);
	BarSlot->SetPadding(FMargin(0.f, 2.f));
	return Bar;
}

void UShinobiHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const AShinobiCharacterBase* Pawn = Cast<AShinobiCharacterBase>(GetOwningPlayerPawn());
	const UShinobiAttributeSet* Stats = Pawn ? Pawn->GetAttributes() : nullptr;
	if (!Stats || !HealthBar)
	{
		return;
	}
	const float HealthRatio = Stats->GetHealth() / FMath::Max(Stats->GetMaxHealth(), 1.f);
	HealthBar->SetPercent(HealthRatio);
	HealthBar->SetFillColorAndOpacity(HealthRatio < 0.3f ? LowHealthColor : HealthColor);
	ChakraBar->SetPercent(Stats->GetChakra() / FMath::Max(Stats->GetMaxChakra(), 1.f));
	StaminaBar->SetPercent(Stats->GetStamina() / FMath::Max(Stats->GetMaxStamina(), 1.f));
}
```

- [ ] **Step 7: Write `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShinobiPlayerController.generated.h"

class UShinobiHUDWidget;

/** Creates the HUD for local players; hosts developer console commands and the smoke-test hook. */
UCLASS()
class SHINOBICOOP_API AShinobiPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Console: ShinobiDamageSelf 50 — server applies damage to this player's pawn. */
	UFUNCTION(Exec)
	void ShinobiDamageSelf(float Amount);

	/** Console: ShinobiDumpStats — logs every character's attributes as this machine sees them. */
	UFUNCTION(Exec)
	void ShinobiDumpStats();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerDamageSelf(float Amount);

	/** With -ShinobiSmoke on a client: damage self, dump stats, quit. Used by Scripts/smoke_m1.ps1. */
	void RunSmokeTest();

	UPROPERTY()
	TObjectPtr<UShinobiHUDWidget> Hud;
};
```

- [ ] **Step 8: Write `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.cpp`**

```cpp
#include "Shinobi/ShinobiPlayerController.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/UI/ShinobiHUDWidget.h"

void AShinobiPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	if (FApp::CanEverRender())
	{
		// CreateWidget returns null if this controller has no Player attached yet.
		Hud = CreateWidget<UShinobiHUDWidget>(this, UShinobiHUDWidget::StaticClass());
		if (Hud)
		{
			Hud->AddToViewport();
		}
	}
	if (GetNetMode() == NM_Client && FParse::Param(FCommandLine::Get(), TEXT("ShinobiSmoke")))
	{
		RunSmokeTest();
	}
}

void AShinobiPlayerController::ShinobiDamageSelf(float Amount)
{
	ServerDamageSelf(Amount);
}

void AShinobiPlayerController::ServerDamageSelf_Implementation(float Amount)
{
	AShinobiCharacterBase* Shinobi = GetPawn<AShinobiCharacterBase>();
	if (!Shinobi)
	{
		return;
	}
	UAbilitySystemComponent* ASC = Shinobi->GetAbilitySystemComponent();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_Damage::StaticClass(), 1.f, ASC->MakeEffectContext());
	Spec.Data->SetSetByCallerMagnitude(ShinobiTags::Data_Damage, Amount);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void AShinobiPlayerController::ShinobiDumpStats()
{
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const UShinobiAttributeSet* Stats = It->GetAttributes();
		UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiStats %s local=%d H=%.1f C=%.1f S=%.1f"),
			*It->GetName(), It->IsLocallyControlled() ? 1 : 0, Stats->GetHealth(), Stats->GetChakra(), Stats->GetStamina());
	}
}

void AShinobiPlayerController::RunSmokeTest()
{
	// ponytail: fixed delays assume a local connection; raise them if the smoke test flakes on a slow machine
	FTimerHandle DamageTimer;
	GetWorldTimerManager().SetTimer(DamageTimer, FTimerDelegate::CreateUObject(this, &AShinobiPlayerController::ShinobiDamageSelf, 50.f), 5.f, false);
	FTimerHandle DumpTimer;
	GetWorldTimerManager().SetTimer(DumpTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ShinobiDumpStats();
		ConsoleCommand(TEXT("quit"));
	}), 9.f, false);
}
```

- [ ] **Step 9: Write `Source/ShinobiCoop/Shinobi/ShinobiGameMode.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShinobiGameMode.generated.h"

/** M1 training mode: Shinobi pawn/controller and three training dummies in front of the first PlayerStart. */
UCLASS()
class SHINOBICOOP_API AShinobiGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShinobiGameMode();

protected:
	virtual void BeginPlay() override;
};
```

- [ ] **Step 10: Write `Source/ShinobiCoop/Shinobi/ShinobiGameMode.cpp`**

```cpp
#include "Shinobi/ShinobiGameMode.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/ShinobiTrainingDummy.h"

AShinobiGameMode::AShinobiGameMode()
{
	DefaultPawnClass = AShinobiPlayerCharacter::StaticClass();
	PlayerControllerClass = AShinobiPlayerController::StaticClass();
}

void AShinobiGameMode::BeginPlay()
{
	Super::BeginPlay();

	TActorIterator<APlayerStart> Start(GetWorld());
	if (!Start)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("AShinobiGameMode::BeginPlay: no PlayerStart found, skipping training dummy spawn"));
		return;
	}
	const FVector Origin = Start->GetActorLocation();
	const FVector Forward = Start->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FRotator FacePlayer = (-Forward).Rotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	for (const float Offset : { -300.f, 0.f, 300.f })
	{
		GetWorld()->SpawnActor<AShinobiTrainingDummy>(Origin + Forward * 600.f + Right * Offset, FacePlayer, Params);
	}
}
```

- [ ] **Step 11: Point the project default GameMode at ours**

In `Config/DefaultEngine.ini`, under `[/Script/EngineSettings.GameMapsSettings]`, replace the `GlobalDefaultGameMode=` line with:
```ini
GlobalDefaultGameMode=/Script/ShinobiCoop.ShinobiGameMode
```

- [ ] **Step 12: Build and run tests**

Run:
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\build.ps1
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
Expected: `ALL 17 TESTS PASSED` (16 earlier + `TrainingDummyResetsAfterDefeat`).

- [ ] **Step 13: Commit**

```powershell
git add Source/ShinobiCoop Config/DefaultEngine.ini
git commit -m "feat: training dummies, HUD bars, game mode and dev commands

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 12: Generate montages and fix the map's GameMode override

**Files:**
- Create: `Scripts/setup_m1.py`
- Creates assets: `Content/Shinobi/Anims/AS_Dodge.uasset`, `AM_Melee_1..3.uasset`, `AM_Dodge.uasset`
- Modifies asset: `Content/ThirdPerson/Lvl_ThirdPerson.umap` (World Settings → GameMode Override)

- [ ] **Step 1: Write `Scripts/setup_m1.py`**

```python
"""M1 asset setup. Idempotent: safe to re-run.

1. Duplicates the template's dash animation with root motion disabled (see create_dash_sequence).
2. Creates melee/dodge montages from the template's Manny animations (DefaultSlot, matches ABP_Manny_Combat).
3. Tunes each montage's blend-in time and (for the dodge) playback rate (see tune_montages).
4. Points Lvl_ThirdPerson's GameMode Override (which the template sets to BP_ThirdPersonGameMode) at ShinobiGameMode.
"""
import unreal

ANIM_DIR = "/Game/Shinobi/Anims"
DASH_SOURCE = "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash"
DASH_SEQUENCE = f"{ANIM_DIR}/AS_Dodge"
MONTAGES = {
    "AM_Melee_1": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01",
    "AM_Melee_2": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02",
    "AM_Melee_3": "/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03",
    "AM_Dodge": DASH_SEQUENCE,
}
# blend_in: UAnimMontage.BlendIn is an FAlphaBlend struct (Engine/Public/AlphaBlend.h), not a plain
# float, so it has to be read, have its blend_time set, and written back -- setting a property on the
# copy get_editor_property() returns does not touch the montage until it's assigned back.
# rate_scale: UAnimSequenceBase.RateScale (Engine/Classes/Animation/AnimSequenceBase.h). MM_Dash is
# ~0.97 s and GA_Dodge plays AM_Dodge with bStopWhenAbilityEnds=false (see GA_Dodge.cpp) -- it doesn't
# stop the montage when the 0.25 s dash (GA_Dodge::Duration) ends, so without this the character keeps
# playing the dash pose for ~0.7 s after it has physically stopped moving. At 3x it finishes in ~0.32 s,
# close to when the dash itself ends.
MONTAGE_TUNING = {
    "AM_Dodge": {"blend_in": 0.05, "blend_out": 0.1, "rate_scale": 3.0},
    "AM_Melee_1": {"blend_in": 0.1},
    "AM_Melee_2": {"blend_in": 0.1},
    "AM_Melee_3": {"blend_in": 0.1},
}
MAP = "/Game/ThirdPerson/Lvl_ThirdPerson"
GAME_MODE = "/Script/ShinobiCoop.ShinobiGameMode"


def create_dash_sequence():
    # GA_Dodge drives the dash with an AbilityTask_ApplyRootMotionConstantForce. MM_Dash has its own
    # baked root motion (bEnableRootMotion=True, ~0.97 s), and ABP_Manny_Combat uses
    # RootMotionFromMontagesOnly; CharacterMovementComponent::ApplyRootMotionToVelocity treats anim
    # root motion as taking precedence over any other root motion source while it plays
    # (CharacterMovementComponent.cpp, ApplyRootMotionToVelocity / the ~2952 HasAnimRootMotion() branch:
    # "Animation root motion overrides Velocity and currently doesn't allow any other root motion
    # sources"), so playing MM_Dash directly would silently replace our 600-unit/0.25 s dash with the
    # template's own translation. Duplicate it and turn root motion off so the montage is visual only;
    # force_root_lock keeps the pelvis from sliding relative to the capsule. Melee montages keep their
    # own root motion (a forward step on the attack is fine and not fighting another root motion task).
    if unreal.EditorAssetLibrary.does_asset_exist(DASH_SEQUENCE):
        sequence = unreal.load_asset(DASH_SEQUENCE)
        unreal.log(f"[setup_m1] exists: {DASH_SEQUENCE}")
    else:
        sequence = unreal.EditorAssetLibrary.duplicate_asset(DASH_SOURCE, DASH_SEQUENCE)
        if sequence is None:
            raise RuntimeError(f"failed to duplicate {DASH_SOURCE} -> {DASH_SEQUENCE}")
        unreal.log(f"[setup_m1] created: {DASH_SEQUENCE}")

    # Re-applied on every run, not just at creation: this is the property that makes the whole dodge
    # work (see above), so if it's ever reset -- a reimport, a manual edit, someone re-duplicating the
    # source by hand -- re-running this script must be enough to put it right again rather than silently
    # leaving the dodge broken until someone remembers to check two flags on an animation asset.
    sequence.set_editor_property("enable_root_motion", False)
    sequence.set_editor_property("force_root_lock", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sequence):
        raise RuntimeError(f"failed to save {DASH_SEQUENCE}")


def create_montages():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name, source_path in MONTAGES.items():
        target = f"{ANIM_DIR}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(target):
            unreal.log(f"[setup_m1] exists: {target}")
            continue
        source = unreal.load_asset(source_path)
        if source is None:
            raise RuntimeError(f"missing source animation {source_path}")
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", source)
        montage = tools.create_asset(name, ANIM_DIR, unreal.AnimMontage, factory)
        if montage is None:
            raise RuntimeError(f"failed to create {target}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
            raise RuntimeError(f"failed to save {target}")
        unreal.log(f"[setup_m1] created: {target}")


def tune_montages():
    # Applied on every run, not just at creation, for the same reason as create_dash_sequence's
    # root-motion flags: re-running this script should be enough to put the tuning back if it's ever
    # reset by hand or by re-creating a montage from its source animation.
    for name, settings in MONTAGE_TUNING.items():
        target = f"{ANIM_DIR}/{name}"
        montage = unreal.load_asset(target)
        if montage is None:
            raise RuntimeError(f"missing montage to tune: {target}")
        if "blend_in" in settings:
            blend_in = montage.get_editor_property("blend_in")
            blend_in.set_editor_property("blend_time", settings["blend_in"])
            montage.set_editor_property("blend_in", blend_in)
        if "blend_out" in settings:
            blend_out = montage.get_editor_property("blend_out")
            blend_out.set_editor_property("blend_time", settings["blend_out"])
            montage.set_editor_property("blend_out", blend_out)
        if "rate_scale" in settings:
            montage.set_editor_property("rate_scale", settings["rate_scale"])
        if not unreal.EditorAssetLibrary.save_loaded_asset(montage):
            raise RuntimeError(f"failed to save {target}")
        unreal.log(f"[setup_m1] tuned: {target}")


def set_game_mode():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    if world is None:
        raise RuntimeError(f"failed to load map {MAP}")
    game_mode = unreal.load_class(None, GAME_MODE)
    if game_mode is None:
        raise RuntimeError(f"class not found: {GAME_MODE} (build the C++ first)")
    world_settings = world.get_world_settings()
    # Unreal's Python bindings intern one wrapper object per underlying UObject, so this equality
    # check is reliable (verified empirically: two independently-obtained wrappers for the same class
    # are `is`-identical). Skipping the save when it's already correct avoids resaving the map -- and
    # so a new LFS blob -- on every single run, which SaveMap did unconditionally before.
    if world_settings.get_editor_property("default_game_mode") == game_mode:
        unreal.log(f"[setup_m1] GameMode Override already {GAME_MODE}")
        return
    world_settings.set_editor_property("default_game_mode", game_mode)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError(f"failed to save {MAP}")
    unreal.log(f"[setup_m1] {MAP} GameMode Override -> {GAME_MODE}")


create_dash_sequence()
create_montages()
tune_montages()
set_game_mode()
```

- [ ] **Step 2: Run it**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\run_python.ps1 -Script Scripts\setup_m1.py`
Expected output contains 5 lines `[setup_m1] created: /Game/Shinobi/Anims/...` (`AS_Dodge`, `AM_Melee_1`, `AM_Melee_2`, `AM_Melee_3`, `AM_Dodge`), 4 lines `[setup_m1] tuned: /Game/Shinobi/Anims/...` (one per `MONTAGE_TUNING` entry), and `[setup_m1] /Game/ThirdPerson/Lvl_ThirdPerson GameMode Override -> /Script/ShinobiCoop.ShinobiGameMode`.

On a re-run, the create-step lines become `[setup_m1] exists: ...` and the GameMode line becomes `[setup_m1] GameMode Override already /Script/ShinobiCoop.ShinobiGameMode` (the `tuned:` lines still print every run — tuning is re-applied unconditionally).

Fallback if `set_game_mode` fails in the commandlet: the user opens `Lvl_ThirdPerson` in the editor → Window → World Settings → GameMode Override → `ShinobiGameMode` → Save.

- [ ] **Step 3: Verify assets exist**

Run: `Get-ChildItem Content\Shinobi\Anims -Name`
Expected: `AM_Dodge.uasset`, `AM_Melee_1.uasset`, `AM_Melee_2.uasset`, `AM_Melee_3.uasset`, `AS_Dodge.uasset`.

- [ ] **Step 4: Re-run tests (montages now load inside abilities)**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1`
Expected: `ALL 17 TESTS PASSED` (no new test; montages now load inside abilities that already existed).

- [ ] **Step 5: Commit**

```powershell
git add Scripts/setup_m1.py Content/Shinobi Content/ThirdPerson/Lvl_ThirdPerson.umap
git commit -m "feat: melee/dodge montages and ShinobiGameMode on startup map

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 13: Two-process replication smoke test

**Files:**
- Create: `Scripts/smoke_m1.ps1`

- [ ] **Step 1: Write `Scripts/smoke_m1.ps1`**

```powershell
# Starts a listen server and a client as separate -game processes (no rendering), has the client
# damage itself through a server RPC, and asserts the client sees the replicated Health.
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Editor = "$UE\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$ServerLog = Join-Path $LogDir "SmokeServer.log"
$ClientLog = Join-Path $LogDir "SmokeClient.log"

# A previous run that was killed out from under this script (Ctrl+C, closed shell) can leave an
# UnrealEditor.exe still listening on port 7777; a fresh server would then be unreachable while the
# client silently joins the stale one instead. Clear those out before touching the logs.
Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" | Where-Object { $_.CommandLine -match '-ShinobiSmoke' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }

Remove-Item -LiteralPath $ServerLog, $ClientLog -ErrorAction SilentlyContinue
if ((Test-Path -LiteralPath $ServerLog) -or (Test-Path -LiteralPath $ClientLog)) {
    throw "Smoke log still locked by another process; close leftover UnrealEditor instances"
}
$Common = "-game -nullrhi -nosound -unattended -nosplash -ShinobiSmoke"

$server = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" /Game/ThirdPerson/Lvl_ThirdPerson?listen $Common -abslog=`"$ServerLog`""
$client = $null
try {
    $deadline = (Get-Date).AddSeconds(240)
    while (-not ((Test-Path -LiteralPath $ServerLog) -and (Select-String -LiteralPath $ServerLog -Pattern "Bringing World .* up for play" -Quiet))) {
        if ($server.HasExited) { throw "Server exited before loading the map (exit $($server.ExitCode)); see $ServerLog" }
        if ((Get-Date) -gt $deadline) { throw "Server did not start within 240s" }
        Start-Sleep -Seconds 2
    }

    $client = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" 127.0.0.1 $Common -abslog=`"$ClientLog`""
    if (-not $client.WaitForExit(240000)) { Stop-Process $client -Force; throw "Client did not finish within 240s" }
}
finally {
    Stop-Process $server -Force -ErrorAction SilentlyContinue
    if ($client -and -not $client.HasExited) { Stop-Process -Id $client.Id -Force -ErrorAction SilentlyContinue }
}

$hit = Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats .* local=1 H=450\.0"
if ($hit) {
    Write-Host "SMOKE PASS: $($hit.Line)"
} else {
    Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats|Error" | Select-Object -Last 20 | ForEach-Object { Write-Host $_.Line }
    throw "SMOKE FAIL: client never saw its replicated Health drop to 450"
}
```

- [ ] **Step 2: Run it**

Run: `powershell -ExecutionPolicy Bypass -File Scripts\smoke_m1.ps1`
Expected: `SMOKE PASS: ... ShinobiStats ShinobiPlayerCharacter_... local=1 H=450.0 C=100.0 S=100.0`

If it fails with no `ShinobiStats` lines at all, check `Saved\Logs\SmokeClient.log` for `Connection` / `Travel` errors (firewall prompt on first run: allow UnrealEditor.exe on private networks, then re-run).

- [ ] **Step 3: Commit**

```powershell
git add Scripts/smoke_m1.ps1
git commit -m "test: two-process listen-server replication smoke test

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 14: Manual 2-player play check (user + agent)

**Files:** none (fixes go back into the relevant task's files)

- [ ] **Step 1: Open the editor and set up 2-player PIE**

User opens `ShinobiCoop.uproject` → Play dropdown (⋮ next to Play) → Number of Players `2`, Net Mode `Play As Listen Server` → Play.

- [ ] **Step 2: Walk the checklist in both windows**

| # | Check | Expected |
|---|---|---|
| 1 | WASD / left stick | Moves relative to camera |
| 2 | Mouse / right stick | Camera orbits; moving mouse up looks up (if inverted, remove the `UInputModifierNegate` on `Mouse2D` in `ShinobiInputConfig.cpp`) |
| 3 | Space twice | Double jump |
| 4 | Hold Shift | Faster run, stamina bar drains, refills ~1 s after release |
| 5 | Ctrl | Quick dash with dash animation, stamina -25, blocked when stamina < 25 |
| 6 | Hold C | Character stops, chakra bar fills fast |
| 7 | LMB ×3 near a dummy | Three punch animations; dummy HP text drops 20 → 25 → 40; third hit knocks it back; chakra bar rises |
| 8 | Beat a dummy to 0 | Resets to 500 after ~1.5 s |
| 9 | Second window | Sees the first player's animations and the same dummy HP; players can't hit each other |
| 10 | Console (`~`) `ShinobiDamageSelf 100` in the client window | Client HUD health bar drops by 20% |
| 11 | In the client window, console `NetEmulation.PktLag 150`, then hold Shift until stamina runs out | No position snapping when sprint ends; stamina bar stops at 0 |
| 12 | Still laggy: tap Shift repeatedly | Each tap costs exactly one 2.5 drain tick, not more; sprint speed stops within ~one round trip of releasing |
| 13 | Still laggy: press Shift during Dodge / Chakra Charge | Sprint is blocked (can't start); no position snapping |
| 14 | Still laggy: dodge in each direction and a backstep | Dash goes the pressed direction ~600 units, no snap-back; backstep with no input |
| 15 | Client window, `NetEmulation.PktLag 120` + `NetEmulation.PktLagVariance 40` + `NetEmulation.PktLoss 2` + `p.NetShowCorrections 1`: attack combos while swinging the camera | No correction snaps during attack lunges; combo doesn't drop mid-swing (fixed lag alone doesn't reproduce the late-press race; the variance and loss are what surface it) |

- [ ] **Step 3: Record results**

Append a short "M1 manual check" note (date, pass/fail per row, fixes made) to the bottom of this plan file and commit:
```powershell
git add docs/superpowers/plans/2026-09-24-m0-m1-setup-and-core.md
git commit -m "docs: record M1 manual play check

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 4: Tag the milestone**

```powershell
git tag m1-core
```

---

## Self-review

**Spec coverage (M0 + M1 rows of §8):**
- M0 "project builds; plugins enabled; git + LFS; automation test harness runs" → Tasks 1–3 (harness proven in Task 4).
- M1 movement, sprint, double jump → Tasks 6, 10 (`JumpMaxCount = 2`).
- Dodge (600 units / 0.25 s, 0.2 s i-frames, 25 stamina, blocked below 25) → Tasks 5, 7.
- Melee combo 20/25/40, 0.5 s window, third-hit knockback, +5 chakra per hit → Task 9.
- Chakra charge +25/s, movement locked → Tasks 5, 8, 10.
- Attribute values (500/100/100/3), chakra regen +2/s, stamina regen +20/s after 1 s → Tasks 4, 5.
- Attribute bars → Task 11. Training dummy takes damage → Task 11. Replicated to a second client → Tasks 13, 14.
- Deferred by design (later milestones): jutsu, statuses, interrupts, substitution teleport (M2); enemies, waves, downed/revive (M3); Steam, menus (M4); toon look, hit-stop, damage numbers (M5). Chakra charge being cancelled by hits needs `Event.Hit.Stagger`, which arrives with the damage execution in M2.

**Placeholder scan:** no TBD/TODO; every code step has complete code.

**Type consistency:** `EShinobiAbilityInputID` {None, Melee, Dodge, Sprint, ChakraCharge} is used consistently by the abilities, the input config and the tests. `ShinobiTest::*` helpers match their uses. The effect class names (`UGE_Damage`, `UGE_ChakraGain`, `UGE_ChakraRegen`, `UGE_StaminaRegen`, `UGE_StaminaRegenDelay`, `UGE_SprintDrain`, `UGE_DodgeCost`, `UGE_DodgeIFrames`, `UGE_ChakraCharge`) match between header, cpp and tests. Montage paths match between `setup_m1.py` and the ability constructors.

---

## Carry-forward notes for M2+

- `UGE_Damage` currently refuses application while the target has `State.Invulnerable`. When M2 adds `UShinobiDamageExec` (perfect dodge → Substitution), remove that application requirement and let the exec zero the damage and fire `Event.Substitution`, otherwise the exec never runs during i-frames.
- `OnOutOfHealth` carries no instigator; M3 downed/kill logic that needs the attacker should pass the GE context (e.g. broadcast from PostGameplayEffectExecute with `Data.EffectSpec.GetContext()`).
- Raising a Max attribute is order-sensitive: `PreAttributeBaseChange` clamps the current value to the *current* Max, so enemy HP scaling and wave restores must raise MaxHealth (etc.) before setting Health. Lowering a Max re-clamps the current value automatically (`PostAttributeChange`, Task 4 review fix); listeners on the Max attribute's change delegate then see a null `GEModData`.
- Invulnerability is only an application requirement on `UGE_Damage`: periodic damage applied before a dodge (Burn, Black Flame) keeps ticking through i-frames, and a blocked application reports nothing, so Substitution can't detect it. M2's damage exec should be the single place that checks `State.Invulnerable`.
- Dodge direction is computed separately on client and server from current acceleration (one frame stale; a lost/reordered move makes the server's root-motion force mismatch → corrections for the whole dash). Before M4 internet play (or earlier if `NetEmulation.PktLag`/`PktLoss` tests show corrections), send the direction to the server explicitly (target data or gameplay-event activation) using the freshest input (`GetPendingMovementInputVector()`, fallback acceleration).
- i-frames are server-only (correct for authority) but shifted by ~one RTT for remote players; if lag playtests show "dodged but got hit", extend server i-frames by min(RTT/2, 100 ms) or give Substitution a grace window.
- Substitution must cancel GA_Dodge before teleporting, or the constant force keeps pushing the teleported character.
- GA_Melee snaps yaw from control rotation on both sides; the server's value can be a move stale. If PktLag tests show corrections during root-motion attack lunges, send the client's yaw to the server under the activation key (ServerSetReplicatedEventWithPayload / CallOrAddReplicatedDelegate).
- Melee presses during a swing are dropped (no input buffer); M5 feel pass: start the input wait at swing start with a queued flag. Also cache loaded montages in a UPROPERTY on the ability instance (GA_Melee/GA_Dodge call LoadSynchronous on every use).
- M3: FindHostilesInCone must skip downed/dead targets; server-only LaunchCharacter on a remote player target rubber-bands — use a root-motion-source hit reaction for player targets.
- Held ability input (Sprint/ChakraCharge) is not released if the pawn is unpossessed mid-hold; both end on their own (stamina 0 / chakra full). If M2/M3 respawn or cutscenes make this visible, call AbilityLocalInputReleased for held IDs from a NotifyControllerChanged override when control is lost.
- UShinobiInputConfig is a rooted process-wide singleton; M2's RB jutsu-modifier context should be built the same way and added with a priority above ShinobiInputPriority.
- Cheat paths (ServerDamageSelf RPC body, -ShinobiSmoke hook) are compiled into Shipping. Before M4 public sessions, early-return in ServerDamageSelf_Implementation and skip RunSmokeTest under UE_BUILD_SHIPPING (UHT can't wrap the UFUNCTION declarations themselves).
- M3: AShinobiTrainingDummy::HandleOutOfHealth calls Super — when the base class gains downed handling, dummies must not go downed (override without Super, or keep downed logic in the player class). The HUD should collapse when there's no pawn (respawn wait).
