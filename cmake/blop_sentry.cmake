# Blop sentry-native wiring (FetchContent). CMakeLists loads this only when BLOP_ENABLE_SENTRY=ON.
# Grep: BLOP_SENTRY_NATIVE_VERSION, BLOP_SENTRY_ANDROID_TRANSPORT, SENTRY_DSN, BLOP_SENTRY_DSN

include(FetchContent)

set(BLOP_SENTRY_NATIVE_VERSION "0.14.0" CACHE STRING "Pinned sentry-native tag (see https://github.com/getsentry/sentry-native/releases)")

# Optional compile-time DSN (empty = rely on runtime env SENTRY_DSN). Prefer env in CI/production.
set(BLOP_SENTRY_DSN "" CACHE STRING "Optional Sentry DSN baked via blop_observability_build.h (escape handled in CMakeLists; prefer SENTRY_DSN at runtime)")
set(BLOP_SENTRY_FORCE_TEST_CRASH OFF CACHE BOOL "If ON and BLOP_ENABLE_SENTRY, abort() after sentry_init for local verification")

string(REPLACE "\\" "\\\\" BLOP_SENTRY_COMPILE_DSN_ESC "${BLOP_SENTRY_DSN}")
string(REPLACE "\"" "\\\"" BLOP_SENTRY_COMPILE_DSN_ESC "${BLOP_SENTRY_COMPILE_DSN_ESC}")

# sentry-native knobs (minimal surface; overridden before MakeAvailable).
set(SENTRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SENTRY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(ANDROID)
    # Crashpad is unsupported on Android; inproc + optional curl transport.
    set(SENTRY_BACKEND "inproc" CACHE STRING "" FORCE)
    #
    # Http: Android NDK does not ship libcurl — find_package rarely succeeds under qt-cmake.
    # Curl enabled when found; otherwise "none" (see ADR manual steps / Gradle prefab).
    set(BLOP_SENTRY_ANDROID_TRANSPORT "curl" CACHE STRING "Sentry ANDROID transport strategy: curl (if found) or fallback none")
endif()

FetchContent_Declare(
    sentry_native
    GIT_REPOSITORY https://github.com/getsentry/sentry-native.git
    GIT_TAG "${BLOP_SENTRY_NATIVE_VERSION}"
    GIT_SHALLOW TRUE
)

if(ANDROID AND BLOP_SENTRY_ANDROID_TRANSPORT STREQUAL "curl")
    # Optional: prefix tree from scripts/ci/build_android_curl_mbedtls.sh
    # Layout: $BLOP_ANDROID_CURL_INSTALL_ROOT/<ANDROID_ABI>/lib/libcurl.a
    set(BLOP_ANDROID_CURL_INSTALL_ROOT "" CACHE PATH
        "Root containing per-ABI curl+mbedTLS installs (lib/libcurl.a under each ABI folder)")
    if(BLOP_ANDROID_CURL_INSTALL_ROOT AND IS_DIRECTORY "${BLOP_ANDROID_CURL_INSTALL_ROOT}")
        if(DEFINED CMAKE_ANDROID_ARCH_ABI AND NOT "${CMAKE_ANDROID_ARCH_ABI}" STREQUAL "")
            set(_BLOP_CURL_PFX "${BLOP_ANDROID_CURL_INSTALL_ROOT}/${CMAKE_ANDROID_ARCH_ABI}")
        elseif(DEFINED ANDROID_ABI AND NOT "${ANDROID_ABI}" STREQUAL "")
            set(_BLOP_CURL_PFX "${BLOP_ANDROID_CURL_INSTALL_ROOT}/${ANDROID_ABI}")
        else()
            set(_BLOP_CURL_PFX "${BLOP_ANDROID_CURL_INSTALL_ROOT}")
        endif()
        if(EXISTS "${_BLOP_CURL_PFX}/lib/libcurl.a")
            list(PREPEND CMAKE_PREFIX_PATH "${_BLOP_CURL_PFX}")
            message(STATUS "Blop Sentry: prepended curl prefix for ABI: ${_BLOP_CURL_PFX}")
        endif()
    endif()

    find_package(CURL QUIET)
    if(CURL_FOUND AND TARGET CURL::libcurl)
        set(SENTRY_TRANSPORT "curl" CACHE STRING "" FORCE)
        message(STATUS "Blop Sentry: Android using SENTRY_TRANSPORT=curl (found CURL::libcurl)")
    else()
        set(SENTRY_TRANSPORT "none" CACHE STRING "" FORCE)
        message(WARNING
            "Blop Sentry: Android libcurl not found for this toolchain — using SENTRY_TRANSPORT=none. "
            "Run scripts/ci/build_android_curl_mbedtls.sh and pass "
            "-DBLOP_ANDROID_CURL_INSTALL_ROOT=... or see docs/ADR-observability.md.")
    endif()
endif()

# FetchContent failure surfaces as a clear configure error; disable with -DBLOP_ENABLE_SENTRY=OFF.
FetchContent_MakeAvailable(sentry_native)

if(NOT TARGET sentry::sentry)
    message(FATAL_ERROR "Blop Sentry: FetchContent_MakeAvailable(sentry_native) did not define target sentry::sentry")
endif()
