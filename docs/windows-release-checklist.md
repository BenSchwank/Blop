# Windows release checklist

Steps for a public desktop build. Installer versioning is already wired from
`git describe` in CI/`installer.nsi`. Code signing and Clean-VM smoke need
machine secrets / a clean VM — not inventable in Cloud Agents.

## Before tagging

- [ ] `git describe --tags` matches the intended `v3.22.N` (or current series)
- [ ] Roadmap DoD items for this cut are green (tools undo/persist, chrome, consent)
- [ ] Windows CI green on the release commit
  (`ctest` persistence + tool sequences in `windows_build.yml`)

## Code signing (Authenticode)

Requires org secrets (store outside git):

| Secret / asset | Use |
|----------------|-----|
| Code-signing certificate (`.pfx` or cloud HSM) | Sign `Blop.exe` and the NSIS installer |
| Certificate password / HSM creds | CI or release machine only |
| Timestamp URL | e.g. DigiCert/Sectigo timestamp server |

Suggested local/CI flow after `makensis`:

```bat
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /f blop.pfx /p %CERT_PASSWORD% Blop-Setup.exe
signtool verify /pa Blop-Setup.exe
```

Gate the sign step on secret presence (same pattern as Android keystore).
Unsigned CI artifacts may still be produced for internal smoke.

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

- Hold or apply production signing certificates
- Run a true Clean VM from this Ubuntu desktop environment

Use this checklist when human/release CI has the secrets.
