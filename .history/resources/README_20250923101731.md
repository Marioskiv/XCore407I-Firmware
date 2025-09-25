# Hardware & Software Resource Archive

This directory tree organizes essential and optional resources for the XCore407I (STM32F407IGT6) bring-up and ongoing development.

## Structure
resources/
  schematic/              - Board schematic (PDF) and PCB drawings
  manuals/                - Board user manual, application notes
  datasheets/             - MCU datasheet, reference manual, peripheral PHY docs
  tools/                  - Utilities (CubeMX installer, ST-LINK utilities, network debug tools)
  drivers/                - ST-LINK USB driver, VCP driver
  examples/               - Optional example code, demo firmware, 3D models, middleware packs

## Acquisition Notes
Some items require accepting STMicroelectronics or third‑party license terms and cannot be auto-downloaded blindly. The helper script (fetch_resources.sh) uses placeholders or comments where manual intervention is needed.

## Priority Classification
High Priority: Must have for bring-up
Medium Priority: Helpful during debug / integration
Optional: Reference or legacy material

Refer to `manifest.json` for a machine-readable list of expected files and source URLs.
