# Launches the game in a 1280x720 window and takes scripted screenshots, then quits.
#   default:  -ShinobiShot: the camera turns to face the character, holds chakra charge (idle, 3x charging,
#             released), then throws a Fireball for a HUD shot.
#   -Jutsu:   -ShinobiJutsuShot: casts each jutsu at the training dummies ahead and screenshots every effect
#             (hand orbs, fireball, burn, black flame, Chidori, Rasengan + Firestorm, Substitution log).
# Opens a visible window for ~15-60 s; close the GUI editor first.
#   -Look:    -ShinobiLookShot: the arena (-Map, default Training Grounds) in wave mode: the view ahead, the character,
#             an overview, the first wave closing in and a fight.
#   -Tour:    -ShinobiWorldTour: flies to every tour point of the open world (village, gate, bridge, camps, boss).
#   -Perf:    the tour at 1920x1080, Epic preset, no vsync, 10 s per point; prints the average frame time at each point.
#             Keep the game window in the foreground: Windows throttles background windows (about 2x slower).
param([switch]$Jutsu, [switch]$Look, [switch]$Tour, [switch]$Perf, [int]$TimeoutSec = 300, [string]$Map = "/Game/Shinobi/Maps/Lvl_TrainingGrounds")
$ErrorActionPreference = "Stop"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$ShotRoot = Join-Path $Root "Saved\Screenshots"
if ($Perf) { $Tour = $true }
$Res = if ($Perf) { "-ResX=1920 -ResY=1080 -ExecCmds=`"scalability 3,r.VSync 0,t.MaxFPS 0`" -ShinobiPerf" } else { "-ResX=1280 -ResY=720" }
$Flag = if ($Tour) { "ShinobiWorldTour" } elseif ($Look) { "ShinobiLookShot" } elseif ($Jutsu) { "ShinobiJutsuShot" } else { "ShinobiShot" }
$Pattern = "$($Flag)_*.png"
$Level = if ($Tour) { "/Game/Shinobi/Maps/Lvl_HiddenLeafValley" } elseif ($Look) { $Map } else { "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/ShinobiCoop.ShinobiGameMode" }

Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Remove-Item
$game = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" $Level -game -nosteam -windowed $Res -nosound -unattended -nosplash -$Flag -log=$Flag.log"
$null = $game.Handle
if (-not $game.WaitForExit($TimeoutSec * 1000)) { Stop-Process $game -Force; throw "Game did not quit within $TimeoutSec s" }

$shots = Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Sort-Object Name
if (-not $shots) { throw "No $Pattern under $ShotRoot; see Saved\Logs\$Flag.log" }
$shots | ForEach-Object { Write-Host $_.FullName }
if ($Perf) { Select-String -Path (Join-Path $Root "Saved\Logs\$Flag.log") -Pattern "ShinobiPerf .*" | ForEach-Object { Write-Host $_.Matches[0].Value } }
