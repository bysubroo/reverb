# Silent install -> update in place -> uninstall test for the Windows installer (used by CI).
param([string]$Setup)
$ErrorActionPreference = 'Stop'
$packs = "$env:ProgramData\SubRoo Rooms\Packs"
$vst3 = "$env:CommonProgramFiles\VST3\SubRoo Rooms.vst3"
$clap = "$env:CommonProgramFiles\CLAP\SubRoo Rooms.clap"
$app = "$env:ProgramFiles\SubRoo\SubRoo Rooms"
function Fail($m) { Write-Host "::error::$m"; foreach ($l in 'install.log','install2.log') { if (Test-Path $l) { Write-Host "--- $l"; Get-Content $l -Tail 60 } }; exit 1 }

# Runs setup silently, shows its log while it runs, kills it after $TimeoutMin minutes.
function Run-Setup($log, $TimeoutMin = 12) {
    $p = Start-Process $Setup -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=$log" -PassThru
    $shown = 0; $deadline = (Get-Date).AddMinutes($TimeoutMin)
    while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep 5
        try {
            if (Test-Path $log) { $lines = @(Get-Content $log -ErrorAction Stop); if ($lines.Count -gt $shown) { $lines[$shown..($lines.Count-1)] | ForEach-Object { Write-Host "  | $_" }; $shown = $lines.Count } }
        } catch { }
    }
    if (-not $p.HasExited) { Get-Process | Where-Object { $_.ProcessName -like 'SubRoo-Rooms-*' } | Stop-Process -Force; Fail "setup did not finish within $TimeoutMin min" }
    return $p.ExitCode
}

# the pack archive must open with the key compiled into the installer
$packsIss = Get-Content "$PSScriptRoot\packs.iss" -Raw
$url = ([regex]'PackBaseUrl "([^"]+)"').Match($packsIss).Groups[1].Value + ([regex]'PackFile "([^"]+)"').Match($packsIss).Groups[1].Value
if ($env:PACKS_KEY) {
    Invoke-WebRequest $url -OutFile packs-check.7z
    $r = & "C:\Program Files\7-Zip\7z.exe" t "-p$($env:PACKS_KEY.Trim())" packs-check.7z
    if ($LASTEXITCODE -ne 0) { $r | Select-Object -Last 8; Fail "pack archive does not open with PACKS_KEY (wrong secret?)" }
    Write-Host "pack archive OK: $($r | Select-String 'Files:|Size:' | ForEach-Object { $_.Line.Trim() })"
    Remove-Item packs-check.7z
}

$t = Get-Date
$code = Run-Setup 'install.log'
if ($code -ne 0) { Fail "setup exit code $code" }
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
$code = Run-Setup 'install2.log' 5
if ($code -ne 0) { Fail "update exit code $code" }
if ((Get-Item "$packs\.packs-version").LastWriteTimeUtc -ne $stamp) { Fail "packs were reinstalled although up to date" }
Write-Host "update in place took $([int]((Get-Date) - $t).TotalSeconds) s"

# uninstall (the uninstaller re-launches itself from temp, so wait for the files to go)
Start-Process "$app\unins000.exe" -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -Wait
for ($i = 0; $i -lt 90 -and ((Test-Path "$app\unins000.exe") -or (Test-Path $packs)); $i++) { Start-Sleep 1 }
foreach ($f in @($vst3, $clap, "$app\SubRoo Rooms.exe", $packs)) { if (Test-Path $f) { Fail "not removed: $f" } }
Write-Host "install / update in place / uninstall OK"
