#include "blop_observability.h"
#include "blop_observability_build.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QSysInfo>
#include <QtGlobal>

#include <cstdio>

namespace {

const BlopObservabilityMeta &holdMeta()
{
  static QByteArray prettyOsUtf8 = QSysInfo::prettyProductName().toUtf8();
  static const BlopObservabilityMeta m = {
      BLOP_OBS_APP_VERSION_STR,
      BLOP_OBS_PROJECT_VERSION_STR,
      BLOP_OBS_GIT_SHA,
      BLOP_OBS_BUILD_NUMBER,
      BLOP_OBS_BUILD_FLAVOR,
      BLOP_OBS_ANDROID_VERSION_CODE,
      BLOP_OBS_PLATFORM_SLUG,
      qVersion(),
      BLOP_OBS_QT_PKG_VERSION,
      prettyOsUtf8.constData(),
      static_cast<uint32_t>(BLOP_OBS_CONSENT_CRASH_UPLOAD),
      static_cast<uint32_t>(BLOP_OBS_CONSENT_ANALYTICS),
  };
  return m;
}

} // namespace

const BlopObservabilityMeta &blopObservabilityMeta() { return holdMeta(); }

void blopLogObservabilityBootstrap()
{
  const auto &m = holdMeta();
  const char *androidVc =
      (m.android_version_code && m.android_version_code[0]) ? m.android_version_code : "";
  // Single-line pseudo-JSON suitable for adb logcat / console greps ("BlopObs")
  std::fprintf(stderr,
               "[BlopObs] {\"app\":\"%s\",\"semver\":\"%s\",\"git\":\"%s\","
               "\"build\":\"%s\",\"flavor\":\"%s\",\"android_vc\":\"%s\","
               "\"tgt\":\"%s\",\"qt_rt\":\"%s\",\"qt_bd\":\"%s\",\"os\":\"%s\","
               "\"consent_crash\":%u,\"consent_analytics\":%u}\n",
               m.app_version, m.project_semver, m.git_sha, m.build_number,
               m.build_flavor, androidVc, m.target_slug, m.qt_version_runtime,
               m.qt_version_build, m.os_pretty_name, unsigned(m.consent_crash_upload),
               unsigned(m.consent_analytics));

  qInfo() << "[BlopObs] app" << m.app_version << "semver" << m.project_semver << "git"
          << m.git_sha << "build" << m.build_number << "flavor" << m.build_flavor
          << "android_vc" << (m.android_version_code ? m.android_version_code : "")
          << "target" << m.target_slug << "Qt" << m.qt_version_runtime << "/"
          << m.qt_version_build << "consent(crash/analytics)"
          << m.consent_crash_upload << m.consent_analytics;
}
