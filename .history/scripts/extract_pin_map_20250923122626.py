#!/usr/bin/env python3
"""
Automatic pin map extraction from `board_xcore407i.h`.

Parses the board header and produces a CSV with pin mappings for:
  - Default (no motion profile flags)
  - FSMC-Safe (MOTION_PROFILE_FSMC_SAFE=1)
  - SDIO-Safe (MOTION_PROFILE_SDIO_SAFE=1)

Approach:
The header uses #if MOTION_PROFILE_FSMC_SAFE / #elif MOTION_PROFILE_SDIO_SAFE / #else blocks
to override only a subset of pins (ENA, ALM, ESTOP, PROBE). STEP/DIR are unconditional.

Instead of invoking a full C preprocessor (which would require STM32 HAL include context),
we do a lightweight single-pass parse and track which conditional branch each define belongs to.

For each motion-profile conditional group we capture overrides separately, then merge:
  BASE (unconditional) -> Default   + overrides from DEFAULT branch (#else part)
  BASE (unconditional) -> FSMC-Safe + overrides from FSMC  branch (#if part)
  BASE (unconditional) -> SDIO-Safe + overrides from SDIO  branch (#elif part)

If a macro is defined multiple times inside the same branch we take the last one (typical C rule).

Output CSV columns:
  Signal,Default,FSMS-Safe,SDIO-Safe
with MCU pins in PORT+PIN format (e.g., PB6). Missing signals (not defined) left blank.

Extend easily to include header pin numbers by merging with a header dictionary or separate
JSON resource if needed.

Usage:
  python3 scripts/extract_pin_map.py \
      --header Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I/board_xcore407i.h \
      --out scripts/pin_map_extracted.csv

Defaults assume repository root execution. Paths auto-normalized.
"""
from __future__ import annotations
import re
import argparse
from pathlib import Path
from typing import Dict, Tuple

# Signals we care about (PORT+PIN pairs)
SIGNALS = [
    'X_STEP','Y_STEP','Z_STEP','A_STEP',
    'X_DIR','Y_DIR','Z_DIR','A_DIR',
    'X_ENA','Y_ENA','Z_ENA','A_ENA',
    'X_ALM','Y_ALM','Z_ALM','A_ALM',
    'ESTOP','PROBE'
]

BRANCH_FSMC = 'FSMC'
BRANCH_SDIO = 'SDIO'
BRANCH_DEFAULT = 'DEFAULT'  # the #else part in each group

DEFINE_PORT_RE = re.compile(r'^#define\s+([A-Z0-9_]+)_PORT\s+GPIO([A-Z])\b')
DEFINE_PIN_RE  = re.compile(r'^#define\s+([A-Z0-9_]+)_PIN\s+GPIO_PIN_(\d+)\b')
IF_FSMC_RE     = re.compile(r'^#if\s+MOTION_PROFILE_FSMC_SAFE')
ELIF_SDIO_RE   = re.compile(r'^#elif\s+MOTION_PROFILE_SDIO_SAFE')
ENDIF_RE       = re.compile(r'^#endif')
ELSE_RE        = re.compile(r'^#else')

def parse(header_path: Path):
    text = header_path.read_text(encoding='utf-8', errors='ignore').splitlines()
    # Store BASE unconditional values first
    base_ports: Dict[str,str] = {}
    base_pins: Dict[str,str]  = {}
    # Overrides per branch
    ports: Dict[str,Dict[str,str]] = {BRANCH_FSMC:{}, BRANCH_SDIO:{}, BRANCH_DEFAULT:{}}
    pins:  Dict[str,Dict[str,str]] = {BRANCH_FSMC:{}, BRANCH_SDIO:{}, BRANCH_DEFAULT:{}}

    # Track if we are inside a motion-profile conditional group.
    # Nested unrelated #if should be ignored; we only switch branch when patterns match.
    active_branch: str | None = None
    branch_stack = []  # track nested depth to know when to pop out

    for line in text:
        line_stripped = line.strip()

        # Detect branch starts
        if IF_FSMC_RE.match(line_stripped):
            branch_stack.append('MOTION_PROFILE')
            active_branch = BRANCH_FSMC
            continue
        if active_branch and ELIF_SDIO_RE.match(line_stripped):
            if branch_stack and branch_stack[-1] == 'MOTION_PROFILE':
                active_branch = BRANCH_SDIO
                continue
        if active_branch and ELSE_RE.match(line_stripped):
            if branch_stack and branch_stack[-1] == 'MOTION_PROFILE':
                active_branch = BRANCH_DEFAULT
                continue
        if ENDIF_RE.match(line_stripped):
            # Close branch if it was a motion profile group
            if branch_stack:
                branch_stack.pop()
                if not branch_stack:
                    active_branch = None
            continue

        # Parse defines
        m_port = DEFINE_PORT_RE.match(line_stripped)
        if m_port:
            name = m_port.group(1)
            port_letter = m_port.group(2)
            if name in SIGNALS:
                if active_branch:
                    ports[active_branch][name] = port_letter
                else:
                    base_ports[name] = port_letter
            continue
        m_pin = DEFINE_PIN_RE.match(line_stripped)
        if m_pin:
            name = m_pin.group(1)
            pin_num = m_pin.group(2)
            if name in SIGNALS:
                if active_branch:
                    pins[active_branch][name] = pin_num
                else:
                    base_pins[name] = pin_num
            continue

    return base_ports, base_pins, ports, pins

def merge_variant(base_ports, base_pins, ports_over, pins_over) -> Dict[str,str]:
    out = {}
    for sig in SIGNALS:
        p = base_ports.get(sig)
        n = base_pins.get(sig)
        if sig in ports_over:
            p = ports_over[sig]
        if sig in pins_over:
            n = pins_over[sig]
        if p and n:
            out[sig] = f"{p}{n}"
        else:
            # If either missing, leave blank (could indicate intentionally internal / unexposed)
            out[sig] = ''
    return out

def main():
    ap = argparse.ArgumentParser(description='Extract motion-related pin mappings from board_xcore407i.h')
    ap.add_argument('--header', default='Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I/board_xcore407i.h', help='Path to board_xcore407i.h')
    ap.add_argument('--out', default='scripts/pin_map_extracted.csv', help='Output CSV path')
    args = ap.parse_args()

    header_path = Path(args.header).resolve()
    if not header_path.exists():
        raise SystemExit(f"Header not found: {header_path}")

    base_ports, base_pins, ports_over, pins_over = parse(header_path)

    # Build variants
    default_map = merge_variant(base_ports, base_pins, ports_over[BRANCH_DEFAULT], pins_over[BRANCH_DEFAULT])
    fsmc_map    = merge_variant(base_ports, base_pins, ports_over[BRANCH_FSMC], pins_over[BRANCH_FSMC])
    sdio_map    = merge_variant(base_ports, base_pins, ports_over[BRANCH_SDIO], pins_over[BRANCH_SDIO])

    lines = ["Signal,Default,FSMC-Safe,SDIO-Safe"]
    for sig in SIGNALS:
        lines.append(f"{sig},{default_map[sig]},{fsmc_map[sig]},{sdio_map[sig]}")
    content = "\n".join(lines) + "\n"
    if args.out == '-':
        # Write to stdout
        print(content, end='')
        return
    out_path = Path(args.out).resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(content, encoding='utf-8')
    print(f"Wrote {out_path} (signals: {len(SIGNALS)})")

if __name__ == '__main__':
    main()
