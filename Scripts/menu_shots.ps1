# Main-menu screenshots. Opens a visible game window; close the GUI editor first.
#   -Tour: -ShinobiMenuTour on Lvl_MainMenu: every tab, an arena in 2v2, Find Games (Saved\Screenshots\...\ShinobiMenuTour_*.png).
param([switch]$Tour, [int]$TimeoutSec = 180)
$ErrorActionPreference = "Stop"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$ShotRoot = Join-Path $Root "Saved\Screenshots"

function Run-Game([string]$Level, [string]$Flags, [string]$Pattern, [string]$LogName) {
    Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Remove-Item
    $game = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" $Level -game -nosteam -windowed -ResX=1280 -ResY=720 -nosound -unattended -nosplash $Flags -log=$LogName.log"
    $null = $game.Handle
    if (-not $game.WaitForExit($TimeoutSec * 1000)) { Stop-Process $game -Force; throw "Game did not quit within $TimeoutSec s" }
    $shots = Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Sort-Object Name
    if (-not $shots) { throw "No $Pattern under $ShotRoot; see Saved\Logs\$LogName.log" }
    $shots | ForEach-Object { Write-Host $_.FullName }
}

if ($Tour) { Run-Game "/Game/Shinobi/Maps/Lvl_MainMenu" "-ShinobiMenuTour" "ShinobiMenuTour_*.png" "ShinobiMenuTour" }
