# Builds the standalone Windows game (BuildCookRun: compile, cook, stage, pak + IoStore) into Packaged\Windows\
# (ShinobiCoop.exe). Maps and always-cooked folders are in Config/DefaultGame.ini [ProjectPackagingSettings].
#   -Config Shipping: the release build (no console, no logs); default Development (console + Saved\Logs in the build).
#   -SkipBuild: reuse the compiled game binaries (content-only changes).
# Close the editor first. The first cook takes hours (City Sample crowd, London); later ones reuse the cook.
param([ValidateSet("Development", "Shipping")][string]$Config = "Development", [switch]$SkipBuild)
$ErrorActionPreference = "Stop"
$UAT = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$Out = Join-Path $Root "Packaged"
$Log = Join-Path $Root "Saved\Logs\Package-$(Get-Date -Format yyyyMMdd-HHmmss).log"
$Build = if ($SkipBuild) { "-skipbuild" } else { "-build" }
# (no -zenstore: its store lives on C:, and 5.8 packaging with it has failed for others)
& $UAT BuildCookRun "-project=$Project" -platform=Win64 "-clientconfig=$Config" -noP4 -utf8output -unattended `
    $Build -cook -stage -pak -iostore -compressed -prereqs -archive "-archivedirectory=$Out" 2>&1 | Tee-Object -FilePath $Log
if ($LASTEXITCODE -ne 0) { throw "Packaging failed (exit $LASTEXITCODE); see $Log" }
Write-Host "PACKAGED: $Out\Windows\ShinobiCoop.exe"
