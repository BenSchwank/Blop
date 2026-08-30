<#
.SYNOPSIS
  Lokaler Blop-Workflow (Windows / MinGW): bauen, starten, Speicherstand taggen.

.DESCRIPTION
  Ein Einstiegspunkt fuer die lokale Schleife "Agent aendert Code -> ich teste
  in Qt -> Speicherstand als Tag". Ersetzt das manuelle Zusammenkopieren der
  PATH- und CMake-Flags aus AGENTS.md.

  Qt WebEngine/Pdf gibt es mit MinGW auf Windows nicht, darum immer
  -DBLOP_DESKTOP_WEBENGINE=OFF (Smoke-Build des Kerns).

.EXAMPLE
  ./scripts/blop-local.ps1 build
  ./scripts/blop-local.ps1 run
  ./scripts/blop-local.ps1 br              # build + run
  ./scripts/blop-local.ps1 br -Trace       # Session-Trace fuer Agent-QA
  ./scripts/blop-local.ps1 tag -Message "J-Rail 3-Punkt Griff ok"
  ./scripts/blop-local.ps1 tag -Message "..." -Push
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0)]
  [ValidateSet('build', 'run', 'br', 'tag', 'configure', 'status')]
  [string]$Task = 'br',

  # Commit-/Tag-Beschreibung fuer den Speicherstand.
  [string]$Message,

  # Eigene Version erzwingen, z. B. "v3.28.1". Sonst Patch-Bump.
  [string]$Version,

  # Tag (und Commit) nach origin schieben.
  [switch]$Push,

  # Kompletten Neuaufbau: CMake-Cache verwerfen.
  [switch]$Fresh,

  # Opt-in Session-Trace (BLOP_SESSION_TRACE=1). Standard aus — kein Monitoring.
  [switch]$Trace
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RepoRoot 'build-check'
$Exe      = Join-Path $BuildDir 'Blop.exe'

function Write-Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host "  ok  $msg" -ForegroundColor Green }
function Write-Warn2($msg){ Write-Host "  !!  $msg" -ForegroundColor Yellow }

# --- Qt-Kit finden -------------------------------------------------------
# Nicht auf eine Version festnageln: AGENTS.md nannte 6.10.2, installiert ist
# inzwischen 6.11.2. Wir nehmen das hoechste vorhandene mingw_64-Kit.
function Resolve-QtKit {
  $qtRoot = 'C:\Qt'
  if (-not (Test-Path $qtRoot)) {
    throw "Qt nicht gefunden unter $qtRoot. Bitte Qt (MinGW) installieren."
  }
  $kit = Get-ChildItem $qtRoot -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
    Where-Object { Test-Path (Join-Path $_.FullName 'mingw_64\bin\qmake.exe') } |
    Sort-Object { [version]$_.Name } -Descending |
    Select-Object -First 1
  if (-not $kit) { throw "Kein Qt mingw_64-Kit unter $qtRoot gefunden." }
  return (Join-Path $kit.FullName 'mingw_64')
}

function Initialize-BuildEnv {
  $script:QtDir = Resolve-QtKit
  $mingw = 'C:\Qt\Tools\mingw1310_64\bin'
  $ninja = 'C:\Qt\Tools\Ninja'
  $cmake = 'C:\Qt\Tools\CMake_64\bin'
  foreach ($p in @($ninja, $mingw, $cmake)) {
    if (-not (Test-Path $p)) { throw "Fehlt: $p (Qt Tools nachinstallieren)" }
  }
  $env:PATH = "$ninja;$mingw;$cmake;$(Join-Path $script:QtDir 'bin');$env:PATH"
  Write-Ok "Qt-Kit: $script:QtDir"
}

function Invoke-Configure {
  if ($Fresh -and (Test-Path $BuildDir)) {
    Write-Step "Fresh: entferne $BuildDir"
    Remove-Item $BuildDir -Recurse -Force
  }
  if (Test-Path (Join-Path $BuildDir 'CMakeCache.txt')) {
    # Cache wiederverwenden, aber pruefen, ob er noch aufs aktuelle Kit zeigt.
    $cache = Get-Content (Join-Path $BuildDir 'CMakeCache.txt') -Raw
    $qtFwd = $script:QtDir.Replace('\', '/')
    if ($cache -match [regex]::Escape($qtFwd)) { return }
    Write-Warn2 'CMake-Cache zeigt auf ein anderes Qt-Kit -> neu konfigurieren.'
    Remove-Item $BuildDir -Recurse -Force
  }
  Write-Step 'CMake konfigurieren (Ninja, Release, WebEngine OFF)'
  & cmake -S $RepoRoot -B $BuildDir -G Ninja `
      -DCMAKE_BUILD_TYPE=Release `
      -DBLOP_DESKTOP_WEBENGINE=OFF `
      -DCMAKE_PREFIX_PATH="$($script:QtDir.Replace('\','/'))" `
      -DCMAKE_C_COMPILER='C:/Qt/Tools/mingw1310_64/bin/gcc.exe' `
      -DCMAKE_CXX_COMPILER='C:/Qt/Tools/mingw1310_64/bin/g++.exe'
  if ($LASTEXITCODE -ne 0) { throw 'CMake-Konfiguration fehlgeschlagen.' }
}

function Invoke-Build {
  Initialize-BuildEnv
  Invoke-Configure
  Write-Step 'Blop bauen (inkrementell)'
  $jobs = $env:NUMBER_OF_PROCESSORS
  & cmake --build $BuildDir --target Blop -j $jobs
  if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen.' }
  Write-Ok "Binary: $Exe"

  # QA console launcher (optional target; may need reconfigure after first add).
  if (Test-Path (Join-Path $BuildDir 'build.ninja')) {
    $traceTarget = & cmake --build $BuildDir --target help 2>$null |
      Select-String -SimpleMatch 'BlopTrace'
    if ($traceTarget) {
      Write-Step 'BlopTrace bauen'
      & cmake --build $BuildDir --target BlopTrace -j $jobs
      if ($LASTEXITCODE -ne 0) { Write-Warn2 'BlopTrace-Build fehlgeschlagen (Blop ok).' }
      else { Write-Ok "BlopTrace: $(Join-Path $BuildDir 'BlopTrace.exe')" }
    }
  }

  # Side-by-side Qt DLLs so blop:// / Explorer launches work without PATH
  # (and without a console .cmd wrapper that floods terminals).
  Write-Step 'Qt-DLLs neben Blop.exe legen'
  $qtBin = Join-Path $script:QtDir 'bin'
  $needed = @(
    'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll',
    'Qt6Svg.dll', 'Qt6OpenGL.dll', 'Qt6OpenGLWidgets.dll',
    'Qt6Multimedia.dll', 'Qt6MultimediaWidgets.dll',
    'Qt6Qml.dll', 'Qt6QmlMeta.dll', 'Qt6QmlModels.dll', 'Qt6QmlWorkerScript.dll',
    'Qt6Quick.dll', 'Qt6QuickWidgets.dll', 'Qt6QuickControls2.dll',
    'Qt6QuickControls2Basic.dll', 'Qt6QuickControls2Impl.dll',
    'Qt6QuickTemplates2.dll', 'Qt6NetworkAuth.dll',
    'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'
  )
  foreach ($name in $needed) {
    $src = Join-Path $qtBin $name
    if (Test-Path $src) {
      Copy-Item -Force $src (Join-Path $BuildDir $name)
    }
  }
  $platSrc = Join-Path $script:QtDir 'plugins\platforms\qwindows.dll'
  $platDst = Join-Path $BuildDir 'platforms'
  if (Test-Path $platSrc) {
    New-Item -ItemType Directory -Force -Path $platDst | Out-Null
    Copy-Item -Force $platSrc (Join-Path $platDst 'qwindows.dll')
  }
  # HTTPS (Google claim/poll) needs Qt TLS plugins under <app>/tls/
  $tlsSrcDir = Join-Path $script:QtDir 'plugins\tls'
  $tlsDst = Join-Path $BuildDir 'tls'
  if (Test-Path $tlsSrcDir) {
    New-Item -ItemType Directory -Force -Path $tlsDst | Out-Null
    foreach ($name in @('qschannelbackend.dll', 'qopensslbackend.dll', 'qcertonlybackend.dll')) {
      $src = Join-Path $tlsSrcDir $name
      if (Test-Path $src) { Copy-Item -Force $src (Join-Path $tlsDst $name) }
    }
  }
  # Drop legacy console launcher that caused terminal spam.
  $legacyCmd = Join-Path $BuildDir 'blop-open.cmd'
  if (Test-Path $legacyCmd) { Remove-Item -Force $legacyCmd }
  Write-Ok 'Qt runtime neben Binary (inkl. tls/)'
}

function Invoke-Run {
  if (-not (Test-Path $Exe)) { throw "Kein Build vorhanden. Erst: blop-local.ps1 build" }
  Initialize-BuildEnv
  if ($Trace) {
    $env:BLOP_SESSION_TRACE = '1'
    Write-Step 'Session-Trace AN (BLOP_SESSION_TRACE=1) — Log unter AppData/…/session_trace.log'
  } else {
    # Do not clear a user-set env from the parent shell; only leave unset if we own it.
    if ($env:BLOP_SESSION_TRACE -eq '1') {
      Write-Warn2 'BLOP_SESSION_TRACE=1 ist in der Shell gesetzt (Trace bleibt an).'
    }
  }
  Write-Step 'Blop starten (lokal testen)'
  & $Exe
  Write-Ok "Blop beendet (Exit $LASTEXITCODE)"
}

# --- Speicherstand ------------------------------------------------------
function Get-NextVersion {
  $last = (& git -C $RepoRoot tag --sort=-v:refname |
           Where-Object { $_ -match '^v\d+\.\d+\.\d+$' } |
           Select-Object -First 1)
  if (-not $last) { return 'v3.28.1' }
  if ($last -notmatch '^v(\d+)\.(\d+)\.(\d+)$') { return 'v3.28.1' }
  return ('v{0}.{1}.{2}' -f $Matches[1], $Matches[2], ([int]$Matches[3] + 1))
}

function Invoke-Tag {
  if (-not $Message) { throw 'Bitte -Message "was wurde erreicht" angeben.' }

  # Speicherstand nur aus einem gebauten, lauffaehigen Zustand.
  if (-not (Test-Path $Exe)) { Write-Warn2 'Noch kein Build -- baue zuerst.'; Invoke-Build }

  $ver = if ($Version) { $Version } else { Get-NextVersion }
  Write-Step "Speicherstand $ver"

  # Nur getrackte Aenderungen: kein versehentliches Einchecken von
  # oracleJdk-26/ o. ae. grossen, ungetrackten Ordnern.
  $dirty = & git -C $RepoRoot status --porcelain --untracked-files=no
  if ($dirty) {
    & git -C $RepoRoot add -u
    & git -C $RepoRoot commit -m $Message
    if ($LASTEXITCODE -ne 0) { throw 'Commit fehlgeschlagen.' }
    Write-Ok 'Aenderungen committet'
  } else {
    Write-Warn2 'Keine Aenderungen -- tagge den aktuellen HEAD.'
  }

  & git -C $RepoRoot tag -a $ver -m $Message
  if ($LASTEXITCODE -ne 0) { throw "Tag $ver konnte nicht erstellt werden (existiert er schon?)." }
  Write-Ok "Tag $ver erstellt"

  if ($Push) {
    $branch = (& git -C $RepoRoot rev-parse --abbrev-ref HEAD).Trim()
    & git -C $RepoRoot push origin $branch
    & git -C $RepoRoot push origin $ver
    Write-Ok "Gepusht: $branch + $ver"
  } else {
    Write-Host "  (lokal. Mit -Push nach origin schieben.)" -ForegroundColor DarkGray
  }
}

function Invoke-Status {
  Write-Step 'Blop-Status'
  $branch = (& git -C $RepoRoot rev-parse --abbrev-ref HEAD).Trim()
  Write-Host "  Branch:  $branch"
  Write-Host "  Version: $((& git -C $RepoRoot describe --tags).Trim())"
  $exeInfo = if (Test-Path $Exe) { (Get-Item $Exe).LastWriteTime } else { 'nicht gebaut' }
  Write-Host "  Build:   $exeInfo"
  $dirty = & git -C $RepoRoot status --porcelain --untracked-files=no
  Write-Host "  Dirty:   $(if ($dirty) { "$(($dirty | Measure-Object).Count) Datei(en)" } else { 'sauber' })"
}

switch ($Task) {
  'configure' { Initialize-BuildEnv; Invoke-Configure }
  'build'     { Invoke-Build }
  'run'       { Invoke-Run }
  'br'        { Invoke-Build; Invoke-Run }
  'tag'       { Invoke-Tag }
  'status'    { Invoke-Status }
}
