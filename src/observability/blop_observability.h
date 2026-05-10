#pragma once

#include <cstdint>

/// Immutable build-/boot-time observability envelope (Phase 0 contract).
/// Populated ahead of heavier UI/network work; pointers remain valid for app lifetime.
struct BlopObservabilityMeta {
  const char *app_version;      //!< tag/describe or CI override string
  const char *project_semver;   //!< CMake project() VERSION normalization
  const char *git_sha;          //!< full SHA or short; may be placeholder
  const char *build_number;     //!< CI run / local monotonic identifier
  const char *build_flavor;    //!< dev, release, ci_release, ...
  const char *android_version_code; //!< empty string on non-Android builds
  const char *target_slug;       //!< CMake platform bucket (windows, android, …)
  const char *qt_version_runtime; //!< from qVersion(); actual loaded Qt
  const char *qt_version_build; //!< from Qt CMake package version
  const char *os_pretty_name; //!< QSysInfo::prettyProductName (UTF-8)
  uint32_t consent_crash_upload; //!< build-time baked flag (runtime policy may tighten)
  uint32_t consent_analytics;
};

[[nodiscard]] const BlopObservabilityMeta &blopObservabilityMeta();

/// Log one structured line suitable for adb logcat / Windows debug out.
void blopLogObservabilityBootstrap();
