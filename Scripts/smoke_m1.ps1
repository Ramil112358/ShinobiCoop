# Starts a listen server and a client as separate -game processes (no rendering). The client damages itself through a
# server RPC and throws a Fireball at the dummy ahead, then switches to jutsu page 2 and casts an Earth Wall and Kamui;
# asserts the client sees all of it through replication (its health, the burning dummy, the wall actor, its own phase).
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Editor = "$UE\Engine\Binaries\Win64\UnrealEditor.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$ServerLog = Join-Path $LogDir "SmokeServer.log"
$ClientLog = Join-Path $LogDir "SmokeClient.log"

# A previous run that was killed out from under this script (Ctrl+C, closed shell) can leave an
# UnrealEditor.exe still listening on port 7777; a fresh server would then be unreachable while the
# client silently joins the stale one instead. Clear those out before touching the logs.
Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" | Where-Object { $_.CommandLine -match '-ShinobiSmoke' } | ForEach-Object {
    Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $_.ProcessId -Timeout 10 -ErrorAction SilentlyContinue # release its log handle before the lock check
}

Remove-Item -LiteralPath $ServerLog, $ClientLog -ErrorAction SilentlyContinue
if ((Test-Path -LiteralPath $ServerLog) -or (Test-Path -LiteralPath $ClientLog)) {
    throw "Smoke log still locked by another process; close leftover UnrealEditor instances"
}
# -nosteam: Null online subsystem and the plain IP net driver, so the client can connect to 127.0.0.1.
$Common = "-game -nullrhi -nosound -unattended -nosplash -nosteam -ShinobiSmoke"

$server = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" /Game/ThirdPerson/Lvl_ThirdPerson?listen?game=/Script/ShinobiCoop.ShinobiGameMode $Common -abslog=`"$ServerLog`""
$null = $server.Handle # PS 5.1 only reports ExitCode if the handle was opened while the process ran
$client = $null
try {
    $deadline = (Get-Date).AddSeconds(240)
    while (-not ((Test-Path -LiteralPath $ServerLog) -and (Select-String -LiteralPath $ServerLog -Pattern "Bringing World .* up for play" -Quiet))) {
        if ($server.HasExited) { throw "Server exited before loading the map (exit $($server.ExitCode)); see $ServerLog" }
        if ((Get-Date) -gt $deadline) { throw "Server did not start within 240s" }
        Start-Sleep -Seconds 2
    }

    $client = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" 127.0.0.1 $Common -abslog=`"$ClientLog`""
    if (-not $client.WaitForExit(240000)) { Stop-Process $client -Force; throw "Client did not finish within 240s" }
}
finally {
    Stop-Process $server -Force -ErrorAction SilentlyContinue
    if ($client -and -not $client.HasExited) { Stop-Process -Id $client.Id -Force -ErrorAction SilentlyContinue }
}

$hit = Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats .* local=1 H=450\.0"
# The Fireball (cast at 5.5 s) hit the middle dummy: 60 direct plus burn ticks, and it's still burning at 9 s.
$burned = Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats ShinobiTrainingDummy\S* local=0 H=(\d+)\.\d .*Burn=1" |
    Where-Object { [double]$_.Matches[0].Groups[1].Value -le 440 }
# Susanoo (up from 8.3 s for 20 s), Ice Meteors (shards falling from 9.15 s to 13.65 s), Earth Wall (10.2 s) and Kamui
# (10.9 s, three seconds): at 11.3 s the client sees the armour, the wall, shards still in the sky, and its own phase.
$wall = Select-String -LiteralPath $ClientLog -Pattern "ShinobiJutsuWorld Page=2 Barriers=1 Kunai=0 Meteors=[1-9]"
$phased = Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats \S+ local=1 .* Phased=1"
$armour = Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats \S+ local=1 .* Susanoo=1"
# Pabu (cast at 7.1 s): at 11.3 s the client sees Pabu, replicated from the server.
$pabu = Select-String -LiteralPath $ClientLog -Pattern "ShinobiJutsuWorld .* Summons=1"
# The Dragon (cast at 14.4 s, after Kamui): the client sees itself mounted, and the dragon actor under it.
$mounted = Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats \S+ local=1 .* Mounted=1"
$dragon = Select-String -LiteralPath $ClientLog -Pattern "ShinobiJutsuWorld .* Dragons=1"
if ($hit -and $burned -and $wall -and $phased -and $armour -and $pabu -and $mounted -and $dragon) {
    Write-Host "SMOKE PASS: $($hit.Line)"
    Write-Host "SMOKE PASS: $($burned[0].Line)"
    Write-Host "SMOKE PASS: $($wall[0].Line)"
    Write-Host "SMOKE PASS: $($phased[0].Line)"
    Write-Host "SMOKE PASS: $($armour[0].Line)"
    Write-Host "SMOKE PASS: $($pabu[0].Line)"
    Write-Host "SMOKE PASS: $($mounted[0].Line)"
    Write-Host "SMOKE PASS: $($dragon[0].Line)"
} else {
    Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats|Error" | Select-Object -Last 20 | ForEach-Object { Write-Host $_.Line }
    Select-String -LiteralPath $ClientLog -Pattern "ShinobiJutsuWorld" | Select-Object -Last 3 | ForEach-Object { Write-Host $_.Line }
    if (-not $wall) { throw "SMOKE FAIL: client never saw the replicated Earth Wall and falling ice shards (page 2, one barrier, meteors in the sky)" }
    if (-not $mounted) { throw "SMOKE FAIL: client never saw itself mounted on the dragon" }
    if (-not $dragon) { throw "SMOKE FAIL: client never saw the replicated dragon" }
    if (-not $pabu) { throw "SMOKE FAIL: client never saw the summoned Pabu" }
    if (-not $armour) { throw "SMOKE FAIL: client never saw its own Susanoo armour" }
    if (-not $phased) { throw "SMOKE FAIL: client never saw its own Kamui phase" }
    if (-not $hit) { throw "SMOKE FAIL: client never saw its replicated Health drop to 450" }
    throw "SMOKE FAIL: client never saw a fireballed (H <= 440), burning dummy"
}
