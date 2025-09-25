#!/usr/bin/env bash
# Robust DFU flash helper for XCORE407I-Firmware
# Usage:
#   bash scripts/check_and_flash.sh            # Just flash (requires board already in DFU)
#   bash scripts/check_and_flash.sh --verify   # Flash + read-back compare
#   bash scripts/check_and_flash.sh --wait 15  # Wait up to 15s for DFU to appear
# Options can be combined (order agnostic).
set -euo pipefail

WAIT_SECS=10
DO_VERIFY=0
FIRMWARE="build/XCORE407I_FIRMWARE.bin"

# If a non-option argument is provided and points to an existing .bin, use it
for arg in "$@"; do
  case "$arg" in
    --verify|--wait|--wait=*) ;;
    *)
      if [ -f "$arg" ]; then
        FIRMWARE="$arg"
      fi
    ;;
  esac
done

for arg in "$@"; do
  case "$arg" in
    --verify) DO_VERIFY=1 ;;
    --wait=*) WAIT_SECS="${arg#*=}" ;;
    --wait) shift; WAIT_SECS="${1:-10}" ;;
  esac
done

if [ ! -f "$FIRMWARE" ]; then
  echo "[error] Firmware binary not found at $FIRMWARE. Build first (cmake --build build or supply path)." >&2
  exit 2
fi

vidpid="0483:df11"
echo "[info] Waiting for DFU device ($vidpid) up to $WAIT_SECS s ..."
FOUND=0
for ((i=0;i<WAIT_SECS;i++)); do
  if lsusb | grep -qi "$vidpid"; then
    FOUND=1; break
  fi
  sleep 1
done
if [ $FOUND -ne 1 ]; then
  echo "[error] DFU device not detected. Steps:" >&2
  echo "  1. Set BOOT0=1 and press RESET" >&2
  echo "  2. Or send protected ENTER_DFU opcode from running firmware (when network works)" >&2
  echo "  3. Reconnect USB / try another port" >&2
  exit 3
fi

echo "[info] DFU device present. Listing:" 
if ! dfu-util -l | sed 's/^/[dfu] /'; then
  echo "[warn] dfu-util listing returned non-zero (continuing)" >&2
fi

echo "[flash] Flashing $FIRMWARE ..."
if ! bash "$(dirname "$0")/flash_dfu.sh"; then
  echo "[error] Flash script failed." >&2
  exit 4
fi

if [ $DO_VERIFY -eq 1 ]; then
  echo "[verify] Performing read-back compare..."
  SIZE=$(stat -c%s "$FIRMWARE")
  dfu-util -a 0 -s 0x08000000:$SIZE -U /tmp/readback.bin
  if cmp -s /tmp/readback.bin "$FIRMWARE"; then
    echo "[verify] MATCH"
  else
    echo "[verify] DIFFER!" >&2
    hexdump -C /tmp/readback.bin | head -n 5 >&2
  fi
fi

echo "[post] If BOOT0=1 set it back to 0 and reset the board to run firmware." 
echo "[post] To confirm exit from DFU after reset: lsusb | grep -i df11 (should be empty)." 
