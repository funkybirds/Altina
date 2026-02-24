#!/usr/bin/env bash
set -euo pipefail

PRESET="${1:-linux-gcc-relwithdebinfo}"
FORCE_CONFIGURE="${FORCE_CONFIGURE:-0}"
# Optional: "ON" or "OFF" (overrides the cached value via a configure pass).
SHIPPING="${2:-${AE_DEMO_ENABLE_SHIPPING:-}}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="${REPO_ROOT}/out/build/${PRESET}"

configure() {
  echo "[CMake] Configuring preset '${PRESET}'"
  if [[ -n "${SHIPPING}" ]]; then
    cmake --preset "${PRESET}" "-DAE_DEMO_ENABLE_SHIPPING=${SHIPPING}"
  else
    cmake --preset "${PRESET}"
  fi
}

cd "${REPO_ROOT}"

if [[ "${FORCE_CONFIGURE}" == "1" || ! -d "${BUILD_DIR}" || -n "${SHIPPING}" ]]; then
  configure
fi

echo "[CMake] Building AltinaEngineDemoMinimal via preset '${PRESET}'"
echo "[CMake] Cleaning Demo/Minimal output folders (Binaries/Shipping)"
cmake --build --preset "${PRESET}" --target AltinaEngineDemoMinimalPreClean
cmake --build --preset "${PRESET}" --target AltinaEngineDemoMinimal
