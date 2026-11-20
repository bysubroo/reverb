# Builds the Windows installer (used by CI). Installs/updates Inno Setup if needed.
param([string]$BuildDir, [string]$Version, [string]$NumericVersion, [string]$OutDir)
$ErrorActionPreference = 'Stop'
choco upgrade innosetup -y --no-progress | Out-Null
$iscc = Get-ChildItem "C:\Program Files*\Inno Setup *\ISCC.exe" | Sort-Object FullName -Descending | Select-Object -First 1
Write-Host "Using $($iscc.FullName) $($iscc.VersionInfo.ProductVersion)"
$key = (Get-Content "$PSScriptRoot\..\packs-key.txt" -Raw).Trim()
& $iscc.FullName /Qp "/DAppVersion=$Version" "/DNumericVersion=$NumericVersion" "/DBuildDir=$BuildDir" "/DOutputDir=$OutDir" "/DPackKey=$key" "$PSScriptRoot\SubRooRooms.iss"
if ($LASTEXITCODE -ne 0) { exit 1 }
Get-ChildItem $OutDir
