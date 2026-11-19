<#
  Windows build: MSVC + Qt (msvc2022_64) + prebuilt libmpv. No MSYS2, no bash.

  Prerequisites:
    - Visual Studio 2022 (or Build Tools) with "Desktop development with C++"
    - Qt 6.6+ for msvc2022_64 with the qtimageformats module, e.g.
        pip install aqtinstall
        python -m aqt install-qt windows desktop 6.10.3 win64_msvc2022_64 -m qtimageformats -O $HOME\Qt
    - CMake 3.21+ (WiX Toolset for the installer is fetched by fetch_deps.ps1)

  Usage:
    .\scripts\build_windows.ps1 [-QtDir C:\path\to\Qt\6.10.3\msvc2022_64] [-NoInstaller] [-Tests]

  Output:
    dist\Andub\                      ready-to-run folder
    dist\Andub-Windows-x86_64.zip    portable version
    dist\Andub-Setup-x86_64.exe      one-file installer (WiX bundle with the MSI inside)
#>
param(
    [string]$QtDir = "",
    [switch]$NoInstaller,
    [switch]$Tests
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "build"
$Dist = Join-Path $Root "dist"
$Bundle = Join-Path $Dist "Andub"
Set-Location $Root

function Invoke-Native([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

# --- MSVC developer environment ---------------------------------------------
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Visual Studio / Build Tools not found (vswhere.exe missing)" }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "No Visual Studio instance with the C++ x64 tools" }
    Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
}

# --- Qt ---------------------------------------------------------------------
if (-not $QtDir) {
    if ($env:QT_ROOT_DIR) { $QtDir = $env:QT_ROOT_DIR }   # jurplel/install-qt-action
    else {
        $QtDir = Get-ChildItem (Join-Path $HOME "Qt") -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^6\.\d+\.\d+$' } |
            Sort-Object { [version]$_.Name } -Descending |
            ForEach-Object { Join-Path $_.FullName "msvc2022_64" } |
            Where-Object { Test-Path $_ } | Select-Object -First 1
    }
}
if (-not $QtDir -or -not (Test-Path (Join-Path $QtDir "bin\windeployqt.exe"))) {
    throw "Qt for msvc2022_64 not found; pass -QtDir <...\6.x.y\msvc2022_64>"
}
Write-Host "==> Qt: $QtDir"

# --- third-party binaries ---------------------------------------------------
& (Join-Path $PSScriptRoot "fetch_deps.ps1")

# --- build ------------------------------------------------------------------
$version = (Select-String -Path (Join-Path $Root "CMakeLists.txt") -Pattern 'project\(andub VERSION ([\d.]+)').Matches[0].Groups[1].Value
Write-Host "==> Building Andub $version"
$cmakeArgs = @("-S", $Root, "-B", $Build, "-G", "Visual Studio 17 2022", "-A", "x64", "-DCMAKE_PREFIX_PATH=$QtDir")
if ($Tests) { $cmakeArgs += "-DANDUB_BUILD_TESTS=ON" }
Invoke-Native cmake $cmakeArgs
Invoke-Native cmake @("--build", $Build, "--config", "Release", "--parallel")

# --- deploy -----------------------------------------------------------------
Write-Host "==> Deploying to $Bundle"
Remove-Item -Recurse -Force $Bundle -ErrorAction SilentlyContinue
Invoke-Native cmake @("--install", $Build, "--config", "Release", "--prefix", $Bundle)
Invoke-Native (Join-Path $QtDir "bin\windeployqt.exe") @(
    "--release", "--qmldir", (Join-Path $Root "qml"), "--no-translations",
    "--no-system-d3d-compiler", "--no-system-dxc-compiler", "--no-opengl-sw", "--no-compiler-runtime",
    (Join-Path $Bundle "andub.exe"))

# App-local MSVC runtime, so users don't need the VC++ Redistributable installed
$crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir "x64") -Directory -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
if (-not $crt) { throw "MSVC runtime not found under $env:VCToolsRedistDir" }
Copy-Item (Join-Path $crt.FullName "*.dll") $Bundle

# --- packages ---------------------------------------------------------------
$zip = Join-Path $Dist "Andub-Windows-x86_64.zip"
Remove-Item -Force $zip -ErrorAction SilentlyContinue
Compress-Archive -Path $Bundle -DestinationPath $zip -CompressionLevel Optimal

if (-not $NoInstaller) {
    # MSI first, then the Burn bundle that embeds it: one Setup.exe on the way out
    $wix = Join-Path $Root "third_party\wix\PFiles64\WiX Toolset v6.0\bin\wix.exe"
    $msi = Join-Path $Dist "Andub-x86_64.msi"
    $setup = Join-Path $Dist "Andub-Setup-x86_64.exe"
    Write-Host "==> Building MSI"
    Invoke-Native $wix @("build", "-arch", "x64", "-d", "Version=$version", "-bindpath", "Bundle=$Bundle",
        "-ext", "WixToolset.Util.wixext", "-culture", "uk-UA",
        (Join-Path $Root "installer\Package.wxs"), "-o", $msi)
    Write-Host "==> Building Setup.exe"
    Invoke-Native $wix @("build", "-arch", "x64", "-d", "Version=$version", "-d", "MsiPath=$msi",
        "-ext", "WixToolset.BootstrapperApplications.wixext", "-ext", "WixToolset.Util.wixext",
        (Join-Path $Root "installer\Bundle.wxs"), "-o", $setup)
    # The MSI lives inside Setup.exe; keep only the one-file installer in dist\
    Remove-Item -Force $msi, (Join-Path $Dist "*.wixpdb") -ErrorAction SilentlyContinue
}

Get-ChildItem $Dist -File | Format-Table Name, @{ n = "MB"; e = { [math]::Round($_.Length / 1MB, 1) } }
