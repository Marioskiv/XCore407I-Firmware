#!/usr/bin/env python3
"""Generate multi-profile pin map CSV comparing default, FSMC-safe, SDIO-safe.
Parses minimal static dictionaries; extend to parse headers if needed later.
"""
import csv, sys, pathlib

# Base header mapping table for relevant pins (subset)
HEADER_PINS = {
    'PA4':'7','PA6':'9','PA8':'11','PA9':'12','PA15':'18','PB4':'23','PB5':'24','PB6':'25','PB7':'26','PB8':'27','PB9':'28','PB11':'30','PB12':'31','PB13':'32','PB14':'33','PC6':'41','PC7':'42','PC8':'43','PC9':'44','PC10':'45','PC11':'46','PC12':'47','PD0':'51','PD1':'52','PD2':'53','PD3':'54','PD8':'59','PE0':'67','PE1':'68','PB2':'21'
}

# Logical signals of interest
signals = [
    ('X_STEP','PB6'),('X_DIR','PB7'),('X_ENA','PD0'),('X_ALM','PE0'),
    ('Y_STEP','PB8'),('Y_DIR','PB9'),('Y_ENA','PD1'),('Y_ALM','PE1'),
    ('Z_STEP','PA6'),('Z_DIR','PB14'),('Z_ENA','PD2'),('Z_ALM','PE2'),
    ('A_STEP','PC6'),('A_DIR','PC7'),('A_ENA','PD3'),('A_ALM','PE3'),
    ('ESTOP','PE4'),('PROBE','PG0'),
]

# Profile remaps
FSMC_SAFE = {
    'X_ENA':'PA4','Y_ENA':'PA15','Z_ENA':'PC8','A_ENA':'PC9',
    'X_ALM':'PC10','Y_ALM':'PC11','Z_ALM':'PC12','A_ALM':'PD8',
    'ESTOP':'PB2','PROBE':'PC8'
}
SDIO_SAFE = {
    # stays default intentionally
}

rows = [("Signal","Default MCU","Default Header","FSMC-Safe MCU","FSMC-Safe Header","SDIO-Safe MCU","SDIO-Safe Header")]
for sig, default_pin in signals:
    fsmc_pin = FSMC_SAFE.get(sig, default_pin)
    sdio_pin = SDIO_SAFE.get(sig, default_pin)
    rows.append((sig,
                 default_pin, HEADER_PINS.get(default_pin,''),
                 fsmc_pin, HEADER_PINS.get(fsmc_pin,''),
                 sdio_pin, HEADER_PINS.get(sdio_pin,'')))

out = pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'pin_map_profiles.csv')
with out.open('w', newline='') as f:
    w = csv.writer(f)
    w.writerows(rows)
print(f"Wrote {out}")
