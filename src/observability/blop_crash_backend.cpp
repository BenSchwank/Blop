#include "blop_crash_backend.h"
#include "blop_observability_build.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

#include <cstdlib>

namespace {

constexpr auto kConsentOrg = "Blop";
constexpr auto kConsentApp = "BlopApp";
constexpr auto kConsentKey = "privacy/crash_upload_consent";

} // namespace

bool blopCrashUploadConsentAsked()
{
  QSettings s(QString::fromLatin1(kConsentOrg), QString::fromLatin1(kConsentApp));
  return s.contains(QString::fromLatin1(kConsentKey));
}

bool blopCrashUploadConsentGranted()
{
  QSettings s(QString::fromLatin1(kConsentOrg), QString::fromLatin1(kConsentApp));
  return s.value(QString::fromLatin1(kConsentKey), false).toBool();
}

void blopSetCrashUploadConsent(bool granted)
{
  QSettings s(QString::fromLatin1(kConsentOrg), QString::fromLatin1(kConsentApp));
  s.setValue(QString::fromLatin1(kConsentKey), granted);
  s.sync();
  if (granted)
    blopInitCrashReporting();
  else
    blopShutdownCrashReporting();
}

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
  qInfo() << "[BlopObs] Sentry: shut down (consent withdrawn or quit)";
}

void blopInitCrashReporting()
{
  if (g_sentryOpen)
    return;

  // Runtime privacy gate (Phase 3). Build-time BLOP_OBS_CONSENT_* is only a
  // CI/default hint — never upload without an explicit user choice.
  if (!blopCrashUploadConsentGranted()) {
    if (!blopCrashUploadConsentAsked()) {
      qInfo() << "[BlopObs] Sentry: waiting for crash-upload consent (first run)";
    } else {
      qInfo() << "[BlopObs] Sentry: crash upload declined — uploads disabled";
    }
    return;
  }

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
    qInfo() << "[BlopObs] Sentry: consent granted but no DSN — set SENTRY_DSN or "
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

void blopInitCrashReporting()
{
  if (!blopCrashUploadConsentGranted()) {
    qInfo() << "[BlopObs] Crash backend not linked (BLOP_ENABLE_SENTRY=OFF); "
               "consent still recorded for future builds.";
  }
}

#endif
