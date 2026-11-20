# Silent install -> update in place -> uninstall test for the Windows installer (used by CI).
param([string]$Setup)
$ErrorActionPreference = 'Stop'
$packs = "$env:ProgramData\SubRoo Rooms\Packs"
$vst3 = "$env:CommonProgramFiles\VST3\SubRoo Rooms.vst3"
$clap = "$env:CommonProgramFiles\CLAP\SubRoo Rooms.clap"
$app = "$env:ProgramFiles\SubRoo\SubRoo Rooms"
function Fail($m) { Write-Host "::error::$m"; foreach ($l in 'install.log','install2.log') { if (Test-Path $l) { Write-Host "--- $l"; Get-Content $l -Tail 60 } }; exit 1 }

$t = Get-Date
$p = Start-Process $Setup -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LOG=install.log' -Wait -PassThru
if ($p.ExitCode -ne 0) { Fail "setup exit code $($p.ExitCode)" }
Write-Host "fresh install took $([int]((Get-Date) - $t).TotalSeconds) s"
foreach ($f in @("$vst3\Contents\x86_64-win\SubRoo Rooms.vst3", $clap, "$app\SubRoo Rooms.exe", "$packs\.packs-version")) {
    if (!(Test-Path $f)) { Fail "missing $f" }
}
$n = (Get-ChildItem $packs -Directory).Count
$w = (Get-ChildItem $packs -Recurse -Filter *.wav).Count
Write-Host "packs installed: $n folders, $w IRs"
if ($n -lt 200) { Fail "only $n packs" }
$stamp = (Get-Item "$packs\.packs-version").LastWriteTimeUtc

# second run = update in place; packs are current, so nothing is downloaded again
$t = Get-Date
$p = Start-Process $Setup -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LOG=install2.log' -Wait -PassThru
if ($p.ExitCode -ne 0) { Fail "update exit code $($p.ExitCode)" }
if ((Get-Item "$packs\.packs-version").LastWriteTimeUtc -ne $stamp) { Fail "packs were reinstalled although up to date" }
Write-Host "update in place took $([int]((Get-Date) - $t).TotalSeconds) s"

# uninstall (the uninstaller re-launches itself from temp, so wait for the files to go)
Start-Process "$app\unins000.exe" -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait
for ($i = 0; $i -lt 90 -and ((Test-Path "$app\unins000.exe") -or (Test-Path $packs)); $i++) { Start-Sleep 1 }
foreach ($f in @($vst3, $clap, "$app\SubRoo Rooms.exe", $packs)) { if (Test-Path $f) { Fail "not removed: $f" } }
Write-Host "install / update in place / uninstall OK"
