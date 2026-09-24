# Runs automation tests headless and prints one line per test. Exit code 1 on any failure.
param([string]$Filter = "ShinobiCoop", [int]$TimeoutSec = 540)
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Log = Join-Path $LogDir "Tests-$Timestamp-$PID.log"
$StdOut = "$Log.stdout.txt"
$StdErr = "$Log.stderr.txt"

# UE expects -Key="value with spaces"; passing one pre-quoted argument string (instead of a
# PowerShell array) makes sure the embedded quotes reach UnrealEditor-Cmd.exe intact, since
# PowerShell 5.1 mangles quoting when native-process args are passed as an array.
$ArgLine = "`"$Project`" -ExecCmds=`"Automation RunTests $Filter;Quit`" -unattended -nopause -nullrhi -nosplash -nosound -notraceserver -abslog=`"$Log`""

$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $ArgLine -NoNewWindow -PassThru -RedirectStandardOutput $StdOut -RedirectStandardError $StdErr
$null = $p.Handle # PS 5.1 needs the handle touched before ExitCode is readable after the process exits
if (-not $p.WaitForExit($TimeoutSec * 1000)) {
    taskkill /PID $p.Id /T /F | Out-Null
    Write-Host "TIMEOUT after $TimeoutSec s waiting for UnrealEditor-Cmd.exe. Tail of log:"
    if (Test-Path $Log) { Get-Content $Log -Tail 40 }
    Write-Host "Log: $Log"
    exit 1
}
$ExitCode = $p.ExitCode

if (-not (Test-Path $Log)) {
    Write-Host "Log file not found (editor exit code $ExitCode). Tail of stdout:"
    if (Test-Path $StdOut) { Get-Content $StdOut -Tail 40 }
    Write-Host "Log: $Log"
    exit 1
}

$geLines = Select-String -Path $Log -Pattern "LogGameplayEffects: (Warning|Error)|Ensure condition failed|should be added to GEComponents"
if ($geLines) {
    Write-Host "--- GAS warnings / ensures ---"
    $geLines | ForEach-Object { Write-Host $_.Line }
}

$results = Select-String -Path $Log -Pattern "Test Completed\. Result=\{([^}]+)\}.*Path=\{([^}]*)\}"
if (-not $results) { Write-Host "NO TESTS RAN (editor exit code $ExitCode). Tail of log:"; Get-Content $Log -Tail 40; Write-Host "Log: $Log"; exit 1 }
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
    Write-Host "Log: $Log"
    exit 1
}

# All reported results were Success - but a crash mid-run can look the same (the crashed test,
# and everything queued after it, just never produces a result line). Cross-check the editor's
# own exit code, its own "test run complete" marker, and that we saw as many results as tests found.
$foundMatch = Select-String -Path $Log -Pattern "Found (\d+) automation tests based on" | Select-Object -Last 1
$expectedCount = if ($foundMatch) { [int]$foundMatch.Matches[0].Groups[1].Value } else { -1 }
$completeOk = [bool](Select-String -Path $Log -Pattern "\*\*\*\* TEST COMPLETE\. EXIT CODE: 0 \*\*\*\*" -Quiet)
if ($ExitCode -ne 0 -or -not $completeOk -or $results.Count -ne $expectedCount) {
    Write-Host "Run did not complete cleanly: editor exit code=$ExitCode, TEST COMPLETE EXIT CODE 0 found=$completeOk, result lines=$($results.Count), tests found=$expectedCount."
    Write-Host "Tail of log:"
    Get-Content $Log -Tail 40
    Write-Host "Log: $Log"
    exit 1
}

Write-Host "ALL $($results.Count) TESTS PASSED"
Write-Host "Log: $Log"
