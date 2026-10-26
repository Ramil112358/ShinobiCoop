# Fits every outfit to every roster body (docs/character/outfit-steps.md): for each pair runs skin_outfit.py (the copy),
# bake_hide_mask.py (what of the body it hides) and check_outfit.py (posed acceptance), and writes each body's tail
# capsule (SourceArt/Character/Models/<Body>/<Body>_tail.json). Prints one RESULT line per pair; exit 1 if any fails.
#   -Outfit Hokage / -Body Cat: just that outfit / body. Check renders go to Saved/OutfitCheck/.
param([string]$Outfit = "", [string]$Body = "")
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$Blender = "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$Scripts = Join-Path $Root "Scripts\character"
$Bodies = @("Hero", "Bika", "Cat", "Misha", "Pabu", "Ramil", "RamilMH", "Elvin", "CrowdMNormal", "CrowdMOver", "CrowdMUnder", "CrowdFNormal", "CrowdFOver", "CrowdFUnder") | Where-Object { -not $Body -or $_ -eq $Body }
$Outfits = Get-ChildItem (Join-Path $Root "SourceArt\Character\Outfits") -Directory | Where-Object { -not $Outfit -or $_.Name -eq $Outfit }
$CheckDir = Join-Path $Root "Saved\OutfitCheck"
New-Item -ItemType Directory -Force $CheckDir | Out-Null

function Invoke-Blender([string]$Script, [string[]]$ArgList, [string]$Log) {
    $quoted = ($ArgList | ForEach-Object { "`"$_`"" }) -join " "
    $p = Start-Process $Blender -ArgumentList "-b --factory-startup --python-exit-code 1 -P `"$Script`" -- $quoted" -NoNewWindow -Wait -PassThru -RedirectStandardOutput $Log -RedirectStandardError "$Log.err"
    return $p.ExitCode
}

$results = @()
foreach ($b in $Bodies) {
    $bodyFbx = Join-Path $Root "SourceArt\Character\Models\$b\${b}_rigged.fbx"
    $tail = Join-Path $Root "SourceArt\Character\Models\$b\${b}_tail.json"
    if ((Invoke-Blender (Join-Path $Scripts "body_landmarks.py") @($bodyFbx, $tail) (Join-Path $CheckDir "${b}_tail.log")) -ne 0) { throw "tail of $b failed" }
}
foreach ($o in $Outfits) {
    $name = $o.Name
    # The master: the cloud-repainted one (fix_cloak_clouds.py) when there is one, else prep_outfit.py's.
    $master = @("${name}_clouds.glb", "${name}_tripo.glb", "$name.glb") | ForEach-Object { Join-Path $o.FullName $_ } | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $master) { Write-Host "skip ${name}: no master GLB"; continue }
    foreach ($b in $Bodies) {
        $bodyFbx = Join-Path $Root "SourceArt\Character\Models\$b\${b}_rigged.fbx"
        $copy = Join-Path $o.FullName "${name}_${b}_outfit.fbx"
        $hide = Join-Path $o.FullName "${name}_${b}_hide.png"
        $log = Join-Path $CheckDir "${name}_${b}"
        if ((Invoke-Blender (Join-Path $Scripts "skin_outfit.py") @($bodyFbx, $master, $o.FullName, $name, $b) "$log.fit.log") -ne 0) { $results += "RESULT FAIL $name $b (fit; see $log.fit.log)"; continue }
        if ($b -like "Crowd*") {
            # A crowd frame is hands, face and clothing pieces with overlapping UVs, in City Sample materials with no
            # HideMask: nothing is hidden, the copy's clearance alone keeps the clothes under it.
            $white = New-Object System.Drawing.Bitmap 8, 8
            [System.Drawing.Graphics]::FromImage($white).Clear([System.Drawing.Color]::White)
            $white.Save($hide, [System.Drawing.Imaging.ImageFormat]::Png); $white.Dispose()
        } elseif ((Invoke-Blender (Join-Path $Scripts "bake_hide_mask.py") @($bodyFbx, $copy, $hide) "$log.mask.log") -ne 0) { $results += "RESULT FAIL $name $b (mask; see $log.mask.log)"; continue }
        $code = Invoke-Blender (Join-Path $Scripts "check_outfit.py") @($bodyFbx, $copy, (Join-Path $o.FullName "${name}_outfit.png"), $hide, $log) "$log.check.log"
        $poses = (Select-String -Path "$log.check.log" -Pattern "^POSE" | ForEach-Object { $_.Line -replace "^POSE ", "" }) -join "; "
        $results += "RESULT $(if ($code -eq 0) { 'PASS' } else { 'FAIL' }) $name $b  $poses"
    }
}
$results | ForEach-Object { Write-Host $_ }
if ($results | Where-Object { $_ -like "RESULT FAIL*" }) { exit 1 }
