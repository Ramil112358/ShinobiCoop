# PvP arena smoke test over LAN (-nosteam: the Null subsystem): a host process hosts the Westminster arena 1v1 from the
# main menu, a client process finds that game and joins it through the menu. The host must put the two humans on opposite
# teams (no bots: both slots are human, so the warmup ends the moment the client is in), run the countdown and start round 1;
# then the host kills team B itself (-ShinobiPvpSmokeKill=B@6, six seconds into the fight, the same path as the
# Shinobi.KillTeam console command), round 1 must be won by A, and the client must log the replicated score A 1 - B 0.
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Editor = "$UE\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$HostLog = Join-Path $LogDir "SmokePvpHost.log"
$ClientLog = Join-Path $LogDir "SmokePvpClient.log"

Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" | Where-Object { $_.CommandLine -match '-ShinobiMenuSmoke' } | ForEach-Object {
    Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $_.ProcessId -Timeout 10 -ErrorAction SilentlyContinue
}
Remove-Item -LiteralPath $HostLog, $ClientLog -ErrorAction SilentlyContinue
$Common = "`"$Project`" /Game/Shinobi/Maps/Lvl_MainMenu -game -nullrhi -nosound -unattended -nosplash -nosteam -ShinobiMenuSmoke"

function Wait-Log($Log, $Pattern, $Seconds, $What) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while (-not ((Test-Path -LiteralPath $Log) -and (Select-String -LiteralPath $Log -Pattern $Pattern -Quiet))) {
        if ((Get-Date) -gt $deadline) { throw "SMOKE FAIL: $What (no '$Pattern' in $Log within $Seconds s)" }
        Start-Sleep -Seconds 2
    }
    Write-Host "ok: $What"
}

$hostProc = Start-Process $Editor -PassThru -ArgumentList "$Common -ShinobiAutoHost -ShinobiAutoHostArena=Westminster -ShinobiAutoHostFormat=1v1 -ShinobiPvpSmokeKill=B@6 -abslog=`"$HostLog`""
# The client starts together with the host, not after it: the warmup lasts 15 s (or until both slots are human), and a client
# started once the arena is up needs longer than that just to load its menu and search, so a bot would take its slot.
$client = $null
try {
    $client = Start-Process $Editor -PassThru -ArgumentList "$Common -ShinobiAutoJoin -abslog=`"$ClientLog`""
    Wait-Log $HostLog "Online subsystem: Null" 240 "host fell back to LAN mode"
    Wait-Log $HostLog "Bringing World /Game/Shinobi/Maps/Lvl_Arena_Westminster.* up for play" 180 "host created the game and opened the Westminster arena"
    Wait-Log $HostLog "Pvp: warmup \(1v1\)" 60 "host is in the 1v1 warmup"
    Wait-Log $ClientLog "Welcomed by server" 240 "client found and joined the arena from the menu"
    Wait-Log $HostLog "Pvp: \S+ joins team A" 30 "host put a human on team A"
    Wait-Log $HostLog "Pvp: \S+ joins team B" 30 "host put a human on team B"
    Wait-Log $HostLog "Pvp: round 1 start" 60 "round 1 started on the host"
    if (Select-String -LiteralPath $HostLog -Pattern "fills a slot" -Quiet) { throw "SMOKE FAIL: a bot filled a slot in a 2-human 1v1 ($HostLog)" }
    Write-Host "ok: no bots with both slots human"
    Wait-Log $HostLog "Pvp: KillTeam B killed 1" 30 "host killed team B's fighter"
    Wait-Log $HostLog "Pvp: round 1 won by A" 30 "host judged round 1 for team A"
    Wait-Log $ClientLog "Pvp: score A 1 - B 0" 30 "client received the replicated score A 1 - B 0"
    Write-Host "SMOKE PVP PASS"
}
finally {
    Stop-Process $hostProc -Force -ErrorAction SilentlyContinue
    if ($client) { Stop-Process -Id $client.Id -Force -ErrorAction SilentlyContinue }
}
