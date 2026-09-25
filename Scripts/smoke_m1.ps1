# Starts a listen server and a client as separate -game processes (no rendering), has the client
# damage itself through a server RPC, and asserts the client sees the replicated Health.
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
$Common = "-game -nullrhi -nosound -unattended -nosplash -ShinobiSmoke"

$server = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" /Game/ThirdPerson/Lvl_ThirdPerson?listen $Common -abslog=`"$ServerLog`""
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
if ($hit) {
    Write-Host "SMOKE PASS: $($hit.Line)"
} else {
    Select-String -LiteralPath $ClientLog -Pattern "ShinobiStats|Error" | Select-Object -Last 20 | ForEach-Object { Write-Host $_.Line }
    throw "SMOKE FAIL: client never saw its replicated Health drop to 450"
}
