"""
Local control center for Blop automation: trigger GitHub Actions workflows and read run status.
Requires a personal access token with `repo` + `workflow` (or fine-grained equivalent).
"""

from __future__ import annotations

import os
from typing import Any, Optional

import httpx
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

GITHUB_API = "https://api.github.com"


def _repo_full() -> str:
    r = os.environ.get("GITHUB_REPOSITORY") or os.environ.get("CONTROL_CENTER_REPOSITORY")
    if not r or "/" not in r:
        raise RuntimeError(
            "Set CONTROL_CENTER_REPOSITORY=owner/repo (e.g. MyUser/Blop-3.3.3) "
            "or run with GITHUB_REPOSITORY set."
        )
    return r


def _token() -> str:
    t = os.environ.get("GITHUB_TOKEN") or os.environ.get("CONTROL_CENTER_TOKEN")
    if not t:
        raise RuntimeError("Set GITHUB_TOKEN or CONTROL_CENTER_TOKEN to a PAT with workflow scope.")
    return t


def _headers() -> dict[str, str]:
    return {
        "Authorization": f"Bearer {_token()}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
    }


app = FastAPI(title="Blop Control Center", version="0.1.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=os.environ.get("CONTROL_CENTER_CORS", "*").split(","),
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_static = os.path.join(_root, "static")


class DispatchBody(BaseModel):
    ref: str = "main"
    inputs: Optional[dict[str, Any]] = None


@app.get("/api/health")
def health() -> dict[str, str]:
    try:
        _repo_full()
        _token()
        return {"status": "ok"}
    except RuntimeError as e:
        return {"status": "error", "detail": str(e)}


@app.get("/api/repo")
def repo_info() -> dict[str, str]:
    full = _repo_full()
    owner, name = full.split("/", 1)
    return {"full_name": full, "owner": owner, "name": name}


@app.get("/api/workflows")
async def list_workflows() -> dict[str, Any]:
    owner, repo = _repo_full().split("/", 1)
    async with httpx.AsyncClient(timeout=60) as client:
        r = await client.get(
            f"{GITHUB_API}/repos/{owner}/{repo}/actions/workflows",
            headers=_headers(),
        )
    if r.status_code != 200:
        raise HTTPException(r.status_code, r.text)
    data = r.json()
    out = []
    for w in data.get("workflows", []):
        out.append(
            {
                "id": w["id"],
                "name": w["name"],
                "path": w["path"],
                "state": w["state"],
            }
        )
    return {"workflows": out}


@app.get("/api/runs")
async def list_runs(
    workflow: Optional[str] = None,
    per_page: int = 10,
) -> dict[str, Any]:
    """Recent workflow runs. Optional `workflow` = filename e.g. windows_build.yml"""
    owner, repo = _repo_full().split("/", 1)
    params: dict[str, Any] = {"per_page": min(per_page, 30)}
    path = f"/repos/{owner}/{repo}/actions/runs"
    if workflow:
        wid = await _resolve_workflow_id(owner, repo, workflow)
        if not wid:
            raise HTTPException(404, f"Workflow not found: {workflow}")
        path = f"/repos/{owner}/{repo}/actions/workflows/{wid}/runs"
    async with httpx.AsyncClient(timeout=60) as client:
        r = await client.get(f"{GITHUB_API}{path}", headers=_headers(), params=params)
    if r.status_code != 200:
        raise HTTPException(r.status_code, r.text)
    return r.json()


async def _resolve_workflow_id(owner: str, repo: str, filename: str) -> Optional[int]:
    """Resolve numeric id; GitHub also accepts filename for dispatch but list runs needs id."""
    if filename.isdigit():
        return int(filename)
    async with httpx.AsyncClient(timeout=60) as client:
        r = await client.get(
            f"{GITHUB_API}/repos/{owner}/{repo}/actions/workflows",
            headers=_headers(),
        )
    if r.status_code != 200:
        return None
    base = filename.replace("\\", "/")
    if "/" not in base:
        base = f".github/workflows/{base}"
    for w in r.json().get("workflows", []):
        if w.get("path", "").replace("\\", "/").endswith(base.split("/")[-1]):
            return w["id"]
        if w.get("path", "") == base:
            return w["id"]
    return None


@app.post("/api/dispatch/{workflow}")
async def dispatch_workflow(workflow: str, body: DispatchBody) -> dict[str, Any]:
    """
    Trigger workflow_dispatch. `workflow` = file name, e.g. windows_build.yml or automation_smoke.yml
    """
    owner, repo = _repo_full().split("/", 1)
    payload: dict[str, Any] = {"ref": body.ref}
    if body.inputs:
        payload["inputs"] = body.inputs
    async with httpx.AsyncClient(timeout=60) as client:
        r = await client.post(
            f"{GITHUB_API}/repos/{owner}/{repo}/actions/workflows/{workflow}/dispatches",
            headers=_headers(),
            json=payload,
        )
    if r.status_code not in (200, 204):
        raise HTTPException(r.status_code, r.text)
    return {"ok": True, "workflow": workflow, "ref": body.ref}


@app.get("/")
def index() -> FileResponse:
    index_path = os.path.join(_static, "index.html")
    if not os.path.isfile(index_path):
        raise HTTPException(500, "static/index.html missing")
    return FileResponse(index_path)


if os.path.isdir(_static):
    app.mount("/static", StaticFiles(directory=_static), name="static")
