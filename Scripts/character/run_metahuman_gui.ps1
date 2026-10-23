# Runs Scripts/setup_metahuman.py stages inside the GUI editor (the build's TextureGraph bakes need a real editor), which
# then quits. Close any open editor first.
param(
    [Parameter(Mandatory)][string]$Stages,
    [string]$Name = "RamilMH",
    [string]$Quality = "HIGH",
    [int]$TimeoutSec = 2400
)
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$Log = Join-Path $Root ("Saved\Logs\MetaHumanGui-{0}.log" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
$env:SETUP_MH_ONLY = $Stages
$env:SETUP_MH_NAME = $Name
$env:SETUP_MH_QUALITY = $Quality
$env:SETUP_MH_QUIT = "1"
$Script = (Join-Path $Root "Scripts\setup_metahuman.py").Replace([char]92, [char]47)
# -ExecCmds, not -ExecutePythonScript (which exits as soon as the script returns): the build stage finishes over editor
# ticks and quits the editor itself.
$ArgLine = "`"$Root\ShinobiCoop.uproject`" -ExecCmds=`"py $Script`" -nosplash -unattended -abslog=`"$Log`""
$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $ArgLine -PassThru
$null = $p.Handle
if (-not $p.WaitForExit($TimeoutSec * 1000)) { taskkill /PID $p.Id /T /F | Out-Null; Write-Host "TIMEOUT" }
Select-String -Path $Log -Pattern "\[setup_metahuman\]|LogPython: Error|LogMetaHuman\w*: Error|MetaHuman: Error" | ForEach-Object { Write-Host $_.Line }
Write-Host "Log: $Log"
