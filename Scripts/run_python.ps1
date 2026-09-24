# Runs an Unreal Python script through the editor's Python commandlet.
param([Parameter(Mandatory)][string]$Script)
$ErrorActionPreference = "Stop"
$UE = "C:\Program Files\Epic Games\UE_5.8"
$Project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\ShinobiCoop.uproject"))
$ScriptPath = (Resolve-Path $Script).Path
$ArgLine = "`"$Project`" -run=pythonscript -script=`"$ScriptPath`" -unattended -nopause -nosplash"
$p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $ArgLine -Wait -NoNewWindow -PassThru
if ($p.ExitCode -ne 0) { throw "Python script failed (exit $($p.ExitCode))" }
