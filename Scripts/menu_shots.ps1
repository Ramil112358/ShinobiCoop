# Main-menu pictures and screenshots. Opens visible game windows; close the GUI editor first.
#   -Packaged: in the built game (Packaged\Windows, Scripts/package.ps1) instead of the editor.
#   -Tour:      -ShinobiMenuTour on Lvl_MainMenu: every tab, an arena in 2v2, Find Games (Saved\Screenshots\...\ShinobiMenuTour_*.png).
#   -Portraits: -ShinobiMenuPortraits in a 3:4 window: each roster character -> Saved\MenuArt\T_Portrait_<Name>.png
#   -Maps:      -ShinobiMapShot on every Host Game map -> Saved\MenuArt\T_Map_<Id>.png; -Map <Id> shoots just that one.
# Then import Saved\MenuArt with: powershell -File Scripts/run_python.ps1 -Script Scripts/setup_menu_art.py
param([switch]$Tour, [switch]$Portraits, [switch]$Maps, [string]$Map = "", [switch]$Packaged, [int]$TimeoutSec = 180)
$ErrorActionPreference = "Stop"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$ShotRoot = Join-Path $Root "Saved\Screenshots"
$ProjectArg = "`"$Project`""
if ($Packaged) { # the built game (Scripts/package.ps1) instead of the editor
    $Editor = Join-Path $Root "Packaged\Windows\ShinobiCoop.exe"; $ProjectArg = ""
    $ShotRoot = Join-Path $Root "Packaged\Windows\ShinobiCoop\Saved\Screenshots"
}
$ArtDir = Join-Path $Root "Saved\MenuArt"
# Must match UShinobiSessionSubsystem::GetMaps() (the Id is the level name without "Lvl_").
$MapPaths = @(
    "/Game/Shinobi/Maps/Lvl_HiddenLeafValley", "/Game/Shinobi/Maps/Lvl_London", "/Game/Shinobi/Maps/Lvl_TrainingGrounds",
    "/Game/ThirdPerson/Lvl_ThirdPerson", "/Game/Variant_Combat/Lvl_Combat",
    "/Game/Shinobi/Maps/Lvl_Arena_WestEnd", "/Game/Shinobi/Maps/Lvl_Arena_Camden", "/Game/Shinobi/Maps/Lvl_Arena_Westminster", "/Game/Shinobi/Maps/Lvl_Arena_Tower"
)

function Run-Game([string]$Level, [string]$Flags, [string]$Pattern, [string]$LogName, [string]$Res = "-ResX=1280 -ResY=720") {
    Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Remove-Item
    $game = Start-Process $Editor -PassThru -ArgumentList "$ProjectArg $Level -game -nosteam -windowed $Res -nosound -unattended -nosplash $Flags -log=$LogName.log"
    $null = $game.Handle
    if (-not $game.WaitForExit($TimeoutSec * 1000)) { Stop-Process $game -Force; throw "Game did not quit within $TimeoutSec s" }
    $shots = Get-ChildItem $ShotRoot -Recurse -Filter $Pattern -ErrorAction SilentlyContinue | Sort-Object Name
    if (-not $shots) { throw "No $Pattern under $ShotRoot; see Saved\Logs\$LogName.log" }
    return $shots
}

if ($Tour) { Run-Game "/Game/Shinobi/Maps/Lvl_MainMenu" "-ShinobiMenuTour" "ShinobiMenuTour_*.png" "ShinobiMenuTour" | ForEach-Object { Write-Host $_.FullName } }

if ($Portraits -or $Maps -or $Map) { New-Item -ItemType Directory -Force $ArtDir | Out-Null }

if ($Portraits) {
    Run-Game "/Game/Shinobi/Maps/Lvl_MainMenu" "-ShinobiMenuPortraits" "ShinobiMenuPortrait_*.png" "ShinobiMenuPortraits" "-ResX=600 -ResY=800" | ForEach-Object {
        $Out = Join-Path $ArtDir ($_.Name -replace "^ShinobiMenuPortrait_", "T_Portrait_")
        Copy-Item $_.FullName $Out -Force
        Write-Host $Out
    }
}

if ($Maps -or $Map) {
    foreach ($Path in $MapPaths) {
        $Id = ($Path -split "/")[-1] -replace "^Lvl_", ""
        if ($Map -and $Map -ne $Id) { continue }
        Run-Game $Path "-ShinobiMapShot" "ShinobiMapShot_$Id.png" "ShinobiMapShot" | ForEach-Object {
            $Out = Join-Path $ArtDir "T_Map_$Id.png"
            Copy-Item $_.FullName $Out -Force
            Write-Host $Out
        }
    }
}
