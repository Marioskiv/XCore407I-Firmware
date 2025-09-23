#!/usr/bin/env bash
set -euo pipefail
# Build helper for known-good Ethernet configuration.
# Adjust BUILD_DIR if you want multiple variants.
BUILD_DIR=build-ethgood

# Default flags capturing working Ethernet + diagnostics profile.
CMAKE_FLAGS=(
  -DUSE_ABSTRACT_DRIVERS=ON
  -DENABLE_QEI_DIAG=ON
  -DCMAKE_BUILD_TYPE=Release
)

# Allow caller to append extra flags: ./scripts/build_eth_good.sh -DETH_LEGACY_DESC_IN_CCM=ON
CMAKE_FLAGS+=("$@")

echo "[build_eth_good] Configuring ${BUILD_DIR} with: ${CMAKE_FLAGS[*]}" 
cmake -B "${BUILD_DIR}" -S targets/TARGET_XCORE407I "${CMAKE_FLAGS[@]}"

echo "[build_eth_good] Building"
cmake --build "${BUILD_DIR}" -j

if [[ -f "${BUILD_DIR}/XCORE407I_FIRMWARE_flash.bin" ]]; then
  ls -lh "${BUILD_DIR}/XCORE407I_FIRMWARE"* | sed 's/^/[build_eth_good] /'
  echo "[build_eth_good] Done. Flash via DFU: dfu-util -a 0 -s 0x08000000:leave -D ${BUILD_DIR}/XCORE407I_FIRMWARE_flash.bin"
else
  echo "[build_eth_good] Trimmed DFU binary not found; DFU_TRIM_BIN may be OFF" >&2
fi
