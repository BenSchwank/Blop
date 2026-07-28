# Windows release checklist

Steps for a public desktop build. Installer versioning is already wired from
`git describe` in CI/`installer.nsi`. Clean-VM smoke still needs a human VM;
Authenticode is wired in CI when secrets are present.

## Before tagging

- [ ] `git describe --tags` matches the intended `v3.22.N` (or current series)
- [ ] Roadmap DoD items for this cut are green (tools undo/persist, chrome, consent)
- [ ] Windows CI green on the release commit
  (`ctest` persistence + tool sequences in `windows_build.yml`)

## Code signing (Authenticode)

CI steps in `.github/workflows/windows_build.yml` sign `deployment/Blop.exe`
(before zip) and `Blop_Windows_Installer.exe` (after NSIS) when secrets exist.
Without secrets the steps skip and still produce unsigned artifacts.

| GitHub Actions secret | Use |
|-----------------------|-----|
| `WINDOWS_CERT_PFX_BASE64` | Base64-encoded `.pfx` (PKCS#12) |
| `WINDOWS_CERT_PASSWORD` | PFX password |
| `WINDOWS_CERT_TIMESTAMP_URL` | Optional; default `http://timestamp.digicert.com` |

Local equivalent:

```bat
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /f blop.pfx /p %CERT_PASSWORD% Blop_Windows_Installer.exe
signtool verify /pa Blop_Windows_Installer.exe
```

## Clean-VM smoke

On a VM **without** prior Blop installs or Qt:

1. Copy the signed installer from the release artifact.
2. Install for current user (and once as admin if you ship that mode).
3. Launch from Start Menu shortcut — version string in Settings matches tag.
4. Create a note → Pen / Pencil / Highlighter / Eraser / Shape → Undo/Redo →
   close app → reopen note (round-trip).
5. Toggle Light theme; open note chrome (no dark islands).
6. Decline crash upload on first run; confirm app runs; enable in Settings →
   Erweitert and confirm consent persists across restart.
7. Uninstall; confirm Start Menu entry and install dir removed.

Record failures against the roadmap Phase 3 Windows item.

## What Cloud Agents cannot do

- Hold or apply production signing certificates (secrets live in GitHub)
- Run a true Clean VM from this Ubuntu desktop environment

Use this checklist when human/release CI has the secrets.
