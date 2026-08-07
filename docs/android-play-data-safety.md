# Android Play Console — Data Safety checklist

Draft answers for Google Play **App content → Data safety**, based on
`android/AndroidManifest.xml`, `docs/privacy-policy.md`, and in-app crash consent.
Update this file when permissions or backends change.

## App identity

| Field | Value |
|-------|--------|
| Package / applicationId | `com.benschwank.blop` |
| Privacy policy URL | Publish `docs/privacy-policy.md` (or store-hosted HTML) before review |
| Approximate location | Not collected |
| Precise location | Not collected |

## Permissions (declared)

| Permission | Why | User-facing note |
|------------|-----|------------------|
| `INTERNET` | Optional Study login/share; optional crash upload | Offline notes work without network |
| `ACCESS_NETWORK_STATE` | Avoid useless network calls when offline | Same |

No camera, mic, contacts, SMS, or storage-scrape permissions in the shipping
manifest beyond Qt/FileProvider for sharing files the user chooses.

## Data types (Play form)

Answer **Does your app collect or share any of the required user data types?**
with **Yes** only for the optional paths below. Local note files on device are
**not** “collected” by you unless synced/shared by the user.

### Account / Study (optional)

| Play category | Collected? | Shared? | Purpose | Ephemeral? |
|---------------|------------|---------|---------|------------|
| Personal info → Name / Email (if Study SSO provides it) | Yes, when signed in | Shared with your Study backend only | App functionality (account) | No |
| App activity → Other in-app actions (share create) | Yes, when user shares | Your Study API | App functionality | No |

Notes content leaves the device **only** when the user explicitly shares or
uses cloud features while signed in.

### Crash diagnostics (optional)

| Play category | Collected? | Shared? | Purpose | Ephemeral? |
|---------------|------------|---------|---------|------------|
| App info and performance → Crash logs | Yes, **only if** user accepts crash upload | Shared with Sentry (crash backend) | Analytics / App functionality (stability) | Prefer “Yes” if you do not retain long-term |

Consent: first-run prompt + Settings → App → Erweitert
(`privacy/crash_upload_consent`). No Sentry init without consent.
See `docs/privacy-policy.md`.

### Not collected (default answers)

- Location, photos/videos (unless user picks a file to insert — treat as
  user-initiated, not background collection), contacts, financial, health,
  messages, device IDs for advertising, web browsing.

## Security practices (typical answers)

- Data encrypted in transit (HTTPS) for Study / crash upload: **Yes**
- Users can request deletion of account data: document Study backend process
  before claiming **Yes**
- Independent security review: **No** unless you have one

## OAuth / SHA-1 (Play Signing)

1. After Play App Signing is enabled, copy **App signing key certificate**
   SHA-1 from Play Console → Setup → App integrity.
2. Also register the **upload key** SHA-1 used by CI
   (`ANDROID_KEYSTORE_BASE64` / alias secrets in `android_build.yml`).
3. Add both SHA-1s to the Google Cloud OAuth Android client for package
   `com.benschwank.blop`.
4. Confirm deep-link schemes in the manifest still match the OAuth client
   (reverse client id + `com.benschwank.blop` `/oauth2redirect`).

Print upload-key fingerprints locally (after decoding the CI PFX/JKS):

```bash
# From a decoded keystore file (do not commit the keystore):
keytool -list -v -keystore blop-upload.jks -alias "$ANDROID_KEY_ALIAS"
# Look for SHA1 / SHA256 under the certificate fingerprint section.
```

Or use the helper (expects env vars, never prints the private key):

```bash
bash scripts/android-oauth-sha.sh /path/to/upload.jks "$ANDROID_KEY_ALIAS"
```

## AAB smoke (manual)

1. CI tag build produces signed `blop.aab` when keystore secrets are set.
2. Upload to Play internal testing track.
3. Install from Play on a physical device; cold start → open note → draw →
   undo → force-stop → reopen (persistence).
4. Optional: Study sign-in and share once with network.

Secrets required: `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEY_ALIAS`,
`ANDROID_KEYSTORE_PASSWORD` (see `.github/workflows/android_build.yml`).
