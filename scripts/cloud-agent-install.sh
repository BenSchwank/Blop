#!/usr/bin/env bash
# Idempotent Cloud Agent update: ensure Qt/CMake toolchain + incremental Blop build.
#
# Hardened against:
# - Ubuntu 24.04 package rename (qt6-networkauth-dev Provides libqt6networkauth6-dev)
# - apt Release-file clock skew ("not valid yet") that previously exited 100
# - unnecessary apt when the toolchain is already present
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# True if the named package is installed OR provided by an installed package.
pkg_present() {
  local pkg="$1"
  dpkg -s "$pkg" >/dev/null 2>&1 && return 0
  # Accept Provides (e.g. qt6-networkauth-dev provides libqt6networkauth6-dev).
  dpkg-query -W -f='${Package} ${Provides}\n' 2>/dev/null \
    | tr ',' ' ' \
    | grep -Eq "(^| )${pkg}( |$)"
}

REQUIRED_PKGS=(
  cmake
  ninja-build
  g++
  qt6-base-dev
  qt6-declarative-dev
  qt6-multimedia-dev
  qt6-webengine-dev
  qt6-tools-dev
  qt6-networkauth-dev
)

NEED_APT=0
MISSING=()
for pkg in "${REQUIRED_PKGS[@]}"; do
  if ! pkg_present "$pkg"; then
    # Legacy alias: headers may already be under the old package name.
    if [[ "$pkg" == "qt6-networkauth-dev" ]] && pkg_present "libqt6networkauth6-dev"; then
      continue
    fi
    NEED_APT=1
    MISSING+=("$pkg")
  fi
done

apt_update_resilient() {
  # First try a normal update; on validity/clock skew retry without
  # Acquire::Check-Valid-Until so Chrome / noble-security Release skew
  # does not abort the whole Cloud Agent install (exit 100).
  if sudo apt-get update -qq; then
    return 0
  fi
  echo "cloud-agent-install: apt-get update failed; retrying with Check-Valid-Until=false" >&2
  sudo apt-get -o Acquire::Check-Valid-Until=false \
               -o Acquire::AllowInsecureRepositories=false \
               update -qq
}

if [[ "$NEED_APT" -eq 1 ]]; then
  echo "cloud-agent-install: missing packages: ${MISSING[*]}"
  apt_update_resilient
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    cmake ninja-build g++ pkg-config \
    qt6-base-dev qt6-base-dev-tools \
    qt6-declarative-dev qt6-declarative-dev-tools \
    qt6-multimedia-dev qt6-webengine-dev qt6-webengine-dev-tools \
    qt6-networkauth-dev \
    qt6-tools-dev qt6-tools-dev-tools \
    libgl1-mesa-dev libxkbcommon-dev
else
  echo "cloud-agent-install: toolchain packages already present; skipping apt"
fi

BUILD_DIR="${ROOT}/build-check"
if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
  cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
fi

cmake --build "$BUILD_DIR" --target Blop -j"$(nproc)"
echo "cloud-agent-install: Blop ready at ${BUILD_DIR}/Blop"
