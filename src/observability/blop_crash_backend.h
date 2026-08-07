#pragma once

/// Native crash upload backend (Sentry when BLOP_ENABLE_SENTRY=ON).
/// Uploads are gated by runtime privacy consent — never init without it.

/// QSettings key: privacy/crash_upload_consent (bool). Absent = not asked yet.
[[nodiscard]] bool blopCrashUploadConsentAsked();
[[nodiscard]] bool blopCrashUploadConsentGranted();
/// Persist consent. When enabling, attempts (re)init; when disabling, shuts down.
void blopSetCrashUploadConsent(bool granted);

void blopInitCrashReporting();
void blopShutdownCrashReporting();
