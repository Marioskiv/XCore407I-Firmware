#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-analysis"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure compile commands for clang-tidy
cmake -DREMORA_STRICT=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S "$ROOT_DIR/targets/TARGET_XCORE407I" -B .

# Run cppcheck (focus on our firmware sources, skip Cube & lwIP third party)
CPPCHK_EXCLUDES="--exclude=$ROOT_DIR/st/STM32CubeF4 --exclude=$ROOT_DIR/targets/TARGET_XCORE407I/lwip_port --exclude=$ROOT_DIR/st/hal_conf"
cppcheck --enable=warning,style,performance,portability --std=c11 $CPPCHK_EXCLUDES "$ROOT_DIR/Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I" 2> cppcheck-report.txt || true

# Run clang-tidy on key source files (can be expanded)
if command -v clang-tidy >/dev/null 2>&1; then
  CLANG_TIDY_FILES=(
    "$ROOT_DIR/Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I/remora_data.c"
    "$ROOT_DIR/Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I/lwip/remora_udp.c"
    "$ROOT_DIR/Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I/motion_io.c"
    "$ROOT_DIR/Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I/stepgen.c"
  )
  for f in "${CLANG_TIDY_FILES[@]}"; do
    [ -f "$f" ] && clang-tidy "$f" -- -I"$ROOT_DIR/Remora-OS6/Firmware/FirmwareSource/Remora-OS6" -I"$ROOT_DIR/st/STM32CubeF4/Drivers/CMSIS/Include" -I"$ROOT_DIR/st/STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
  done | tee clang-tidy-report.txt
else
  echo "clang-tidy not found; skipping" | tee clang-tidy-report.txt
fi

echo "Static analysis complete. Reports: cppcheck-report.txt, clang-tidy-report.txt"
