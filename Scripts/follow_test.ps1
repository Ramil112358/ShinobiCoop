# Summons the dogs, toad and snake on a map, walks away from them for four seconds and prints how far each is from the player
# (-ShinobiFollowTest). They should close up to ~4-8 m while the player walks and settle there once it stops.
#   .\Scripts\follow_test.ps1 -Map /Game/Shinobi/Maps/Lvl_London
# Opens a visible window for about 30 s; close the GUI editor first only if you rebuilt.
param([string]$Map = "/Game/Shinobi/Maps/Lvl_London", [int]$TimeoutSec = 300)
$ErrorActionPreference = "Stop"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$Log = Join-Path $Root "Saved\Logs\FollowTest.log"
Remove-Item $Log -ErrorAction SilentlyContinue
$game = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" $Map -game -nosteam -windowed -ResX=960 -ResY=540 -nosound -unattended -nosplash -ShinobiFollowTest -abslog=`"$Log`""
$null = $game.Handle
if (-not $game.WaitForExit($TimeoutSec * 1000)) { Stop-Process $game -Force; throw "Game did not quit within $TimeoutSec s" }
Select-String -Path $Log -Pattern "ShinobiFollow " | ForEach-Object { $_.Line -replace '^.*ShinobiFollow ', '' }
