# Launches the game in a 1280x720 window with -ShinobiShot: the camera turns to face the character, holds
# chakra charge, takes screenshots (idle, 3x charging, released), throws a Fireball for a HUD shot, and quits.
# Opens a visible window for ~15-60 s; close the GUI editor first.
$ErrorActionPreference = "Stop"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$ShotRoot = Join-Path $Root "Saved\Screenshots"

Get-ChildItem $ShotRoot -Recurse -Filter "ShinobiShot_*.png" -ErrorAction SilentlyContinue | Remove-Item
$game = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" /Game/ThirdPerson/Lvl_ThirdPerson -game -windowed -ResX=1280 -ResY=720 -nosound -unattended -nosplash -ShinobiShot -log=ShinobiShot.log"
$null = $game.Handle
if (-not $game.WaitForExit(300000)) { Stop-Process $game -Force; throw "Game did not quit within 300s" }

$shots = Get-ChildItem $ShotRoot -Recurse -Filter "ShinobiShot_*.png" -ErrorAction SilentlyContinue | Sort-Object Name
if (-not $shots) { throw "No ShinobiShot_*.png under $ShotRoot; see Saved\Logs\ShinobiShot.log" }
$shots | ForEach-Object { Write-Host $_.FullName }
