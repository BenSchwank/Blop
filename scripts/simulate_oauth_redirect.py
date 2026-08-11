#!/usr/bin/env python3
"""Simulate Android OAuth redirect URI shapes that Blop must accept.

Mirrors what Android Uri + Qt QUrlQuery extract for reverse-client-id and
legacy package schemes (path-form and host-form).

Exit 0 if all cases yield code+state; exit 1 otherwise.
"""
from urllib.parse import urlparse, parse_qs

REVERSE = (
    "com.googleusercontent.apps.571766217-5pcb10b1bgdv5g31vjgfvftdudufjc4s"
)
LEGACY = "com.benschwank.blop"


def extract(uri: str) -> tuple[str, str, str | None, str]:
    p = urlparse(uri)
    q = parse_qs(p.query)
    code = (q.get("code") or [""])[0]
    state = (q.get("state") or [""])[0]
    return p.scheme, p.path or "", p.hostname, f"code={code} state={state}"


def main() -> int:
    cases = [
        ("reverse-path", f"{REVERSE}:/oauth2redirect?code=ABC&state=XYZ"),
        ("reverse-host", f"{REVERSE}://oauth2redirect?code=ABC&state=XYZ"),
        ("reverse-host-slash", f"{REVERSE}://oauth2redirect/?code=ABC&state=XYZ"),
        ("legacy-path", f"{LEGACY}:/oauth2redirect?code=ABC&state=XYZ"),
        ("legacy-host", f"{LEGACY}://oauth2redirect?code=ABC&state=XYZ"),
        ("legacy-host-slash", f"{LEGACY}://oauth2redirect/?code=ABC&state=XYZ"),
    ]
    ok = True
    for name, uri in cases:
        scheme, path, host, summary = extract(uri)
        has = "ABC" in summary and "XYZ" in summary
        expect_scheme = REVERSE if name.startswith("reverse") else LEGACY
        good = has and scheme == expect_scheme
        print(
            f"{name}: scheme={scheme!r} host={host!r} path={path!r} "
            f"→ {summary} {'OK' if good else 'FAIL'}"
        )
        ok = ok and good
    print("note: host=None with path=/oauth2redirect is the expected single-slash shape")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
