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
Remove-Item $ServerLog, $ClientLog -ErrorAction SilentlyContinue
$Common = "-game -nullrhi -nosound -unattended -nosplash -ShinobiSmoke"

$server = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" /Game/ThirdPerson/Lvl_ThirdPerson?listen $Common -abslog=`"$ServerLog`""
try {
    $deadline = (Get-Date).AddSeconds(240)
    while (-not ((Test-Path $ServerLog) -and (Select-String -Path $ServerLog -Pattern "Bringing World .* up for play" -Quiet))) {
        if ((Get-Date) -gt $deadline) { throw "Server did not start within 240s" }
        Start-Sleep -Seconds 2
    }

    $client = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" 127.0.0.1 $Common -abslog=`"$ClientLog`""
    if (-not $client.WaitForExit(240000)) { Stop-Process $client -Force; throw "Client did not finish within 240s" }
}
finally {
    Stop-Process $server -Force -ErrorAction SilentlyContinue
}

$hit = Select-String -Path $ClientLog -Pattern "ShinobiStats .* local=1 H=450\.0"
if ($hit) {
    Write-Host "SMOKE PASS: $($hit.Line)"
} else {
    Select-String -Path $ClientLog -Pattern "ShinobiStats|Error" | Select-Object -Last 20 | ForEach-Object { Write-Host $_.Line }
    throw "SMOKE FAIL: client never saw its replicated Health drop to 450"
}
