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

### Windows local workflow (preferred for UI work)

Qt WebEngine/Pdf are **not available with MinGW on Windows**, so a full release
build needs MSVC or the Linux Cloud Agent. For local UI iteration use the
wrapper script — it picks the highest installed `C:\Qt\<ver>\mingw_64` kit
automatically (currently **6.11.2**) and sets all flags:

```powershell
./scripts/blop-local.ps1 status                      # branch, version, build age
./scripts/blop-local.ps1 build                       # incremental
./scripts/blop-local.ps1 br                          # build + launch for manual test
./scripts/blop-local.ps1 tag -Message "was klappt"   # Speicherstand (commit + tag)
./scripts/blop-local.ps1 tag -Message "..." -Push    # und nach origin schieben
./scripts/blop-local.ps1 build -Fresh                # CMake-Cache verwerfen
```

The loop for UI changes is: agent edits → `br` → user verifies in the running
app → `tag` to lock the save point. Tagging refuses to run without a build and
only stages **tracked** files (`git add -u`), so large local folders such as
`oracleJdk-26/` can never sneak into a commit.

Raw commands, if the script is unavailable:

```powershell
$env:PATH = "C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\6.11.2\mingw_64\bin;$env:PATH"

cmake -S . -B build-check -G Ninja `
 -DCMAKE_BUILD_TYPE=Release `
 -DBLOP_DESKTOP_WEBENGINE=OFF `
 -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64" `
 -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" `
 -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"

cmake --build build-check --target Blop -j $env:NUMBER_OF_PROCESSORS
```

### UI / note-editor conventions

- Desktop note chrome uses **NoteChrome** colors (charcoal + blue accent `#5B9DFF`), not Blop purple, while a note is open.
- Theme defaults (Light + Blue) live in `BlopTheme::install()` as **QSettings fallbacks**. Never call `setMode()`/`setAccent()` at startup to force a look — that overwrites and persists over the user's own choice on every launch.
- Favorites rail = `ModernToolbar` Drawboard vertical mode; do not break `AndroidPhoneToolbar` / MorphTray Android paths.
- Sticky notes persist in `NotePage::stickies` via `NoteManager` JSON — keep harvest/hydrate in `MultiPageNoteView` in sync when changing sticky graphics.

### Git / PR habits

- Work on the current feature branch; commit and push often.
- After a stable desktop smoke: tag `v3.22.N` and FF-sync `master`/`main` when that is the project convention.
- Prefer small, screenshot-verifiable tasks over broad “make it prettier” prompts.

### Useful local tools in-repo

- `control-center/` — local FastAPI UI to dispatch GitHub Actions (`127.0.0.1:8765`).
- `docs/agentic-automation.md` — optional benchmarks / CI dispatch notes.
