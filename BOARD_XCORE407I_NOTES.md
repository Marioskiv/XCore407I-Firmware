# XCore407I Remora Firmware Board Notes

## Clock Configuration
- Default external HSE crystal: 8 MHz (per most board spins). Some variants populate 25 MHz.
- Select at configure time with CMake option `-DBOARD_HSE_FREQ_MHZ=8` or `25` (only these two values accepted).
- PLL strategy: VCO input fixed at 1 MHz (PLLM = HSE MHz). PLLN=336, PLLP=2 -> SYSCLK 168 MHz. PLLQ=7 -> 48 MHz USB.
- Runtime banner (`[CLK] ...`) prints detected configuration for validation without instruments.

## Flashing
- Linker places vectors at `0x08000000` (internal FLASH). Generated `.hex` uses the same base; program with ST-LINK / CubeProgrammer directly.
- Trimmed binary `_flash.bin` is suitable for raw flashing if required.

## Ethernet (RMII) DP83848I
- Fixed pin mapping:
  - REF_CLK: PA1
  - MDIO:    PA2
  - CRS_DV:  PA7
  - MDC:     PC1
  - RXD0:    PC4
  - RXD1:    PC5
  - TX_EN:   PB11
  - TXD0:    PB12
  - TXD1:    PB13
- PHY RESET_N tied to MCU NRST (no dedicated GPIO reset).
- No PHY INT line routed (autoneg & link status polled via MDIO).
- Default strapped PHY address expected: 0x01. Firmware scans all addresses and adopts first responsive.
- UART command `phyaddr` scans addresses 0..3 live and will auto-switch if current address is silent.

## USB OTG FS (Device, CDC)
- D+ / D- on PA12 / PA11.
- MIC2075 high-side switch present for VBUS sourcing in host/OTG scenarios; for pure device (self‑ or bus‑powered) the enable is still driven high so FLG status is monitored.
  - EN (VBUSEN / OTG_PWR_OUT): PC1
  - FLG (OTG_U1_FLG):          PC2 (open-drain, active low fault)
- Define / override via:
```
#define USB_FS_VBUS_SW_PRESENT 1
#define USB_FS_VBUS_EN_PORT GPIOC
#define USB_FS_VBUS_EN_PIN  GPIO_PIN_1
#define USB_FS_VBUS_FLG_PORT GPIOC
#define USB_FS_VBUS_FLG_PIN  GPIO_PIN_2
```
- Enumeration helpers: forced reconnect sequence if no SOF within ~1s.

## Persistent Network Configuration
- Stored near end of flash (sector containing configuration struct) with CRC32.
- Commands:
  - `ip?` current addressing
  - `setip A.B.C.D M.M.M.M G.G.G.G` sets static & disables DHCP (saves)
  - `cfgreset` restore defaults & save
  - `netstate` shows DHCP attempt/active/fallback state

## Diagnostic UART Commands (USART3 115200 8N1)
`help, phy?, phyaddr, mdio?, mdioclr, ethrst, ethloop, ethtest, phyid, board?, ethforce, status, ip?, cfgreset, setip, version?, netstate`

## Build Options (CMake)
- `BOARD_HSE_FREQ_MHZ` : 8 (default) or 25
- `BOARD_ENABLE_ETH` (default 1) / `BOARD_ENABLE_ULPI` (default 0) mutually exclusive due to PB12/PB13 sharing.
- `BOARD_ENABLE_NAND`, `BOARD_ENABLE_SDIO` (disabled by default; enable only if hardware populated and motion pin conflicts resolved).

## Safety & Future
- If NAND enabled, remap motion ENA lines (`MOTION_PROFILE_FSMC_SAFE`).
- Consider adding FreeRTOS if task separation required; current stack uses NO_SYS LwIP.

## Quick Bring-Up Checklist
1. Configure correct HSE: pass `-DBOARD_HSE_FREQ_MHZ=8` (or 25) and build.
2. Flash `.hex` to 0x08000000.
3. Open UART @115200: observe `[CLK]` banner and USB / NET diagnostics.
4. Connect Ethernet; watch for `[NET]` lines and link up transitions.
5. Use `phyaddr` to ensure PHY detected; if mismatch it auto-selects.
6. If USB not enumerating: verify DP (PA12) ~3.3V after attach, check FLG (PC2) not low (fault).

---
Generated automatically to reflect current firmware state (Sep 2025).
