# Lernvideo & Podcast: Betrieb ohne teures Hosting

## Render Free / günstige Pläne

- **Idle / Cold Start:** Regelmäßiger Ping mit **UptimeRobot** auf `GET`/`HEAD` **`/api/health`** reduziert Einschlafen und träge erste Requests — Anleitung: [`UPTIMEROBOT.md`](UPTIMEROBOT.md).
- **Instanz-Neustarts** (Deploy, Speicher, Idle): Laufende Jobs im RAM gehen verloren. Mit der Tabelle `ai_background_jobs` (Migration unter `supabase/migrations/`) bleibt der **Status** in Supabase erhalten; Polling liefert weiterhin `pending`/`done`/`error`, sobald die Migration ausgeführt wurde.
- **Weniger Last / weniger RAM:** In der App weniger Szenen, Erzähltiefe „compact“, kürzeres Material.
- **ffmpeg / CPU:** Ken-Burns und Crossfade sind standardmäßig schon sparsam bzw. Crossfade aus. Optional per Umgebungsvariablen steuerbar (siehe [`main.py`](main.py) in `_normalize_learning_video_options`):
  - `LEARNING_VIDEO_KEN_BURNS=0` — keine Zoom/Pan-Animation
  - `LEARNING_VIDEO_XFADE=1` — weiche Übergänge (mehr CPU)

## Gemini-API (503 / Überlast)

- Das Backend nutzt für das **Storyboard** standardmäßig **Flash zuerst** (weniger anfällig für Spitzenlast als Pro).
- Optional: `LEARNING_VIDEO_STORYBOARD_MODEL=gemini-2.5-pro` erzwingt eine andere erste Wahl.
- Storyboard-Requests haben **mehrere Retries** mit Backoff bei transienten Fehlern.

## Supabase: `ai_background_jobs`

Einmalig im SQL-Editor ausführen: [`supabase/migrations/20260402120000_ai_background_jobs.sql`](supabase/migrations/20260402120000_ai_background_jobs.sql).

Schlägt `save_background_job` in den Logs fehl: **Service-Role-Key** für das Backend prüfen oder RLS/Policies anpassen (Tabelle ist für serverseitigen Zugriff gedacht).
