# Windows release checklist

Steps for a public desktop build. Installer versioning is wired from
`git describe` in CI/`installer.nsi`. Preferred signing path is **Azure Artifact
Signing** (account `blop-signing`). PFX remains a fallback.

## Before tagging

- [ ] `git describe --tags` matches the intended `v3.22.N` (or current series)
- [ ] Roadmap DoD items for this cut are green
- [ ] Windows CI green on the release commit
- [ ] Azure identity validation **Completed** (Organisation / Public Trust)
- [ ] Certificate profile exists (Public Trust / Code Signing)

## Path A — Azure Artifact Signing (preferred)

### A1. Azure resources (manual)

1. Azure Portal → Artifact Signing account `blop-signing` (RG `blop-signing`, North Europe, Basic)
2. Role on your user: **Artifact Signing Identity Verifier**
3. **Identitätsüberprüfungen** → Organisation → Public → wait until **Completed**
4. **Zertifikatprofile** → create e.g. `blop-public` (Public Trust, Code Signing, linked identity)
5. Create an Entra **App registration** for GitHub Actions (or use existing SP)
6. On the signing account IAM: assign that app
   **Artifact Signing Certificate Profile Signer**

### A2. GitHub Actions secrets

| Secret | Example / notes |
|--------|------------------|
| `AZURE_CLIENT_ID` | App registration application (client) ID |
| `AZURE_TENANT_ID` | Directory (tenant) ID |
| `AZURE_SUBSCRIPTION_ID` | Subscription ID |
| `AZURE_CLIENT_SECRET` | App client secret (or later switch to OIDC) |
| `AZURE_TRUSTED_SIGNING_ACCOUNT` | `blop-signing` |
| `AZURE_TRUSTED_SIGNING_PROFILE` | e.g. `blop-public` |
| `AZURE_TRUSTED_SIGNING_ENDPOINT` | North Europe: `https://neu.codesigning.azure.net/` |

CI signs `deployment/Blop.exe` then `Blop_Windows_Installer.exe` when these are set
(`.github/workflows/windows_build.yml`). Without them the Azure steps are skipped.

### A3. Verify a CI run

1. Trigger Windows workflow (`workflow_dispatch` or push to master/tag)
2. Confirm steps **Sign Blop.exe (Azure Artifact Signing)** and installer sign are green
3. Download artifact → Properties → Digital Signatures should show a Microsoft/Azure CA chain

## Path B — PFX fallback (optional)

Only if not using Azure. Secrets:

| Secret | Use |
|--------|-----|
| `WINDOWS_CERT_PFX_BASE64` | Base64 `.pfx` |
| `WINDOWS_CERT_PASSWORD` | PFX password |
| `WINDOWS_CERT_TIMESTAMP_URL` | Optional timestamp URL |

Used only when Azure Artifact Signing secrets are incomplete.

## Clean-VM smoke

On a VM **without** prior Blop installs or Qt:

1. Copy the signed installer from the release artifact.
2. Install → launch → version matches tag.
3. Create a note → Pen / Eraser / Shape → Undo/Redo → reopen note.
4. Light theme + crash-consent toggle.
5. Uninstall cleanly.

## What Cloud Agents cannot do

- Complete AU10TIX / Microsoft org identity validation for you
- Hold production Azure client secrets (you paste them into GitHub Secrets)

When identity shows **Completed**, continue with certificate profile + secrets above.
