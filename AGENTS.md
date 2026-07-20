# AGENTS.md

## Cursor Cloud specific instructions

This is a multi-product monorepo. The primary product that can be run and tested end-to-end
in the cloud VM is **Blop Study** (web): a Next.js frontend (`StudyFlow/frontend`) that proxies
`/api/*` to a FastAPI backend (`StudyFlow/backend`), backed by Supabase (Postgres + Storage).
Other products are secondary here: the native **Blop** Qt/C++ desktop+Android app (root, CMake —
GUI/build only, not run headless), the **Marketing Hub** Next.js app (`StudyFlow/marketing-hub`),
the **Control Center** FastAPI app (`control-center`), and a legacy Streamlit prototype
(`StudyFlow/app.py`).

### What the update script already does
On VM startup the update script installs deps: `npm install` at the repo root, `npm install` in
`StudyFlow/frontend`, and the backend Python deps into `/workspace/.venv`. You normally do not
need to reinstall anything.

### Non-obvious setup facts (read before running services)
- **Root `npm install` is required, not optional.** `StudyFlow/frontend` imports
  `@tiptap/extension-image`, but that dependency is declared only in the **root** `package.json`.
  Next.js (Turbopack) picks `/workspace` as the workspace root and resolves it from
  `/workspace/node_modules`. If you only install in `StudyFlow/frontend`, the folder/editor page
  fails with `Module not found: @tiptap/extension-image`.
- **Backend Python deps live in `/workspace/.venv`.** Activate with
  `source /workspace/.venv/bin/activate` before running the backend. (`python3.12-venv` is already
  installed at the system level in the snapshot.)
- **Docker is installed but does NOT auto-start** (no systemd in the VM). Start it manually with
  `sudo dockerd` (run it in a tmux/background session). It is configured for docker-in-docker via
  `fuse-overlayfs` with the containerd snapshotter disabled (`/etc/docker/daemon.json`). After
  starting, `sudo chmod 666 /var/run/docker.sock` lets the `ubuntu` user use docker without sudo.

### Local Supabase (provides the DB the backend needs)
The backend needs Supabase. A local stack is run via the **Supabase CLI** (installed in the
snapshot) from `~/dev-supabase` (a scratch dir outside the repo, created with `supabase init`).
- Start it (needs dockerd running): `cd ~/dev-supabase && supabase start`. It listens on
  `http://127.0.0.1:54321` (API), `54322` (Postgres), `54323` (Studio).
- The Blop schema is NOT auto-applied by `supabase start`. The committed repo schema
  (`StudyFlow/backend/supabase_schema.sql`) is out of date vs. the code (the `users` table also
  needs `email`, `auth_id`, `preferred_model`, `subscription_tier`). A complete, code-matching
  schema is kept at `~/dev-supabase/blop_schema.sql`; apply it with:
  `docker exec -i supabase_db_dev-supabase psql -U postgres -d postgres < ~/dev-supabase/blop_schema.sql`
- The backend reads Supabase creds from `StudyFlow/backend/.env` (gitignored). For local dev it is
  pointed at `http://127.0.0.1:54321` using the Supabase **service_role** key for `SUPABASE_KEY`
  (bypasses RLS; simplest for local). Get the current local keys with
  `cd ~/dev-supabase && supabase status`. If the keys ever change, update `.env` accordingly.

### Running the app (dev mode)
- Backend: `cd StudyFlow/backend && source /workspace/.venv/bin/activate && python main.py`
  → serves on `:8000`. Health: `GET /api/health`; DB check: `GET /api/health/db`.
- Frontend: `cd StudyFlow/frontend && BACKEND_URL=http://localhost:8000 npm run dev`
  → serves on `:3000`. **You must set `BACKEND_URL`**, otherwise `next.config.ts` proxies `/api/*`
  to the production Render backend (`https://blop-study-backend.onrender.com`) instead of local.

### Lint / test / build
- Frontend lint: `cd StudyFlow/frontend && npm run lint` (runs eslint; the repo currently has
  pre-existing lint errors/warnings — that's expected, not something you introduced).
- Frontend build: `cd StudyFlow/frontend && BACKEND_URL=http://localhost:8000 npm run build`.
- There is no real automated test suite (only `StudyFlow/test_mime.py`, a trivial dev script).

### AI / media features are optional
Account/folder/file (note) CRUD works with just Supabase. AI generation (summaries, flashcards,
chat, learning videos, podcasts) additionally needs `GOOGLE_API_KEY` (Gemini) in the backend
`.env`; podcasts also need `OPENAI_API_KEY`. `ffmpeg` is installed for the video/podcast pipeline.

### Known pre-existing app bug (not an environment problem)
Creating a brand-new document via the folder page's "Neues Dokument" button opens a blank editor
and "Speichern" fails with "Fehler beim Speichern der Datei". This is a bug in the app code
(a missing `setIsEditingFile(true)` on the new-doc handler and a missing `immediatelyRender: false`
on the TipTap editor), unrelated to environment setup. Account creation, login, and folder
creation (which persist to Supabase) all work correctly.
