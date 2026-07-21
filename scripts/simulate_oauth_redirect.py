#!/usr/bin/env python3
"""Simulate Android OAuth redirect URI shapes that Blop must accept.

Mirrors what Android Uri + Qt QUrlQuery extract for:
  com.benschwank.blop:/oauth2redirect?code=...&state=...   (path form, host null)
  com.benschwank.blop://oauth2redirect?code=...&state=...  (host form)

Exit 0 if both yield code+state; exit 1 otherwise.
"""
from urllib.parse import urlparse, parse_qs


def extract(uri: str) -> tuple[str, str, str | None, str]:
    p = urlparse(uri)
    q = parse_qs(p.query)
    code = (q.get("code") or [""])[0]
    state = (q.get("state") or [""])[0]
    return p.scheme, p.path or "", p.hostname, f"code={code} state={state}"


def main() -> int:
    cases = [
        ("path-form", "com.benschwank.blop:/oauth2redirect?code=ABC&state=XYZ"),
        ("host-form", "com.benschwank.blop://oauth2redirect?code=ABC&state=XYZ"),
        ("host-slash", "com.benschwank.blop://oauth2redirect/?code=ABC&state=XYZ"),
    ]
    ok = True
    for name, uri in cases:
        scheme, path, host, summary = extract(uri)
        has = "ABC" in summary and "XYZ" in summary
        print(f"{name}: scheme={scheme!r} host={host!r} path={path!r} → {summary} {'OK' if has else 'FAIL'}")
        ok = ok and has and scheme == "com.benschwank.blop"
    # Expected toast diagnostics: host=None for path-form is NOT an error.
    print("note: host=None with path=/oauth2redirect is the expected single-slash shape")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
