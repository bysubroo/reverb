# Builds the Windows installer (used by CI). Installs/updates Inno Setup if needed.
param([string]$BuildDir, [string]$Version, [string]$NumericVersion, [string]$OutDir)
$ErrorActionPreference = 'Stop'
choco upgrade innosetup -y --no-progress | Out-Null
$iscc = Get-ChildItem "C:\Program Files*\Inno Setup *\ISCC.exe" | Sort-Object FullName -Descending | Select-Object -First 1
Write-Host "Using $($iscc.FullName) $($iscc.VersionInfo.ProductVersion)"
$key = $env:PACKS_KEY
if (-not $key -and (Test-Path "$PSScriptRoot\..\packs-key.txt")) { $key = (Get-Content "$PSScriptRoot\..\packs-key.txt" -Raw) }
if ($key) { $key = $key.Trim() } else { Write-Host "::warning::PACKS_KEY not set - building the installer without packs" }
$packArg = if ($key) { "/DPackKey=$key" } else { "/DNoPackKey" }
& $iscc.FullName /Qp "/DAppVersion=$Version" "/DNumericVersion=$NumericVersion" "/DBuildDir=$BuildDir" "/DOutputDir=$OutDir" $packArg "$PSScriptRoot\SubRooRooms.iss"
if ($LASTEXITCODE -ne 0) { exit 1 }
Get-ChildItem $OutDir
