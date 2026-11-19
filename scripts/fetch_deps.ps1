<#
  Downloads the prebuilt third-party pieces into third_party\ (git-ignored):
    third_party\mpv\           libmpv-2.dll, include\mpv\*.h, libmpv.lib (MSVC import library)
    third_party\tools\mpv.exe  external player ("Відкрити в mpv")
    third_party\tools\yt-dlp.exe  episode downloads
    third_party\wix\           WiX Toolset CLI (installer)

  libmpv/mpv come from shinchiro/mpv-winbuild-cmake (the builds mpv.io links to),
  yt-dlp from its official GitHub releases. Versions are pinned below; bump them
  deliberately. Needs the MSVC tools in PATH (run from build_windows.ps1, which
  enters the VS developer environment) to create libmpv.lib.

  Usage: .\scripts\fetch_deps.ps1 [-Force]
#>
param(
    [string]$MpvRelease = "20261111",
    [string]$YtDlpVersion = "latest",
    [string]$WixVersion = "6.0.2",
    [switch]$Force
)
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"   # Invoke-WebRequest is very slow with the progress bar

$Root = Split-Path -Parent $PSScriptRoot
$ThirdParty = Join-Path $Root "third_party"
$MpvDir = Join-Path $ThirdParty "mpv"
$ToolsDir = Join-Path $ThirdParty "tools"
$Downloads = Join-Path $ThirdParty "_downloads"
New-Item -ItemType Directory -Force $MpvDir, $ToolsDir, $Downloads | Out-Null

function Get-File([string]$Url, [string]$Dest) {
    if ((Test-Path $Dest) -and -not $Force) { return }
    Write-Host "  downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile "$Dest.part" -UseBasicParsing
    Move-Item -Force "$Dest.part" $Dest
}

function Expand-7z([string]$Archive, [string]$Dest) {
    # bsdtar in Windows 10 1803+ / 11 reads 7z archives
    New-Item -ItemType Directory -Force $Dest | Out-Null
    & "$env:SystemRoot\System32\tar.exe" -xf $Archive -C $Dest
    if ($LASTEXITCODE -ne 0) { throw "failed to unpack $Archive" }
}

# --- libmpv + mpv.exe -------------------------------------------------------
$needMpv = $Force -or -not (Test-Path (Join-Path $MpvDir "libmpv.lib")) -or -not (Test-Path (Join-Path $ToolsDir "mpv.exe"))
if ($needMpv) {
    Write-Host "==> mpv $MpvRelease"
    $release = Invoke-RestMethod -UseBasicParsing "https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/tags/$MpvRelease"
    # Baseline x86_64 build (the -v3 one needs AVX2)
    $dev = $release.assets | Where-Object { $_.name -match '^mpv-dev-x86_64-\d' } | Select-Object -First 1
    $app = $release.assets | Where-Object { $_.name -match '^mpv-x86_64-\d' } | Select-Object -First 1
    if (-not $dev -or -not $app) { throw "mpv assets not found in release $MpvRelease" }

    $devArchive = Join-Path $Downloads $dev.name
    $appArchive = Join-Path $Downloads $app.name
    Get-File $dev.browser_download_url $devArchive
    Get-File $app.browser_download_url $appArchive

    Remove-Item -Recurse -Force $MpvDir -ErrorAction SilentlyContinue
    Expand-7z $devArchive $MpvDir

    $appTmp = Join-Path $Downloads "mpv-app"
    Remove-Item -Recurse -Force $appTmp -ErrorAction SilentlyContinue
    Expand-7z $appArchive $appTmp
    Copy-Item -Force (Join-Path $appTmp "mpv.exe") $ToolsDir
    Remove-Item -Recurse -Force $appTmp

    # MSVC import library from the DLL's export table
    $dll = Join-Path $MpvDir "libmpv-2.dll"
    if (-not (Test-Path $dll)) { throw "libmpv-2.dll missing in $($dev.name)" }
    $def = Join-Path $MpvDir "libmpv.def"
    if (-not (Test-Path $def)) {
        $exports = & dumpbin /nologo /exports $dll | ForEach-Object {
            if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(mpv_\w+)') { $Matches[1] }
        }
        if (-not $exports) { throw "no mpv_* exports found (is dumpbin from MSVC in PATH?)" }
        @("LIBRARY libmpv-2.dll", "EXPORTS") + $exports | Set-Content -Encoding ascii $def
    }
    & lib /nologo /machine:x64 "/def:$def" "/out:$(Join-Path $MpvDir 'libmpv.lib')" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "lib.exe failed to create libmpv.lib" }
}

# --- yt-dlp ----------------------------------------------------------------
$ytdlp = Join-Path $ToolsDir "yt-dlp.exe"
if ($Force -or -not (Test-Path $ytdlp)) {
    Write-Host "==> yt-dlp $YtDlpVersion"
    $url = if ($YtDlpVersion -eq "latest") {
        "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"
    } else {
        "https://github.com/yt-dlp/yt-dlp/releases/download/$YtDlpVersion/yt-dlp.exe"
    }
    Get-File $url $ytdlp
}

# --- WiX Toolset (MSI + one-file bundle) -------------------------------------
# 6.x is MS-RL licensed; 7.x additionally requires accepting the OSMF EULA.
# The CLI MSI is unpacked with "msiexec /a" (administrative image): no install,
# no admin rights. Extensions go to the per-user WiX cache (%USERPROFILE%\.wix).
$wixExe = Join-Path $ThirdParty "wix\PFiles64\WiX Toolset v6.0\bin\wix.exe"
if ($Force -or -not (Test-Path $wixExe)) {
    Write-Host "==> WiX $WixVersion"
    $wixMsi = Join-Path $Downloads "wix-cli-x64-$WixVersion.msi"
    Get-File "https://github.com/wixtoolset/wix/releases/download/v$WixVersion/wix-cli-x64.msi" $wixMsi
    $wixDir = Join-Path $ThirdParty "wix"
    Remove-Item -Recurse -Force $wixDir -ErrorAction SilentlyContinue
    $p = Start-Process msiexec.exe -ArgumentList "/a `"$wixMsi`" /qn TARGETDIR=`"$wixDir`"" -Wait -PassThru
    if ($p.ExitCode -ne 0 -or -not (Test-Path $wixExe)) { throw "failed to unpack WiX ($($p.ExitCode))" }
}
$installed = & $wixExe extension list -g
foreach ($ext in "WixToolset.Util.wixext", "WixToolset.BootstrapperApplications.wixext") {
    if (-not ($installed -match "^$([regex]::Escape($ext)) $([regex]::Escape($WixVersion))")) {
        & $wixExe extension add -g "$ext/$WixVersion"
        if ($LASTEXITCODE -ne 0) { throw "wix extension add $ext failed" }
    }
}

Write-Host "==> third_party ready"
