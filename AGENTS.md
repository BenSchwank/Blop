# AGENTS.md

## Cursor Cloud specific instructions

Blop is a **Qt 6 / C++17** desktop (+ Android) note app. Cloud Agents should treat the Ubuntu VM + Desktop tab as the primary verify loop for UI work.

### One-time / environment

1. Click **Set Up Cloud Agents** (or use this repo’s `.cursor/environment.json`).
2. After Qt/CMake is installed and `build-check/Blop` links, **save a Snapshot** in the Cloud Agents dashboard so later runs skip apt installs.
3. Secrets (Sentry DSN, GitHub PAT for control-center, etc.) belong in the dashboard Secrets tab — not in git.

### Build & run

```bash
# Idempotent refresh (also the environment `install` script)
bash scripts/cloud-agent-install.sh

# Fast incremental rebuild after edits
cmake --build build-check --target Blop -j"$(nproc)"

# Launch for GUI verification (use the Desktop tab / Computer Use)
./build-check/Blop
```

- Preferred build dir: **`build-check/`** (Ninja). Do not wipe it unless CMake cache is broken.
- Version string comes from `git describe --tags`; bump tags when shipping testable desktop builds (`v3.22.x`).

### UI / note-editor conventions

- Desktop note chrome uses **NoteChrome** colors (charcoal + blue accent `#5B9DFF`), not Blop purple, while a note is open.
- Favorites rail = `ModernToolbar` Drawboard vertical mode; do not break `AndroidPhoneToolbar` / MorphTray Android paths.
- Sticky notes persist in `NotePage::stickies` via `NoteManager` JSON — keep harvest/hydrate in `MultiPageNoteView` in sync when changing sticky graphics.

### Git / PR habits

- Work on the current feature branch; commit and push often.
- After a stable desktop smoke: tag `v3.22.N` and FF-sync `master`/`main` when that is the project convention.
- Prefer small, screenshot-verifiable tasks over broad “make it prettier” prompts.

### Useful local tools in-repo

- `control-center/` — local FastAPI UI to dispatch GitHub Actions (`127.0.0.1:8765`).
- `docs/agentic-automation.md` — optional benchmarks / CI dispatch notes.
