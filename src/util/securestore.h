#pragma once

#include <QString>

/// Encrypts secrets (OAuth tokens) before they are written to QSettings.
/// Windows: DPAPI (CryptProtectData, bound to the current user account).
/// Other platforms: pass-through (Android relies on app-private storage and
/// android:allowBackup="false").
/// unprotect() accepts legacy plaintext values, so existing installs keep
/// working and are upgraded the next time the token is written.
namespace SecureStore {
QString protect(const QString &plain);
QString unprotect(const QString &stored);
} // namespace SecureStore