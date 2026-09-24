# Runs automation tests headless and prints one line per test. Exit code 1 on any failure.
param([string]$Filter = "ShinobiCoop")
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Log = Join-Path $LogDir "Tests.log"
Remove-Item $Log -ErrorAction SilentlyContinue

# One pre-quoted argument string: UE expects -Key="value with spaces", which PowerShell 5.1 mangles for native args.
$ArgLine = "`"$Project`" -ExecCmds=`"Automation RunTests $Filter;Quit`" -TestExit=`"Automation Test Queue Empty`" -unattended -nopause -nullrhi -nosplash -nosound -abslog=`"$Log`""
Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $ArgLine -Wait -NoNewWindow

$results = Select-String -Path $Log -Pattern "Test Completed\. Result=\{(\w+)\}.*Path=\{([^}]*)\}"
if (-not $results) { Write-Host "NO TESTS RAN. Tail of log:"; Get-Content $Log -Tail 40; exit 1 }
$failed = 0
foreach ($r in $results) {
    $outcome = $r.Matches[0].Groups[1].Value
    $name = $r.Matches[0].Groups[2].Value
    Write-Host ("{0,-8} {1}" -f $outcome, $name)
    if ($outcome -ne "Success") { $failed++ }
}
if ($failed -gt 0) {
    Select-String -Path $Log -Pattern "LogAutomationController: Error" | ForEach-Object { Write-Host $_.Line }
    Write-Host "$failed test(s) failed"
    exit 1
}
Write-Host "ALL $($results.Count) TESTS PASSED"
