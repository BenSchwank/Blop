#!/usr/bin/env bash
# Idempotent Cloud Agent update: ensure Qt/CMake toolchain + incremental Blop build.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

NEED_APT=0
for pkg in cmake ninja-build g++ qt6-base-dev qt6-declarative-dev \
           qt6-multimedia-dev qt6-webengine-dev libqt6networkauth6-dev \
           qt6-tools-dev; do
  if ! dpkg -s "$pkg" >/dev/null 2>&1; then
    NEED_APT=1
    break
  fi
done

if [[ "$NEED_APT" -eq 1 ]]; then
  sudo apt-get update -qq
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    cmake ninja-build g++ pkg-config \
    qt6-base-dev qt6-base-dev-tools \
    qt6-declarative-dev qt6-declarative-dev-tools \
    qt6-multimedia-dev qt6-webengine-dev qt6-webengine-dev-tools \
    libqt6networkauth6-dev qt6-tools-dev qt6-tools-dev-tools \
    libgl1-mesa-dev libxkbcommon-dev
fi

BUILD_DIR="${ROOT}/build-check"
if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
  cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
fi

cmake --build "$BUILD_DIR" --target Blop -j"$(nproc)"
echo "cloud-agent-install: Blop ready at ${BUILD_DIR}/Blop"
