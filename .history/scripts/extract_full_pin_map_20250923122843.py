#!/usr/bin/env python3
"""Produce a comprehensive pin map CSV including motion, encoders, ethernet, and safety signals.

Columns:
  Signal,Category,Default MCU,Default Header,FSMC-Safe MCU,FSMC-Safe Header,SDIO-Safe MCU,SDIO-Safe Header

Motion ENA/ALM/E-Stop/Probe vary by motion profile; STEP/DIR and encoders are invariant.
Ethernet signals currently invariant with respect to motion profiles (TX_EN variant controlled by USE_PG11_TXEN, not handled here).

If you later add profile-dependent Ethernet differences, extend the extraction accordingly.
"""
from __future__ import annotations
from pathlib import Path
import csv
import re

HEADER_FILE = Path('Remora-OS6/Firmware/FirmwareSource/Remora-OS6/TARGET_XCORE407I/board_xcore407i.h')

# Header pin index map (subset used earlier; extend if needed)
HEADER_PINS = {
    'PA4':'7','PA6':'9','PA8':'11','PA9':'12','PA15':'18',
    'PB2':'21','PB3':'22','PB4':'23','PB5':'24','PB6':'25','PB7':'26','PB8':'27','PB9':'28','PB11':'30','PB12':'31','PB13':'32','PB14':'33',
    'PC6':'41','PC7':'42','PC8':'43','PC9':'44','PC10':'45','PC11':'46','PC12':'47',
    'PD0':'51','PD1':'52','PD2':'53','PD3':'54','PD8':'59',
    'PE0':'67','PE1':'68',
    # Add more as required
}

MOTION_SIGNALS = [
    ('X_STEP','MotionStep'),('Y_STEP','MotionStep'),('Z_STEP','MotionStep'),('A_STEP','MotionStep'),
    ('X_DIR','MotionDir'),('Y_DIR','MotionDir'),('Z_DIR','MotionDir'),('A_DIR','MotionDir'),
    ('X_ENA','MotionEnable'),('Y_ENA','MotionEnable'),('Z_ENA','MotionEnable'),('A_ENA','MotionEnable'),
    ('X_ALM','MotionAlarm'),('Y_ALM','MotionAlarm'),('Z_ALM','MotionAlarm'),('A_ALM','MotionAlarm'),
    ('ESTOP','Safety'),('PROBE','Probe')
]

ENCODERS = [
    ('ENC0_CH1','Encoder'),('ENC0_CH2','Encoder'),
    ('ENC1_CH1','Encoder'),('ENC1_CH2','Encoder'),
    ('ENC2_CH1','Encoder'),('ENC2_CH2','Encoder')
]

ETHERNET = [
    ('ETH_REF_CLK','Ethernet'),('ETH_MDIO','Ethernet'),('ETH_CRS_DV','Ethernet'),
    ('ETH_MDC','Ethernet'),('ETH_RXD0','Ethernet'),('ETH_RXD1','Ethernet'),
    ('ETH_TX_EN','Ethernet'),('ETH_TXD0','Ethernet'),('ETH_TXD1','Ethernet')
]

ALL_SIGNALS = MOTION_SIGNALS + ENCODERS + ETHERNET

# Simple regex to capture ETH pin macros if we wanted dynamic parsing
ETH_DEFINE_RE = re.compile(r'^#define\s+(ETH_[A-Z0-9_]+)_PORT\s+GPIO([A-Z])')
ETH_PIN_RE    = re.compile(r'^#define\s+(ETH_[A-Z0-9_]+)_PIN\s+GPIO_PIN_(\d+)')

def parse_board_header(path: Path):
    lines = path.read_text(encoding='utf-8', errors='ignore').splitlines()
    # Reuse logic from prior extraction by a lightweight manual parse
    # We'll just look for unconditional STEP/DIR definitions + conditional enables.
    content = '\n'.join(lines)

    def find_pin(name: str) -> tuple[str|None,str|None]:
        # Look for last occurrence of name_PORT / name_PIN macro (#define <NAME>_PORT GPIOX)
        port_pattern = re.compile(rf'#define\s+{name}_PORT\s+GPIO([A-Z])')
        pin_pattern  = re.compile(rf'#define\s+{name}_PIN\s+GPIO_PIN_(\d+)')
        port = None
        pin = None
        for m in port_pattern.finditer(content):
            port = m.group(1)
        for m in pin_pattern.finditer(content):
            pin = m.group(1)
        return port, pin

    # Motion profile specific: replicate logic from extract_pin_map.py by calling it directly if available.
    # To avoid duplication, import the existing module if on path.
    from importlib import util
    profile_map = None
    try:
        spec = util.spec_from_file_location('extract_pin_map', 'scripts/extract_pin_map.py')
        mod = util.module_from_spec(spec)
        assert spec.loader
        spec.loader.exec_module(mod)  # type: ignore
        base_ports, base_pins, ports_over, pins_over = mod.parse(path)  # type: ignore
        def merge(ports_over_r, pins_over_r):
            return mod.merge_variant(base_ports, base_pins, ports_over_r, pins_over_r)  # type: ignore
        default_map = merge(ports_over['DEFAULT'], pins_over['DEFAULT'])
        fsmc_map = merge(ports_over['FSMC'], pins_over['FSMC'])
        sdio_map = merge(ports_over['SDIO'], pins_over['SDIO'])
        profile_map = (default_map, fsmc_map, sdio_map)
    except Exception as e:
        raise SystemExit(f"Failed to leverage extract_pin_map.py: {e}")

    rows = [("Signal","Category","Default MCU","Default Header","FSMC-Safe MCU","FSMC-Safe Header","SDIO-Safe MCU","SDIO-Safe Header")]

    def hdr(pin: str|None):
        return HEADER_PINS.get(pin, '') if pin else ''

    default_map, fsmc_map, sdio_map = profile_map

    # Motion + safety signals come from profile maps
    for sig, cat in MOTION_SIGNALS:
        d = default_map.get(sig,'')
        f = fsmc_map.get(sig,'')
        s = sdio_map.get(sig,'')
        rows.append((sig,cat,d,hdr(d),f,hdr(f),s,hdr(s)))

    # Encoders: Use find_pin (not profile dependent)
    encoder_lookup = {
        'ENC0_CH1':('PB4'), 'ENC0_CH2':('PB5'),
        'ENC1_CH1':('PA8'), 'ENC1_CH2':('PA9'),
        'ENC2_CH1':('PA15'),'ENC2_CH2':('PB3'),
    }
    for sig, cat in ENCODERS:
        p = encoder_lookup.get(sig,'')
        rows.append((sig,cat,p,hdr(p),p,hdr(p),p,hdr(p)))

    # Ethernet (parse or static mapping)
    eth_map = {
        'ETH_REF_CLK':'PA1','ETH_MDIO':'PA2','ETH_CRS_DV':'PA7','ETH_MDC':'PC1','ETH_RXD0':'PC4','ETH_RXD1':'PC5','ETH_TX_EN':'PB11','ETH_TXD0':'PB12','ETH_TXD1':'PB13'
    }
    for sig, cat in ETHERNET:
        p = eth_map.get(sig,'')
        rows.append((sig,cat,p,hdr(p),p,hdr(p),p,hdr(p)))

    return rows

def write_csv(rows, out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open('w', newline='') as f:
        w = csv.writer(f)
        w.writerows(rows)

def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('--header', default=str(HEADER_FILE))
    ap.add_argument('--out', default='scripts/pin_map_full.csv')
    args = ap.parse_args()
    header = Path(args.header)
    if not header.exists():
        raise SystemExit(f"Header file not found: {header}")
    rows = parse_board_header(header)
    out_path = Path(args.out)
    write_csv(rows, out_path)
    print(f"Wrote {out_path} rows={len(rows)-1}")

if __name__ == '__main__':
    main()
