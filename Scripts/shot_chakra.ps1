# Launches the game in a 1280x720 window and takes scripted screenshots, then quits.
#   default:  -ShinobiShot: the camera turns to face the character, holds chakra charge (idle, 3x charging,
#             released), then throws a Fireball for a HUD shot.
#   -Jutsu:   -ShinobiJutsuShot: casts each jutsu at the training dummies ahead and screenshots every effect
#             (hand orbs, fireball, burn, black flame, Chidori, Rasengan + Firestorm, Substitution log).
# Opens a visible window for ~15-60 s; close the GUI editor first.
#   -Susanoo: -ShinobiSusanooShot: the Perfect Susanoo from four sides, the gameplay camera and a melee slash.
#   -Fire:    -ShinobiFireShot: a fire patch and a black-flame patch 5 m ahead, close-ups of the flame cards.
#   -Look:    -ShinobiLookShot: the arena (-Map, default Training Grounds) in wave mode: the view ahead, the character,
#             an overview, the first wave closing in and a fight.
#   -Tour:    -ShinobiWorldTour: flies to every tour point of the open world (village, gate, bridge, camps, boss);
#             -Map picks another world map (London: /Game/Shinobi/Maps/Lvl_London).
#   -Outfit Hokage: -ShinobiOutfitShot=Hokage: that outfit on every roster body from the front, side and back, idle,
#             walking, running, mid-jump and settled again (~80 s).
#   -Portrait RamilMH: -ShinobiPortraitShot=RamilMH (on -Map): that roster character's face up close (front, 3/4, side)
#             and its whole body from the front (~15 s); -Phone: the face from 38 cm with a 45 degree lens, like a selfie.
#   -Perf:    the tour at 1920x1080, Epic preset, no vsync, 10 s per point; prints the average frame time at each point.
#             Keep the game window in the foreground: Windows throttles background windows (about 2x slower).
param([string]$Outfit = "", [string]$Portrait = "", [switch]$Phone, [switch]$Susanoo, [switch]$Fire, [switch]$Jutsu, [switch]$Look, [switch]$Tour, [switch]$Perf, [int]$TimeoutSec = 300, [string]$Map = "/Game/Shinobi/Maps/Lvl_TrainingGrounds")
$ErrorActionPreference = "Stop"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$ShotRoot = Join-Path $Root "Saved\Screenshots"
if ($Perf) { $Tour = $true }
$Res = if ($Perf) { "-ResX=1920 -ResY=1080 -ExecCmds=`"scalability 3,r.VSync 0,t.MaxFPS 0`" -ShinobiPerf" } else { "-ResX=1280 -ResY=720" }
$Flag = if ($Portrait) { "ShinobiPortraitShot" } elseif ($Outfit) { "ShinobiOutfitShot" } elseif ($Tour) { "ShinobiWorldTour" } elseif ($Look) { "ShinobiLookShot" } elseif ($Fire) { "ShinobiFireShot" } elseif ($Susanoo) { "ShinobiSusanooShot" } elseif ($Jutsu) { "ShinobiJutsuShot" } else { "ShinobiShot" }
$Pattern = "$($Flag)_*.png"
$Level = if ($Portrait) { $Map } elseif ($Tour) { if ($PSBoundParameters.ContainsKey("Map")) { $Map } else { "/Game/Shinobi/Maps/Lvl_HiddenLeafValley" } } elseif ($Look) { $Map } else { "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/ShinobiCoop.ShinobiGameMode" }

Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Remove-Item
$game = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" $Level -game -nosteam -windowed $Res -nosound -unattended -nosplash $(if ($Portrait) { "-ShinobiPortraitShot=$Portrait$(if ($Phone) { ' -ShinobiPortraitPhone' })" } elseif ($Outfit) { "-ShinobiOutfitShot=$Outfit" } else { "-$Flag" }) -log=$Flag.log"
$null = $game.Handle
if (-not $game.WaitForExit($TimeoutSec * 1000)) { Stop-Process $game -Force; throw "Game did not quit within $TimeoutSec s" }

$shots = Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Sort-Object Name
if (-not $shots) { throw "No $Pattern under $ShotRoot; see Saved\Logs\$Flag.log" }
$shots | ForEach-Object { Write-Host $_.FullName }
if ($Perf) { Select-String -Path (Join-Path $Root "Saved\Logs\$Flag.log") -Pattern "ShinobiPerf .*" | ForEach-Object { Write-Host $_.Matches[0].Value } }
