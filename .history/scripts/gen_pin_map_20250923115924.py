#!/usr/bin/env python3
"""Generate a CSV pin map (firmware logical signals -> MCU pin -> header pin if present).
Reads minimal embedded tables here (kept in sync manually) to avoid parsing C headers.
Future improvement: parse board_xcore407i.h macros and schematic JSON automatically.
"""
import csv, sys, pathlib, json

# Minimal curated mapping (extend if needed)
MOTION = [
    ("X_STEP","PB6","25"),("X_DIR","PB7","26"),("X_ENA","PD0","51"),("X_ALM","PE0","67"),
    ("Y_STEP","PB8","27"),("Y_DIR","PB9","28"),("Y_ENA","PD1","52"),("Y_ALM","PE1","68"),
    ("Z_STEP","PA6","9"),("Z_DIR","PB14","33"),("Z_ENA","PD2","53"),("Z_ALM","PE2",""),
    ("A_STEP","PC6","41"),("A_DIR","PC7","42"),("A_ENA","PD3","54"),("A_ALM","PE3",""),
]
SAFETY = [
    ("ESTOP","PE4",""),("PROBE","PG0",""),
]
ENCODERS = [
    ("ENC0_CH1","PB4","23"),("ENC0_CH2","PB5","24"),
    ("ENC1_CH1","PA8","11"),("ENC1_CH2","PA9","12"),
    ("ENC2_CH1","PA15","18"),("ENC2_CH2","PB3","22"),
]
ETH = [
    ("ETH_REF_CLK","PA1","4"),("ETH_MDIO","PA2","5"),("ETH_CRS_DV","PA7","10"),
    ("ETH_MDC","PC1","36"),("ETH_RXD0","PC4","39"),("ETH_RXD1","PC5","40"),
    ("ETH_TX_EN","PB11","30"),("ETH_TXD0","PB12","31"),("ETH_TXD1","PB13","32"),
]

ALL = [("TYPE","SIGNAL","MCU_PIN","HEADER_PIN" )]
for group,rows in (
    ("MOTION",MOTION),("SAFETY",SAFETY),("ENCODER",ENCODERS),("ETH",ETH)
):
    for sig, mcu, hdr in rows:
        ALL.append((group,sig,mcu,hdr))

out_path = pathlib.Path("pin_map.csv") if len(sys.argv)<2 else pathlib.Path(sys.argv[1])
with out_path.open("w", newline="") as f:
    w = csv.writer(f)
    for row in ALL:
        w.writerow(row)
print(f"Wrote {out_path}")
