# Speichert das Desktop-OAuth-Clientgeheimnis lokal (nicht in Git),
# damit Blop den Kalender-Token-Tausch machen kann.
#
# 1) Google Cloud Console → Anmeldedaten → „blop notes“ (Desktop)
#    → Download-Pfeil (JSON) speichern
# 2) Dieses Skript ausführen:
#      ./scripts/setup-google-desktop-secret.ps1 -JsonPath "$env:USERPROFILE\Downloads\client_secret_*.json"
#
# Danach Blop neu starten (Release/Debug). Optional zusätzlich in Qt Creator
# unter Ausführen → Umgebung: BLOP_GOOGLE_CLIENT_SECRET=<wert>

param(
  [Parameter(Mandatory = $true)]
  [string]$JsonPath
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $JsonPath)) {
  throw "JSON nicht gefunden: $JsonPath"
}

$json = Get-Content -LiteralPath $JsonPath -Raw | ConvertFrom-Json
# Google liefert entweder {installed:{...}} oder {web:{...}}
$node = $null
if ($json.installed) { $node = $json.installed }
elseif ($json.web) { $node = $json.web }
else { $node = $json }

$secret = [string]$node.client_secret
$clientId = [string]$node.client_id
if ([string]::IsNullOrWhiteSpace($secret)) {
  throw "In der JSON-Datei steht kein client_secret. Bitte die Desktop-Client-JSON von „blop notes“ laden."
}

$destDir = Join-Path $env:APPDATA "Blop\BlopApp"
New-Item -ItemType Directory -Force -Path $destDir | Out-Null
$dest = Join-Path $destDir "google_desktop_client_secret.txt"
Set-Content -LiteralPath $dest -Value $secret.Trim() -NoNewline -Encoding ascii

Write-Host "OK  Secret gespeichert unter:"
Write-Host "    $dest"
if ($clientId) {
  Write-Host "    client_id in JSON: $clientId"
  Write-Host "    Erwartet in Blop:  571766217-omvcb33l9m0kr1bjk9ecdik6gcljpkf6.apps.googleusercontent.com"
}
Write-Host ""
Write-Host "Nächste Schritte:"
Write-Host "  1) Blop beenden und neu starten"
Write-Host "  2) Dashboard → Google verbinden (Kalender)"
Write-Host "  3) Für Produktion auf Render: GOOGLE_CLIENT_SECRET / GOOGLE_DESKTOP_CLIENT_SECRET setzen + Backend deployen"
