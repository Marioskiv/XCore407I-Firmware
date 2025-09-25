#!/usr/bin/env bash
set -euo pipefail

# Initialize only the submodules required for a minimal firmware build.
# This fetches CMSIS device headers, HAL driver, and LwIP middleware.
# Usage: ./scripts/init_minimal_submodules.sh

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

REQUIRED=(
  st/STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx
  st/STM32CubeF4/Drivers/STM32F4xx_HAL_Driver
  st/STM32CubeF4/Middlewares/Third_Party/LwIP
)

echo "[INFO] Initializing minimal required submodules..."
for path in "${REQUIRED[@]}"; do
  echo "[INFO] -> $path"
  git submodule update --init "$path"
done
echo "[INFO] Minimal submodule initialization complete."