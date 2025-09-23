#!/usr/bin/env bash
set -euo pipefail
# DFU-only build helper for the known-good Ethernet configuration.
# Usage: ./scripts/build_dfu_good.sh [extra -D flags]
BUILD_DIR=build-dfu
CMAKE_FLAGS=(
  -DUSE_ABSTRACT_DRIVERS=ON
  -DENABLE_QEI_DIAG=ON
  -DBOARD_ENABLE_ETH=ON
  -DBOARD_ENABLE_ULPI=OFF
  -DBOARD_ENABLE_NAND=OFF
  -DCMAKE_BUILD_TYPE=Release
)
CMAKE_FLAGS+=("$@")

PROFILE_FILE=cmake/eth_good_profile.cmake
EXTRA_PRESET=()
if [[ -f "$PROFILE_FILE" ]]; then
  echo "[build_dfu_good] Using profile -C $PROFILE_FILE"
  EXTRA_PRESET=(-C "$PROFILE_FILE")
fi

echo "[build_dfu_good] Configure ${BUILD_DIR}"
cmake "${EXTRA_PRESET[@]}" -B "${BUILD_DIR}" -S . "${CMAKE_FLAGS[@]}"

echo "[build_dfu_good] Build"
cmake --build "${BUILD_DIR}" -j

FLASH_BIN="${BUILD_DIR}/targets/TARGET_XCORE407I/XCORE407I_FIRMWARE_flash.bin"
RAW_BIN="${BUILD_DIR}/targets/TARGET_XCORE407I/XCORE407I_FIRMWARE.bin"
if [[ -f "$FLASH_BIN" ]]; then
  ls -lh "$FLASH_BIN" "$RAW_BIN" 2>/dev/null || true
  echo "[build_dfu_good] Flash via DFU (BOOT0=1 then reset, verify dfu-util -l):"
  echo "dfu-util -a 0 -s 0x08000000:leave -D $FLASH_BIN"
else
  echo "[build_dfu_good] Trimmed flash binary not found (maybe DFU_TRIM_BIN=OFF)." >&2
fi
