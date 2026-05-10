#include "blop_crash_backend.h"
#include "blop_observability_build.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <cstdlib>

#if defined(BLOP_SENTRY_ENABLED) && BLOP_SENTRY_ENABLED
#  include <sentry.h>

namespace {

bool g_sentryOpen = false;

} // namespace

void blopShutdownCrashReporting()
{
  if (!g_sentryOpen) {
    return;
  }
  sentry_close();
  g_sentryOpen = false;
}

void blopInitCrashReporting()
{
  // Precedence (documented in docs/ADR-observability.md): non-empty SENTRY_DSN env wins,
  // otherwise BLOP_SENTRY_COMPILE_DSN from CMake (-DBLOP_SENTRY_DSN=...).
  const QByteArray envDsn = qgetenv("SENTRY_DSN");
  const char *dsn = nullptr;
  if (!envDsn.isEmpty()) {
    dsn = envDsn.constData();
  } else if (BLOP_SENTRY_COMPILE_DSN[0] != '\0') {
    dsn = BLOP_SENTRY_COMPILE_DSN;
  }

  if (!dsn || dsn[0] == '\0') {
    qInfo() << "[BlopObs] Sentry: linked (BLOP_ENABLE_SENTRY=ON) but no DSN — set SENTRY_DSN or "
               "configure -DBLOP_SENTRY_DSN=... to enable uploads.";
    return;
  }

  sentry_options_t *options = sentry_options_new();
  sentry_options_set_dsn(options, dsn);
  sentry_options_set_release(options, BLOP_SENTRY_RELEASE_STR);
  sentry_options_set_environment(options, BLOP_OBS_BUILD_FLAVOR);
  sentry_options_set_dist(options, BLOP_OBS_BUILD_NUMBER);

  sentry_options_set_debug(options, qgetenv("SENTRY_DEBUG").isEmpty() ? 0 : 1);

  // sentry-native ≥0.14 defaults; keep scope to crashes unless product opts in later.
  sentry_options_set_enable_metrics(options, 0);
  sentry_options_set_enable_logs(options, 0);

  const QString dbDir =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QLatin1String("/sentry");
  QDir().mkpath(dbDir);
  sentry_options_set_database_path(options, dbDir.toUtf8().constData());

#ifdef Q_OS_WIN
  const QString handler =
      QCoreApplication::applicationDirPath() + QLatin1String("/crashpad_handler.exe");
  if (QFile::exists(handler)) {
    sentry_options_set_handler_path(options, handler.toUtf8().constData());
  }
#endif

  const int initCode = sentry_init(options);
  if (initCode != 0) {
    qWarning() << "[BlopObs] Sentry: sentry_init failed with code" << initCode;
    return;
  }

  g_sentryOpen = true;
  qInfo() << "[BlopObs] Sentry: initialized; release" << BLOP_SENTRY_RELEASE_STR;

#if defined(BLOP_SENTRY_FORCE_TEST_CRASH) && BLOP_SENTRY_FORCE_TEST_CRASH
  qWarning() << "[BlopObs] Sentry: BLOP_SENTRY_FORCE_TEST_CRASH — abort()";
  std::abort();
#endif
}

#else

void blopShutdownCrashReporting() {}

void blopInitCrashReporting() {}

#endif
