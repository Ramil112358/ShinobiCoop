# Launches the game in a 1280x720 window and takes scripted screenshots, then quits.
#   default:  -ShinobiShot: the camera turns to face the character, holds chakra charge (idle, 3x charging,
#             released), then throws a Fireball for a HUD shot.
#   -Jutsu:   -ShinobiJutsuShot: casts each jutsu at the training dummies ahead and screenshots every effect
#             (hand orbs, fireball, burn, black flame, Chidori, Rasengan + Firestorm, Substitution log).
# Opens a visible window for ~15-60 s; close the GUI editor first.
param([switch]$Jutsu)
$ErrorActionPreference = "Stop"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$ShotRoot = Join-Path $Root "Saved\Screenshots"
$Flag = if ($Jutsu) { "ShinobiJutsuShot" } else { "ShinobiShot" }
$Pattern = if ($Jutsu) { "ShinobiJutsuShot_*.png" } else { "ShinobiShot_*.png" }

Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Remove-Item
$game = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" /Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/ShinobiCoop.ShinobiGameMode -game -windowed -ResX=1280 -ResY=720 -nosound -unattended -nosplash -$Flag -log=$Flag.log"
$null = $game.Handle
if (-not $game.WaitForExit(300000)) { Stop-Process $game -Force; throw "Game did not quit within 300s" }

$shots = Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Sort-Object Name
if (-not $shots) { throw "No $Pattern under $ShotRoot; see Saved\Logs\$Flag.log" }
$shots | ForEach-Object { Write-Host $_.FullName }
