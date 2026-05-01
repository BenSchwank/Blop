<# 
  Applies share table migration + PostgREST reload using psql.

  Requires:
  - psql on PATH (PostgreSQL client)
  - DATABASE_URL (recommended) OR SUPABASE_DB_URL set to the Supabase Postgres connection string
    (Supabase Dashboard → Project Settings → Database → Connection string → URI)

  Usage (PowerShell):
    $env:DATABASE_URL = "postgresql://..."
    .\scripts\apply_share_tables.ps1
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$dbUrl = $env:DATABASE_URL
if (-not $dbUrl) { $dbUrl = $env:SUPABASE_DB_URL }
if (-not $dbUrl) {
    throw "Set DATABASE_URL (or SUPABASE_DB_URL) to your Supabase Postgres connection URI before running."
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$backendRoot = Split-Path -Parent $here
$sqlPath = Join-Path $backendRoot "migrations\002_create_share_tables.sql"
if (-not (Test-Path -LiteralPath $sqlPath)) {
    throw "Missing SQL file at: $sqlPath"
}

Write-Host "Applying: $sqlPath"
& psql $dbUrl -v ON_ERROR_STOP=1 -f $sqlPath

Write-Host "Reloading PostgREST schema cache..."
& psql $dbUrl -v ON_ERROR_STOP=1 -c "notify pgrst, 'reload schema';"

Write-Host "Done."
