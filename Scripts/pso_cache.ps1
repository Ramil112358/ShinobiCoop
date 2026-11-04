# Builds the bundled PSO cache: the pipeline states (shader + render state combinations) the jutsu, maps and menus use,
# compiled when the game starts instead of the first time each one is drawn (the hitch on a jutsu's first cast).
# docs/perf/2026-11-03-performance-audit.md, item 13; Config/DefaultEngine.ini has r.ShaderPipelineCache.Enabled and
# [DevOptions.Shaders] NeedsShaderStableKeys (the cook writes the shader keys this needs).
#
#   1. Scripts/package.ps1                       a Development build (its cook writes the .shk shader key files)
#   2. Scripts/pso_cache.ps1 -Record             plays the build with -logPSO through the screenshot harnesses (every
#                                                jutsu, the hail, the dragon, Susanoo, the arena look), recording each PSO
#   3. Scripts/pso_cache.ps1 -Expand             turns the recordings + shader keys into Build\Windows\PipelineCaches\*.spc
#   4. Scripts/package.ps1 -SkipBuild            (or -Config Shipping) bundles the cache into the build
# Re-run after big content changes (new jutsu, new maps): PSOs the cache doesn't have still compile on first use.
param([switch]$Record, [switch]$Expand, [int]$SecondsPerRun = 240)
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$Game = Join-Path $Root "Packaged\Windows\ShinobiCoop.exe"
$Collected = Join-Path $Root "Packaged\Windows\ShinobiCoop\Saved\CollectedPSOs"
$Keys = Join-Path $Root "Saved\Cooked\Windows\ShinobiCoop\Metadata\PipelineCaches"
$Out = Join-Path $Root "Build\Windows\PipelineCaches"
$Format = "PCD3D_SM6"

if (-not ($Record -or $Expand)) { throw "Pass -Record (play the packaged build, logging PSOs) and/or -Expand (build the .spc)." }

if ($Record) {
    if (-not (Test-Path $Game)) { throw "No packaged build at ${Game}: run Scripts/package.ps1 first (Development)." }
    # Each run casts / shows a set of effects and quits on its own (ShinobiPlayerController's harnesses: Development builds
    # only), on the map Scripts/shot_chakra.ps1 uses for it.
    $Test = "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/ShinobiCoop.ShinobiGameMode"
    $Runs = @(
        @($Test, "-ShinobiJutsuShot"), @($Test, "-ShinobiFireShot"), @($Test, "-ShinobiHailShot"), @($Test, "-ShinobiSusanooShot"),
        @($Test, "-ShinobiDragonShot"), @($Test, "-ShinobiMarkShot"), @("/Game/Shinobi/Maps/Lvl_TrainingGrounds", "-ShinobiLookShot"),
        @("/Game/Shinobi/Maps/Lvl_London", "-ShinobiLookShot")
    )
    foreach ($Run in $Runs) {
        Write-Host "Recording PSOs: $($Run -join ' ')"
        $Process = Start-Process -FilePath $Game -ArgumentList (@($Run[0], "-logPSO", "-nosteam", "-windowed", "-ResX=1920", "-ResY=1080") + $Run[1..($Run.Count - 1)]) -PassThru
        if (-not $Process.WaitForExit($SecondsPerRun * 1000)) {
            Write-Warning "$($Run -join ' ') still running after $SecondsPerRun s: stopping it (its PSOs so far are kept)"
            $Process.Kill()
        }
    }
    Write-Host "Also play a few minutes yourself with -logPSO (menus, the valley, London, a PvP arena), then run -Expand:"
    Write-Host "  & `"$Game`" -logPSO"
}

if ($Expand) {
    $Recordings = Get-ChildItem -Path $Collected -Filter "*.rec.upipelinecache" -ErrorAction SilentlyContinue
    $ShaderKeys = Get-ChildItem -Path $Keys -Filter "*.shk" -ErrorAction SilentlyContinue
    if (-not $Recordings) { throw "No recordings (*.rec.upipelinecache) in ${Collected}: run -Record first." }
    if (-not $ShaderKeys) { throw "No shader keys (*.shk) in ${Keys}: package once with NeedsShaderStableKeys (Config/DefaultEngine.ini)." }
    New-Item -ItemType Directory -Force $Out | Out-Null
    $Cache = Join-Path $Out "PSO_ShinobiCoop_$Format.spc"
    # One pre-quoted argument string (as Scripts/run_python.ps1): PowerShell 5.1 mangles embedded quotes in an argument array.
    $Files = (@($Recordings) + @($ShaderKeys) | ForEach-Object { "`"$($_.FullName)`"" }) -join " "
    $Arguments = "`"$Project`" -run=ShaderPipelineCacheTools expand $Files `"$Cache`" -unattended -nopause"
    $Tool = Start-Process -FilePath (Join-Path $UE "Engine\Binaries\Win64\UnrealEditor-Cmd.exe") -ArgumentList $Arguments -NoNewWindow -Wait -PassThru
    if ($Tool.ExitCode -ne 0) { throw "ShaderPipelineCacheTools expand failed (exit $($Tool.ExitCode))" }
    Write-Host "PSO cache: $Cache ($($Recordings.Count) recordings). Package again (Scripts/package.ps1 -SkipBuild) to bundle it."
}
