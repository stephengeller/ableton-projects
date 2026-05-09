# Package the current Windows build of ClipToZero into a shareable zip.
#
# Usage:
#   .\dist\package-windows.ps1                # rebuild + zip
#   .\dist\package-windows.ps1 -SkipBuild     # zip whatever is in build/
#
# Output: dist\output\ClipToZero-vX.Y.Z-windows.zip

[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$ScriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$PluginRoot  = Resolve-Path (Join-Path $ScriptDir "..")
Set-Location $PluginRoot

# ---- Resolve version from CMakeLists -----------------------------------
$cmakeContent = Get-Content "CMakeLists.txt" -Raw
if ($cmakeContent -match 'project\(.*VERSION\s+([0-9.]+)') {
    $Version = $Matches[1]
} else {
    $Version = "0.0.0-dev"
}

try {
    $GitSha = (git rev-parse --short HEAD 2>$null).Trim()
} catch {
    $GitSha = "nogit"
}
if (-not $GitSha) { $GitSha = "nogit" }

$PkgName = "ClipToZero-v$Version-windows"
$OutDir  = Join-Path $PluginRoot "dist\output"
$PkgDir  = Join-Path $OutDir $PkgName
$ZipPath = Join-Path $OutDir "$PkgName.zip"

Write-Host "==> ClipToZero packaging (Windows)"
Write-Host "    version: $Version (git $GitSha)"
Write-Host "    output:  $ZipPath"

if (-not $SkipBuild) {
    Write-Host "==> Building Release"
    cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) {
        throw "cmake --build failed (exit $LASTEXITCODE)"
    }
}

# ---- Verify expected artefacts exist -----------------------------------
$ArtDir   = Join-Path $PluginRoot "build\ClipToZero_artefacts\Release"
$Vst3Src  = Join-Path $ArtDir "VST3\ClipToZero.vst3"
$ExeSrc   = Join-Path $ArtDir "Standalone\ClipToZero.exe"

if (-not (Test-Path $Vst3Src)) {
    throw "Expected VST3 missing: $Vst3Src (run without -SkipBuild)"
}

# ---- Stage the package directory ---------------------------------------
Write-Host "==> Staging $PkgDir"
if (Test-Path $PkgDir)  { Remove-Item -Recurse -Force $PkgDir }
if (Test-Path $ZipPath) { Remove-Item -Force $ZipPath }
New-Item -ItemType Directory -Path $PkgDir            -Force | Out-Null
New-Item -ItemType Directory -Path "$PkgDir\VST3"     -Force | Out-Null

Copy-Item -Recurse $Vst3Src "$PkgDir\VST3\ClipToZero.vst3"

if (Test-Path $ExeSrc) {
    New-Item -ItemType Directory -Path "$PkgDir\Standalone" -Force | Out-Null
    Copy-Item $ExeSrc "$PkgDir\Standalone\"
}

Copy-Item (Join-Path $ScriptDir "INSTALL.md") "$PkgDir\INSTALL.md"

$BuildInfo = @"
ClipToZero $Version
git: $GitSha
built: $(Get-Date -AsUTC -Format "yyyy-MM-ddTHH:mm:ssZ")
host: Windows $($PSVersionTable.OS)
"@
Set-Content -Path "$PkgDir\BUILD_INFO.txt" -Value $BuildInfo -Encoding UTF8

# ---- Create the zip ----------------------------------------------------
Write-Host "==> Archiving"
Compress-Archive -Path $PkgDir -DestinationPath $ZipPath -Force

$SizeMB = [math]::Round((Get-Item $ZipPath).Length / 1MB, 2)
Write-Host "==> Done."
Write-Host "    $ZipPath  ($SizeMB MB)"
