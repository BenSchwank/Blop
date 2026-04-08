# Lernvideo & Podcast: Betrieb ohne teures Hosting

## Render Free / günstige Pläne

- **Idle / Cold Start:** Regelmäßiger Ping mit **UptimeRobot** auf `GET`/`HEAD` **`/api/health`** reduziert Einschlafen und träge erste Requests — Anleitung: [`UPTIMEROBOT.md`](UPTIMEROBOT.md).
- **Instanz-Neustarts** (Deploy, Speicher, Idle): Laufende Jobs im RAM gehen verloren. Mit der Tabelle `ai_background_jobs` (Migration unter `supabase/migrations/`) bleibt der **Status** in Supabase erhalten; Polling liefert weiterhin `pending`/`done`/`error`, sobald die Migration ausgeführt wurde.
- **Weniger Last / weniger RAM:** In der App weniger Szenen, Erzähltiefe „compact“, kürzeres Material.
- **ffmpeg / CPU:** Ken-Burns und Crossfade sind standardmäßig schon sparsam bzw. Crossfade aus. Optional per Umgebungsvariablen steuerbar (siehe [`main.py`](main.py) in `_normalize_learning_video_options`):
  - `LEARNING_VIDEO_KEN_BURNS=0` — keine Zoom/Pan-Animation
  - `LEARNING_VIDEO_XFADE=1` — weiche Übergänge (mehr CPU)

### 512MB Balanced Defaults (aktiv)

- Video-Encoding Defaults in [`media_pipeline.py`](media_pipeline.py):
  - `LEARNING_VIDEO_WIDTH=640`
  - `LEARNING_VIDEO_HEIGHT=360`
  - `LEARNING_VIDEO_FPS=10`
  - `LEARNING_VIDEO_CRF=32`
  - `LEARNING_VIDEO_MAXRATE=600k`
  - `LEARNING_VIDEO_BUFSIZE=1200k`
- Lernvideo-Defaults in [`main.py`](main.py):
  - `target_scenes` standardmäßig 5 (Clamp max. 12)
  - Stock-Bilder standardmäßig aus
  - Ken-Burns standardmäßig aus
  - Guardrail: bei vielen Szenen werden Effekte deaktiviert und Tiefe konservativer gesetzt

Wenn weiterhin OOM auftritt: zuerst Szenen reduzieren (4-5), Erzähltiefe „compact“, Stock-Bilder aus. Danach Lernvideo-Worker aus dem Webprozess auslagern.

## Gemini-API (503 / Überlast)

- Das Backend nutzt für das **Storyboard** standardmäßig **Flash zuerst** (weniger anfällig für Spitzenlast als Pro).
- Optional: `LEARNING_VIDEO_STORYBOARD_MODEL=gemini-2.5-pro` erzwingt eine andere erste Wahl.
- Storyboard-Requests haben **mehrere Retries** mit Backoff bei transienten Fehlern.

## Supabase: `ai_background_jobs`

Einmalig im SQL-Editor ausführen: [`supabase/migrations/20260402120000_ai_background_jobs.sql`](supabase/migrations/20260402120000_ai_background_jobs.sql).

Schlägt `save_background_job` in den Logs fehl: **Service-Role-Key** für das Backend prüfen oder RLS/Policies anpassen (Tabelle ist für serverseitigen Zugriff gedacht).
