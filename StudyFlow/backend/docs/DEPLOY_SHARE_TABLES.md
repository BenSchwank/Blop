# Share Tabellen auf Supabase deployen (`share_requests`, `share_links`)

Wenn du in Blop Study Fehler siehst wie:

```text
PGRST205 ... Could not find the table 'public.share_requests' ...
PGRST205 ... Could not find the table 'public.share_links' ...
```

dann sind die Tabellen auf der jeweiligen Supabase-Instanz (Production **und** Preview) noch nicht angelegt.

## 1) Schnell-Hotfix: SQL Editor (je Projekt einmal)

1. Supabase Dashboard → **SQL Editor** → New query  
2. Inhalt dieser Datei **vollständig** einfügen und ausführen:
   - `StudyFlow/backend/migrations/002_create_share_tables.sql`  
   (identisch zu `StudyFlow/backend/supabase/migrations/20260501130000_share_requests_and_links.sql`)

3. **PostgREST Schema Cache neu laden** (wichtig, sonst kann PGRST205 noch kurz bestehen):

```sql
notify pgrst, 'reload schema';
```

Wiederhole Schritte 1–3 für **Preview** und **Production**.

### Alternative (lokal/automatisiert): `psql` + Repo-Script (Windows)

Wenn du eine Postgres URI hast (Dashboard → Database → Connection string → URI):

```powershell
cd StudyFlow\backend
$env:DATABASE_URL = "postgresql://..."
.\scripts\apply_share_tables.ps1
```

Das Script führt die Migration aus und sendet anschließend `NOTIFY pgrst`.

### Kurzcheck in SQL

```sql
select to_regclass('public.share_requests') as share_requests,
       to_regclass('public.share_links')    as share_links;
```

Beide sollten nicht `NULL` sein.

## 2) Dauerhaft: Supabase CLI (empfohlen)

Voraussetzung: `supabase` CLI installiert und mit beiden Projekten verknüpft.

- Projekt-Verzeichnis: `StudyFlow/backend/` (enthält auch `supabase/migrations`)

Typischer Ablauf (Beispiele – `ref` durch deinen Project Reference ersetzen):

```bash
cd StudyFlow/backend
supabase link --project-ref YOUR_PREVIEW_REF
supabase db push
supabase link --project-ref YOUR_PROD_REF
supabase db push
```

Hinweis: Wenn du die Tabellen bereits per **SQL Editor** angelegt hast, kann Supabase später beim `db push` meckern, dass die Migration „schon angewendet“ wirkt — dann Migration-History pflegen/zurücksetzen oder die Migration dort als „repaired“ markieren (`supabase migration repair …`, je nach Setup).

Alternativ ohne `supabase link` (nur Connection URI):

```bash
cd StudyFlow/backend
supabase db push --db-url "postgresql://..."
```

Die Migration liegt unter:

- `StudyFlow/backend/supabase/migrations/20260501130000_share_requests_and_links.sql`

## 3) App-Verifikation (nach Deploy)

Mit eingeloggtem Nutzer auf einer Ordner-Seite (`/folder/...`) testen:

- Datei-Menü → **Teilen** → Username-Share (keine PGRST205)
- **Teilen** → Share-Link erstellen (keine PGRST205)
- Kopf-Menü (**…**) → **Requests** (lädt ohne PGRST205; Liste kann leer sein)

Backend-Endpunkte (FastAPI):

- `POST /api/shares/username`
- `POST /api/shares/link`
- `GET /api/shares/requests`
- `POST /api/shares/link/{token}/import`

### Backend-Schnelltest (ohne echte Nutzer/Dateien)

1. In Render (oder lokal) setzen:
   - `BLOP_SHARE_TABLE_DIAG_TOKEN` = ein langes zufälliges Secret

2. Nach Deploy:

```bash
curl "https://DEIN-BACKEND/api/health/share-tables?token=DEIN_TOKEN"
```

Erwartung: HTTP 200 + `ok: true`.

Zusätzlich (funktional, trifft PostgREST direkt):

```bash
curl "https://DEIN-BACKEND/api/shares/requests?username=doesnotexist12345"
```

Erwartung: **kein** `PGRST205` mehr (üblicherweise `{ "items": [] }` statt Crash).
