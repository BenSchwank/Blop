#!/usr/bin/env python3
"""Optional live check against GitHub (PAT required). Run from ``control-center/`` after pip install.

Usage::

    set GITHUB_TOKEN=ghp_...
    set CONTROL_CENTER_REPOSITORY=OWNER/Repo
    python smoke_local.py
    python smoke_local.py --dispatch-smoke

Exits 0 when API reachable and workflows list non-empty; 1 on hard failure.
"""

from __future__ import annotations

import argparse
import os
import sys


def main() -> int:
    ap = argparse.ArgumentParser(description="GitHub API smoke test for Blop control center.")
    ap.add_argument(
        "--dispatch-smoke",
        action="store_true",
        help="POST workflow_dispatch for automation_smoke.yml (needs workflow scope on PAT).",
    )
    ap.add_argument("--ref", default="main", help="Branch or tag for dispatch (default main).")
    args = ap.parse_args()

    repo = os.environ.get("CONTROL_CENTER_REPOSITORY") or os.environ.get("GITHUB_REPOSITORY")
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("CONTROL_CENTER_TOKEN")

    try:
        import httpx  # noqa: WPS433
    except ImportError:
        print("Install deps first: pip install -r requirements.txt", file=sys.stderr)
        return 1

    if not token:
        if args.dispatch_smoke:
            print(
                "GITHUB_TOKEN or CONTROL_CENTER_TOKEN required for --dispatch-smoke.",
                file=sys.stderr,
            )
            return 1
        print("Skip: no GITHUB_TOKEN / CONTROL_CENTER_TOKEN — set one to probe GitHub API.")
        return 0
    if not repo or "/" not in repo:
        print(
            "Skip: set CONTROL_CENTER_REPOSITORY=owner/repo "
            "(or GITHUB_REPOSITORY) for workflow checks.",
            file=sys.stderr,
        )
        return 1

    owner, name = repo.split("/", 1)
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
    }

    with httpx.Client(timeout=30.0) as client:
        rme = client.get("https://api.github.com/user", headers=headers)
        if rme.status_code != 200:
            print(f"AUTH_FAILED {rme.status_code}: {rme.text[:500]}", file=sys.stderr)
            return 1
        login = rme.json().get("login")
        print(f"OK authenticated as GitHub user: {login}")

        rr = client.get(f"https://api.github.com/repos/{owner}/{name}", headers=headers)
        if rr.status_code != 200:
            print(f"REPO_FAILED {rr.status_code}: {rr.text[:500]}", file=sys.stderr)
            return 1
        print(f"OK repo reachable: {rr.json().get('full_name')}")

        rw = client.get(
            f"https://api.github.com/repos/{owner}/{name}/actions/workflows",
            headers=headers,
        )
        if rw.status_code != 200:
            print(f"WORKFLOWS_FAILED {rw.status_code}: {rw.text[:500]}", file=sys.stderr)
            return 1
        workflows = rw.json().get("workflows", [])
        print(f"OK workflows listed: {len(workflows)}")
        needles = ("automation_smoke.yml", "windows_build.yml", "android_build.yml")
        paths = [w.get("path", "").replace("\\", "/") for w in workflows]
        for n in needles:
            hit = any(p.endswith(n) or p.endswith(f"/workflows/{n}") for p in paths)
            print(f"  {'+' if hit else '?'} {n}")

        rsp = client.get(f"https://api.github.com/repos/{owner}/{name}/git/ref/heads/main", headers=headers)
        if rsp.status_code == 200:
            print("OK default branch ref 'main' resolves")
        else:
            rspm = client.get(f"https://api.github.com/repos/{owner}/{name}/git/ref/heads/master", headers=headers)
            if rspm.status_code == 200:
                print("OK default branch ref 'master' resolves (use ref=master when dispatching)")
            else:
                print(f"? could not resolve main/master ref ({rsp.status_code})")

        if args.dispatch_smoke:
            ds = client.post(
                f"https://api.github.com/repos/{owner}/{name}/actions/workflows/"
                "automation_smoke.yml/dispatches",
                headers=headers,
                json={"ref": args.ref},
            )
            if ds.status_code not in (200, 204):
                print(
                    f"DISPATCH_FAILED {ds.status_code}: {ds.text[:800]}",
                    file=sys.stderr,
                )
                print(
                    "(Is automation_smoke.yml merged on this repo? PAT needs workflow scope.)",
                    file=sys.stderr,
                )
                return 1
            print(
                f"OK dispatched automation_smoke.yml on ref={args.ref} — check Actions tab.",
            )

    print("Smoke done. Use UI or --dispatch-smoke to run automation_smoke.yml.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
