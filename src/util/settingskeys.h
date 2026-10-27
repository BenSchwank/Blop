#pragma once

#include <QSettings>
#include <QString>

/// Central registry of QSettings keys that more than one file touches, so a
/// rename or typo cannot silently split a setting in two. Add new shared keys
/// here instead of repeating string literals; existing call sites are being
/// migrated incrementally.
///
/// NOTE: organization/application ("Blop"/"BlopApp") are deliberately NOT
/// changed - that would move the settings store and orphan user data.
namespace SettingsKeys {

inline QSettings open() {
  return QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
}

// --- Auth / session -------------------------------------------------------
inline QString username() { return QStringLiteral("username"); }
inline QString sessionId() { return QStringLiteral("session_id"); }
/// true = user chose to use Blop Notes without a Study account.
inline QString authGuestMode() { return QStringLiteral("auth/guestMode"); }

// --- Onboarding / UI ------------------------------------------------------
inline QString onboardingDone() { return QStringLiteral("ui/onboardingDone"); }
/// NoteToolbarStyle: bar | floating | radiant | science.
inline QString noteToolbar() { return QStringLiteral("ui/note_toolbar"); }
inline QString autoSaveMs() { return QStringLiteral("ui/autoSaveMs"); }
inline QString defaultPageColor() {
  return QStringLiteral("ui/defaultPageColor");
}

// --- Privacy / diagnostics ------------------------------------------------
/// Opt-in: upload anonymous crash reports (default false).
inline QString crashUpload() { return QStringLiteral("privacy/crashUpload"); }
inline QString diagSessionTrace() {
  return QStringLiteral("diag/sessionTrace");
}

// --- Updater --------------------------------------------------------------
inline QString dismissedUpdateVersion() {
  return QStringLiteral("dismissedUpdateVersion");
}

} // namespace SettingsKeys