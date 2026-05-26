#!/usr/bin/env bash
# Build static mbedTLS + libcurl (HTTPS via mbedTLS) for Android ABIs.
# Used so sentry-native can use SENTRY_TRANSPORT=curl on Qt-Android/CMake CI.
# Usage: build_android_curl_mbedtls.sh <NDK_ROOT> <ANDROID_API_LEVEL> <INSTALL_PREFIX> [abi ...]
# Example:
#   ./build_android_curl_mbedtls.sh "$ANDROID_NDK_ROOT" 34 "$GITHUB_WORKSPACE/.deps/curlmbed" \
#       arm64-v8a armeabi-v7a x86 x86_64
set -euo pipefail

NDK_ROOT="${1:?NDK root required}"
API="${2:?API level required}"
PREFIX="${3:?install prefix required}"
shift 3

if [[ ! -d "${NDK_ROOT}/build/cmake" ]]; then
  echo "NDK_ROOT does not look valid: ${NDK_ROOT}" >&2
  exit 1
fi
if [[ "$#" -lt 1 ]]; then
  set -- arm64-v8a armeabi-v7a x86 x86_64
fi

TOOLCHAIN="${NDK_ROOT}/build/cmake/android.toolchain.cmake"
if [[ ! -f "${TOOLCHAIN}" ]]; then
  echo "NDK toolchain not found: ${TOOLCHAIN}" >&2
  exit 1
fi

WORKDIR="${WORKDIR:-$(pwd)/.curl-mbedtls-src}"
mkdir -p "${WORKDIR}"
cd "${WORKDIR}"

MBEDTLS_VER="mbedtls-3.5.2"
CURL_TAG="curl-8_5_0"

fetch_tgz() {
  local url="$1"
  local out="$2"
  if [[ ! -f "$out" ]]; then
    curl -fsSL "${url}" -o "${out}"
  fi
}

fetch_tgz "https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/${MBEDTLS_VER}.tar.gz" mbedtls.tgz
fetch_tgz "https://github.com/curl/curl/archive/refs/tags/${CURL_TAG}.tar.gz" curl.tgz

rm -rf mbedtls-src curl-src
mkdir -p mbedtls-src curl-src
tar -xzf mbedtls.tgz -C mbedtls-src --strip-components=1
tar -xzf curl.tgz -C curl-src --strip-components=1

CMAKE_COMMON=(
  -G Ninja
  -DCMAKE_MAKE_PROGRAM="$(command -v ninja)"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}"
  -DANDROID_PLATFORM="android-${API}"
  -DANDROID_STL=c++_shared
)

build_one_abi() {
  local abi="$1"
  local inst="${PREFIX}/${abi}"
  rm -rf "${inst}"
  mkdir -p "${inst}"

  local mb_build="${WORKDIR}/mbedtls-build-${abi}"
  local curl_build="${WORKDIR}/curl-build-${abi}"
  rm -rf "${mb_build}" "${curl_build}"

  cmake -S mbedtls-src -B "${mb_build}" \
    "${CMAKE_COMMON[@]}" \
    -DANDROID_ABI="${abi}" \
    -DENABLE_PROGRAMS=OFF \
    -DENABLE_TESTING=OFF \
    -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
    -DCMAKE_INSTALL_PREFIX="${inst}"
  cmake --build "${mb_build}"
  cmake --install "${mb_build}"

  # curl's bundled CMake/FindMbedTLS.cmake uses find_path/find_library, which
  # the Android NDK toolchain restricts to the NDK sysroot (CMAKE_FIND_ROOT_PATH_MODE_*=ONLY).
  # That hides our ${inst}/{include,lib}, so we pre-set the cache variables it expects
  # and additionally widen the find-root-mode to BOTH for this sub-build only.
  cmake -S curl-src -B "${curl_build}" \
    "${CMAKE_COMMON[@]}" \
    -DANDROID_ABI="${abi}" \
    -DCMAKE_INSTALL_PREFIX="${inst}" \
    -DCMAKE_PREFIX_PATH="${inst}" \
    -DCMAKE_FIND_ROOT_PATH="${inst}" \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DMBEDTLS_INCLUDE_DIRS="${inst}/include" \
    -DMBEDTLS_LIBRARY="${inst}/lib/libmbedtls.a" \
    -DMBEDX509_LIBRARY="${inst}/lib/libmbedx509.a" \
    -DMBEDCRYPTO_LIBRARY="${inst}/lib/libmbedcrypto.a" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_CURL_EXE=OFF \
    -DCURL_STATICLIB=ON \
    -DCURL_USE_MBEDTLS=ON \
    -DCURL_DISABLE_LDAP=ON \
    -DCURL_DISABLE_LDAPS=ON \
    -DUSE_NGHTTP2=OFF \
    -DUSE_LIBIDN2=OFF \
    -DCURL_ZLIB=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_CURL_TESTS=OFF

  cmake --build "${curl_build}"
  cmake --install "${curl_build}"

  echo "Installed static curl for ${abi} under ${inst}"
}

for abi in "$@"; do
  build_one_abi "${abi}"
done

echo "All ABIs done. Point CMake at: -DBLOP_ANDROID_CURL_INSTALL_ROOT=${PREFIX}"
