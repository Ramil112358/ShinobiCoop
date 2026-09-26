# Launches the game standalone at the main menu (Steam needs a standalone game, not PIE). Close the editor first if
# you rebuilt. -Instances 2 opens two windows for a local test; Steam allows one per PC, so the second one runs in
# LAN mode. -NoSteam forces LAN mode on all of them.
param([int]$Instances = 1, [switch]$NoSteam)
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\ShinobiCoop.uproject"))
$Extra = if ($NoSteam) { "-nosteam" } else { "" }
for ($i = 0; $i -lt $Instances; $i++) {
    Start-Process $Editor -ArgumentList "`"$Project`" /Game/Shinobi/Maps/Lvl_MainMenu -game -windowed -ResX=1280 -ResY=720 -WinX=$(40 + 660 * $i) -WinY=60 $Extra -log=Play$i.log"
    if ($i -eq 0 -and $Instances -gt 1) { Start-Sleep -Seconds 8 } # let the first one claim Steam
}
