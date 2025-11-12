#!/usr/bin/env bash
set -euo pipefail

PRESET="${1:-linux-gcc-relwithdebinfo}"
FORCE_CONFIGURE="${FORCE_CONFIGURE:-0}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="${REPO_ROOT}/out/build/${PRESET}"

configure() {
  echo "[CMake] Configuring preset '${PRESET}'"
  cmake --preset "${PRESET}"
}

cd "${REPO_ROOT}"

if [[ "${FORCE_CONFIGURE}" == "1" || ! -d "${BUILD_DIR}" ]]; then
  configure
fi

echo "[CMake] Building AltinaEngineDemoMinimal via preset '${PRESET}'"
cmake --build --preset "${PRESET}" --target AltinaEngineDemoMinimal
