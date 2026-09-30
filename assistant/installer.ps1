$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:PATH = "C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\6.11.2\mingw_64\bin;$env:PATH"

$builtExe = Join-Path $root "build\BlopAssistent.exe"
if (-not (Test-Path $builtExe)) {
    $buildScript = Join-Path $root "bauen.ps1"
    if (-not (Test-Path $buildScript)) {
        throw "BlopAssistent.exe fehlt und bauen.ps1 liegt nicht neben diesem Skript."
    }
    & $buildScript
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$deploy = Join-Path $root "deployment"
if (Test-Path $deploy) {
    Remove-Item $deploy -Recurse -Force
}
New-Item -ItemType Directory -Path $deploy | Out-Null
Copy-Item $builtExe (Join-Path $deploy "BlopAssistent.exe") -Force

& "C:\Qt\6.11.2\mingw_64\bin\windeployqt.exe" `
    --dir $deploy `
    (Join-Path $deploy "BlopAssistent.exe") `
    --release `
    --compiler-runtime `
    --no-translations
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not (Test-Path (Join-Path $deploy "platforms\qwindows.dll"))) {
    throw "Qt-Plattform qwindows.dll fehlt in deployment."
}

$nsisCandidates = @(
    "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
    "$env:ProgramFiles\NSIS\makensis.exe",
    (Join-Path $root "tools\nsis\makensis.exe")
)
$nsis = $nsisCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $nsis) {
    $cmd = Get-Command makensis -ErrorAction SilentlyContinue
    if ($cmd) { $nsis = $cmd.Source }
}
if (-not $nsis) {
    throw "makensis.exe nicht gefunden. NSIS 3 installieren."
}

Push-Location $root
try {
    & $nsis "installer.nsi"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}

$out = Join-Path $root "BlopAssistent_Windows_Installer.exe"
if (-not (Test-Path $out)) {
    throw "Installer wurde nicht erzeugt."
}
Write-Output "Fertig: $out"
