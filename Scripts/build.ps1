# Builds the editor target. Close the Unreal Editor first (Live Coding locks the DLLs).
param([string]$Config = "Development")
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Project = (Resolve-Path (Join-Path $PSScriptRoot "..\ShinobiCoop.uproject")).Path
& "$UE\Engine\Build\BatchFiles\Build.bat" ShinobiCoopEditor Win64 $Config "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }
Write-Host "BUILD OK"
