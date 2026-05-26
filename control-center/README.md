# Blop Control Center

Kleine **lokale Web-Oberfläche** (FastAPI + statische Seite), um **GitHub Actions** zu **triggern** und **Run-Status** zu lesen — Basis für spätere „Auto-Fix“-Hooks (PR-Bot, Webhooks).

## Voraussetzungen

- Python 3.10+
- GitHub **Personal Access Token** mit mindestens: `repo`, `workflow` (Classic) bzw. fine-grained: Actions read/write + Contents read auf dem Ziel-Repo.

## Konfiguration

| Variable | Bedeutung |
|----------|-----------|
| `CONTROL_CENTER_REPOSITORY` | `owner/repo` (z. B. `BenSchwank/Blop-3.3.3`), falls nicht `GITHUB_REPOSITORY` gesetzt ist |
| `GITHUB_TOKEN` oder `CONTROL_CENTER_TOKEN` | PAT |
| `CONTROL_CENTER_CORS` | Optional, kommagetrennte Origins (Standard `*`) |
| `CONTROL_CENTER_HOST` / `CONTROL_CENTER_PORT` | Optional, Standard `127.0.0.1` / `8765` |

## Start

```bash
cd control-center
python -m venv .venv
.venv\Scripts\activate   # Windows
pip install -r requirements.txt
set CONTROL_CENTER_REPOSITORY=owner/Blop-3.3.3
set GITHUB_TOKEN=ghp_...
python smoke_local.py
python -m uvicorn app.main:app --reload --host 127.0.0.1 --port 8765
```

Browser: `http://127.0.0.1:8765/`

## API

- `GET /api/health` — Konfiguration ok?
- `GET /api/repo` — Ziel-Repository
- `GET /api/workflows` — Workflow-Liste
- `GET /api/runs?workflow=windows_build.yml` — letzte Runs (optional gefiltert)
- `POST /api/dispatch/windows_build.yml` — JSON `{"ref":"main"}` (branch anpassen)

**Hinweis:** Volle Windows-/Android-Builds sind teuer; zum Testen eignet sich `automation_smoke.yml` (schneller Leerlauf-Workflow).

`python smoke_local.py` nach `pip install` prüft PAT, Repo und die drei Workflows. Optional: `python smoke_local.py --dispatch-smoke` löst **`automation_smoke.yml`** aus (PAT mit `workflow`-Scope; Datei muss auf `main` liegen).

## Sicherheit

Token **nur** auf dem Rechner, der den Server startet — nicht in die Webseite einbetten und nicht ins Repo committen.
