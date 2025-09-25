# XCore407I Schematic Cross‑Check and Pin Mapping

This document maps the firmware’s GPIO/peripheral assignments to the XCore407I schematics in `resources/schematic xcore407i/` and notes any conflicts or caveats.

Sources:
- `XCore407I-Schematic.pdf` (summarized JSON attachments)
- STM32F407IGT6 datasheet/reference manual

## Clocks and Power
- HSE: 8 MHz crystal on `PH0/PH1` → Used. Firmware config: `PLLM=8, PLLN=336, PLLP=2` → 168 MHz SYSCLK.
- LSE: 32.768 kHz on `PC14/PC15` → May be unpopulated on some boards. Firmware: LSE disabled, HSE-only.
- USB clock: `PLLQ=7` → 48 MHz for USB FS.
- Rails: `5V_USB` → `AMS1117-3.3` → `3V3` to MCU. `VDDA` decoupled (`3V3_A`). `VCAP1/2` 1V2 core caps present.

## USB FS (OTG_FS, Device Mode)
Schematic block: “USB FS OTG schematic”

- Connector: Mini‑USB device.
- Signals:
  - D+ → `PA12` (`USB_FS_DP`) with series 22 Ω → Configured as `AF10_OTG_FS`.
  - D− → `PA11` (`USB_FS_DM`) with series 22 Ω → `AF10_OTG_FS`.
  - VBUS → `PA9` (`OTG_FS_VBUS`) → Left as input; device stack has `vbus_sensing_enable = DISABLE` and uses manual connect/disconnect pulses.
  - ID → `PA10` → Left as input (no host mode).
- VID/PID: `0x0483:0x5740` (ST CDC ACM). Device strings report “XCore407I CDC”.
- Notes:
  - FS VBUS power switch `MIC2075-2` EN/FLG pins are present on schematic but not used by firmware (device mode doesn’t drive VBUS).

## USB HS (ULPI, USB3300)
Schematic block: “USB HS ULPI schematic”

- ULPI lines are routed to MCU but share pins with Ethernet TX:
  - ULPI_D5 `PB12` and ULPI_D6 `PB13` conflict with RMII `TXD0/TXD1`.
  - Firmware disables ULPI by default (`BOARD_ENABLE_ULPI=0`).
- If enabling ULPI, Ethernet must be disabled due to shared pins.

## Ethernet RMII (DP83848I)
Schematic block: “ETHERNET DP83848I schematic”

- PHY: DP83848I in RMII mode with 50 MHz crystal.
- MCU mapping (matches firmware):
  - `REF_CLK` → `PA1`
  - `MDIO` → `PA2`
  - `CRS_DV` → `PA7`
  - `MDC` → `PC1`
  - `RXD0` → `PC4`
  - `RXD1` → `PC5`
  - `TX_EN` → `PB11` (default). Optional variant `PG11` via `-DUSE_PG11_TXEN=1`.
  - `TXD0` → `PB12`
  - `TXD1` → `PB13`
  - `RESET_N` tied to MCU `NRST` in schematic → No independent GPIO reset.
  - `INT/PWRDOWN` not routed → Firmware leaves PHY interrupt unused.
- Link LEDs are on RJ45; firmware provides UART/link LED patterns for diagnostics.

## NAND Flash (K9F1G08U0C via FSMC, optional)
Schematic block: “NAND FLASH schematic”

- Address/Data:
  - D0..D7 → `PD14, PD15, PD0, PD1, PE7, PE8, PE9, PE10`
  - `NOE` → `PD4`, `NWE` → `PD5`
  - `CE#` (NE2) → `PG9`
  - `ALE` → `PD11` (A16), `CLE` → `PD12` (A17)
  - `R/B#` → `PD6` (NWAIT)
- Conflicts:
  - Motion enables default to `PD0/PD1/PD2/PD3`. With NAND enabled, `PD0/PD1` are used by D2/D3. Use `-DMOTION_PROFILE_FSMC_SAFE=1` to remap ENA lines off PD0/PD1.

## Motion IO (current default)
- Step/Dir:
  - X: `PB6` (STEP TIM4_CH1), `PB7` (DIR), ENA `PD0`
  - Y: `PB8` (STEP TIM4_CH3), `PB9` (DIR), ENA `PD1`
  - Z: `PA6` (STEP TIM3_CH1), `PB14` (DIR), ENA `PD2`
  - A: `PC6` (STEP TIM8_CH1), `PC7` (DIR), ENA `PD3`
- Encoders (example mapping avoiding RMII conflicts):
  - Enc0: `PB4/PB5` (TIM3)
  - Enc1: `PA8/PA9` (TIM1)
  - Enc2: `PA15/PB3` (TIM2)

## Network Configuration
- DHCP first, then static fallback if no lease within ~5 s:
  - Fallback IP: `192.168.2.50/24`, GW `192.168.2.1`.
- Services:
  - UDP Echo on port `5005` (stateless).
  - Remora UDP on port `27181` (starts feedback after first valid packet).

## Known Conflicts and Build‑time Options
- ULPI vs Ethernet TX (PB12/PB13): cannot be used simultaneously.
- NAND vs Motion ENA (PD0/PD1): enable `-DMOTION_PROFILE_FSMC_SAFE=1` or disable NAND.
- `USE_PG11_TXEN`: select TX_EN pin variant (default 0 = `PB11`).

## Quick Verification Checklist
1) USB: Enumerates as CDC ACM `0483:5740`.
2) Ethernet: Link up, obtain DHCP address; if not, fallback to `192.168.2.50`.
3) UDP Echo: `echo hello | ncat -u <IP> 5005` → same payload back.
4) Remora: Run `host/remora_client.py --ip <IP>` → feedback packets received.
