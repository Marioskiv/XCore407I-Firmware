#!/usr/bin/env bash
# Extended DFU flash helper for STM32F4 system bootloader (VID:PID 0483:df11)
# Features:
#  - Auto-detect default firmware binary if not provided
#  - Retry loop waiting for DFU enumeration
#  - Helpful guidance for entering DFU (runtime command, BOOT0 jumper, reset sequence)
#  - Parses dfu-util output robustly (ignores non-zero leave status if success string present)
#
# Usage:
#   scripts/flash_dfu.sh [firmware.bin] [--retries N] [--wait SECONDS] [--quiet]
# Defaults:
#   firmware.bin -> build/XCORE407I_FIRMWARE.bin if exists
#   retries      -> 10 (1s interval)
#   wait         -> 0 (no extra post-flash wait)
#
set -euo pipefail

FW=""
RETRIES=10
WAIT=0
QUIET=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --retries) RETRIES=${2:-10}; shift 2;;
    --wait) WAIT=${2:-0}; shift 2;;
    --quiet) QUIET=1; shift;;
    -h|--help)
      sed -n '1,60p' "$0" | sed 's/^# //p' | sed '/^#!/d'
      exit 0;;
    *)
      if [[ -z "$FW" ]]; then FW=$1; else echo "[flash_dfu] WARN: extra arg $1 ignored" >&2; fi
      shift;;
  esac
done

if [[ -z "$FW" ]]; then
  if [[ -f build/XCORE407I_FIRMWARE.bin ]]; then
    FW=build/XCORE407I_FIRMWARE.bin
  elif [[ -f build-target/XCORE407I_FIRMWARE.bin ]]; then
    FW=build-target/XCORE407I_FIRMWARE.bin
  else
    echo "[flash_dfu] ERROR: no firmware path given and no default binary found" >&2
    exit 2
  fi
fi

if [[ ! -f "$FW" ]]; then
  echo "[flash_dfu] ERROR: firmware binary $FW not found" >&2
  exit 2
fi

if ! command -v dfu-util >/dev/null 2>&1; then
  echo "[flash_dfu] ERROR: dfu-util not in PATH" >&2
  exit 3
fi

[[ $QUIET -eq 1 ]] || echo "[flash_dfu] Firmware: $FW"
[[ $QUIET -eq 1 ]] || echo "[flash_dfu] Waiting for DFU device (0483:df11) up to $RETRIES s..."

HAVE=0
for ((i=1;i<=RETRIES;i++)); do
  if dfu-util -l 2>/dev/null | grep -qi '0483:df11'; then
    HAVE=1; break
  fi
  sleep 1
done

if [[ $HAVE -ne 1 ]]; then
  cat <<EOF >&2
[flash_dfu] ERROR: DFU device not detected.
Hints:
  1. Runtime DFU: send protected ENTER_DFU opcode (will reboot into ROM DFU).
  2. Hardware: Set BOOT0=1 (jumper), RESET low->high, ensure USB cable stable.
  3. If it appears briefly then vanishes: try a different cable/port (power).
  4. Check dmesg:   dmesg | tail -n 30
EOF
  exit 4
fi

[[ $QUIET -eq 1 ]] || echo "[flash_dfu] DFU device detected. Flashing..."

# Compute binary size and supply explicit length to avoid dfu-util misinterpreting size
BIN_SIZE=$(stat -c %s "$FW")
BIN_SIZE_HEX=$(printf '0x%X' "$BIN_SIZE")
[[ $QUIET -eq 1 ]] || echo "[flash_dfu] Binary size: $BIN_SIZE bytes ($BIN_SIZE_HEX)"

TMPLOG=$(mktemp -t dfu_flash_XXXX.log)

flash_with_len() {
  set +e
  (dfu-util -a 0 -s 0x08000000:${BIN_SIZE_HEX}:leave -D "$FW" 2>&1 | tee "$TMPLOG")
  RC=$?
  set -e
}

flash_simple() {
  set +e
  (dfu-util -a 0 -s 0x08000000:leave -D "$FW" 2>&1 | tee "$TMPLOG")
  RC=$?
  set -e
}

# First try with explicit length
flash_with_len

if ! grep -q 'File downloaded successfully' "$TMPLOG"; then
  if grep -qi 'not writeable' "$TMPLOG" || grep -qi 'Last page' "$TMPLOG"; then
    [[ $QUIET -eq 1 ]] || echo "[flash_dfu] WARN: Length attempt failed, retrying without explicit length..."
    : > "$TMPLOG"
    flash_simple
  fi
fi

if grep -q 'File downloaded successfully' "$TMPLOG"; then
  [[ $QUIET -eq 1 ]] || echo "[flash_dfu] SUCCESS: Firmware written."
  rm -f "$TMPLOG"
  if [[ $WAIT -gt 0 ]]; then
    [[ $QUIET -eq 1 ]] || echo "[flash_dfu] Waiting $WAIT s for reboot..."
    sleep $WAIT
  fi
  exit 0
fi

echo "[flash_dfu] dfu-util exit code: $RC" >&2
sed -n '1,160p' "$TMPLOG" >&2
echo "[flash_dfu] Hints: Try power cycle + BOOT0 high again, or use ST-Link if persistent." >&2
rm -f "$TMPLOG"
exit $RC
