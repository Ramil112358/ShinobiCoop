# Menu and session smoke test over LAN (-nosteam: the Null subsystem): a host process hosts from the main menu, a
# client process finds that game and joins it through the menu; then the host quits and the client must land back
# on the main menu with a "lost the connection" message. On the way, each player's roster pick (saved picks are
# overridden on the command line) must show on both machines. ponytail: the roster is only the hero until the
# photo-based heroes join; then give host and client different picks here.
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Editor = "$UE\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$HostLog = Join-Path $LogDir "SmokeM4Host.log"
$ClientLog = Join-Path $LogDir "SmokeM4Client.log"

Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" | Where-Object { $_.CommandLine -match '-ShinobiMenuSmoke' } | ForEach-Object {
    Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $_.ProcessId -Timeout 10 -ErrorAction SilentlyContinue
}
Remove-Item -LiteralPath $HostLog, $ClientLog -ErrorAction SilentlyContinue
$Common = "`"$Project`" /Game/Shinobi/Maps/Lvl_MainMenu -game -nullrhi -nosound -unattended -nosplash -nosteam -ShinobiMenuSmoke"
function Pick($Name) { "`"-ini:GameUserSettings:[/Script/ShinobiCoop.ShinobiSettings]:Character=$Name`"" }

function Wait-Log($Log, $Pattern, $Seconds, $What) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while (-not ((Test-Path -LiteralPath $Log) -and (Select-String -LiteralPath $Log -Pattern $Pattern -Quiet))) {
        if ((Get-Date) -gt $deadline) { throw "SMOKE FAIL: $What (no '$Pattern' in $Log within $Seconds s)" }
        Start-Sleep -Seconds 2
    }
    Write-Host "ok: $What"
}

$hostProc = Start-Process $Editor -PassThru -ArgumentList "$Common $(Pick Hero) -ShinobiAutoHost -abslog=`"$HostLog`""
$client = $null
try {
    Wait-Log $HostLog "Online subsystem: Null" 240 "host fell back to LAN mode"
    Wait-Log $HostLog "Bringing World /Game/Shinobi/Maps/Lvl_HiddenLeafValley.* up for play" 180 "host created the game and opened Hidden Leaf Valley"
    $client = Start-Process $Editor -PassThru -ArgumentList "$Common $(Pick Hero) -ShinobiAutoJoin -abslog=`"$ClientLog`""
    Wait-Log $ClientLog "Welcomed by server" 240 "client found and joined the game from the menu"
    Wait-Log $HostLog "Join succeeded" 30 "host accepted the client"
    Wait-Log $HostLog "plays Hero" 30 "host sees the client's pick"
    Wait-Log $ClientLog "plays Hero" 30 "client sees the host's pick"
    Wait-Log $ClientLog "plays Hero" 30 "client sees its own pick"
    Start-Sleep -Seconds 3
    Stop-Process $hostProc -Force
    Wait-Log $ClientLog "Session: Lost the connection" 90 "client noticed the host leaving"
    Wait-Log $ClientLog "Bringing World /Game/Shinobi/Maps/Lvl_MainMenu.* up for play" 60 "client returned to the main menu"
    Write-Host "SMOKE M4 PASS"
}
finally {
    Stop-Process $hostProc -Force -ErrorAction SilentlyContinue
    if ($client) { Stop-Process -Id $client.Id -Force -ErrorAction SilentlyContinue }
}
