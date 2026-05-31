#pragma once

#include <QString>

/// In-app crash + log diagnostics. Independent of Sentry; works with no
/// network and no DSN. Writes a file at <AppDataLocation>/last_crash.txt
/// when the process is killed by SIGSEGV/SIGABRT/SIGBUS/SIGILL/SIGFPE.
/// Replay the file contents at next launch via takeCrashReportIfPresent().
namespace BlopDiag {

/// Install the Qt message handler, the POSIX signal handlers, and pre-open
/// the crash dump file descriptor. Idempotent. Call once, after QApplication
/// has been constructed but before any heavy UI is built.
void install();

/// Append a UI action tag (free-form, e.g. "tab_click", "tile_pill_tap") to
/// the ring buffer. Cheap. Safe to call from the GUI thread.
void recordUiAction(const QString &tag);

/// If a crash dump from the previous run exists, return its contents and
/// remove the file. Otherwise return an empty string. Call once at startup
/// after MainWindow is shown.
QString takeCrashReportIfPresent();

/// Path to the last_crash.txt file (for debugging / docs); valid after
/// install() has run.
QString crashReportPath();

} // namespace BlopDiag
