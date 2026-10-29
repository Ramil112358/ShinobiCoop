# Runs an Unreal Python script through the editor's Python commandlet.
param(
    [Parameter(Mandatory)][string]$Script,
    [int]$TimeoutSec = 540,
    # Let the commandlet render (GPU work such as the landscape edit-layer merge in Scripts/world/build_world.py).
    [switch]$Render,
    # Run in a headless editor (-nullrhi, as run_tests.ps1) instead of the commandlet: for editor code that needs Slate,
    # such as Mutable's Customizable Object factory (Scripts/setup_mutable.py).
    [switch]$Editor
)
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Project = Join-Path $Root "ShinobiCoop.uproject"
$ScriptPath = (Resolve-Path $Script).Path
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Log = Join-Path $LogDir "Python-$Timestamp-$PID.log"
$StdOut = "$Log.stdout.txt"
$StdErr = "$Log.stderr.txt"

# UE expects -Key="value with spaces"; passing one pre-quoted argument string (instead of a
# PowerShell array) makes sure the embedded quotes reach UnrealEditor-Cmd.exe intact, since
# PowerShell 5.1 mangles quoting when native-process args are passed as an array.
$ArgLine = "`"$Project`" -run=pythonscript -script=`"$ScriptPath`" -unattended -nopause -nosplash -notraceserver -abslog=`"$Log`"$(if ($Render) { ' -AllowCommandletRendering' })"
if ($Editor) {
    # A launcher that runs the script, logs a traceback as a Python error, and always quits the editor.
    $Launcher = "$Log.launcher.py"
    @"
import runpy, traceback, unreal
try:
    runpy.run_path(r'$ScriptPath', run_name='__main__')
except BaseException:
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
"@ | Set-Content -Encoding utf8 $Launcher
    $ArgLine = "`"$Project`" -ExecutePythonScript=`"$Launcher`" -unattended -nopause -nullrhi -nosplash -nosound -notraceserver -abslog=`"$Log`""
}

$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $ArgLine -NoNewWindow -PassThru -RedirectStandardOutput $StdOut -RedirectStandardError $StdErr
$null = $p.Handle # PS 5.1 needs the handle touched before ExitCode is readable after the process exits
$TimedOut = -not $p.WaitForExit($TimeoutSec * 1000)
if ($TimedOut) { taskkill /PID $p.Id /T /F | Out-Null }
$ExitCode = if ($TimedOut) { -1 } else { $p.ExitCode }
# The editor exits cleanly after a failed py command: a Python traceback is the failure there.
if ($Editor -and $ExitCode -eq 0 -and (Select-String -Path $Log -Pattern "LogPython: Error" -Quiet)) { $ExitCode = 1 }

if ($TimedOut -or $ExitCode -ne 0) {
    if ($TimedOut) { Write-Host "TIMEOUT after $TimeoutSec s running $ScriptPath." }
    Write-Host "Tail of log:"
    if (Test-Path $Log) { Get-Content $Log -Tail 40 }
    Select-String -Path $Log -Pattern "Error:" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_.Line }
    Write-Host "Log: $Log"
    throw "Python script failed (exit $ExitCode)"
}

Select-String -Path $Log -Pattern "\[setup_|LogPython: Error|LogPython: Warning" | ForEach-Object { Write-Host $_.Line }
Write-Host "Log: $Log"
