# Project Setup & Onboarding

This guide walks through cloning, initializing required submodules, building, and troubleshooting the XCore407I firmware.

## 1. Prerequisites

Install:
- Git (>= 2.25 recommended for sparse / partial clone features)
- CMake (>= 3.20)
- ARM GNU Toolchain (e.g. `arm-none-eabi-gcc` 10+)
- Make or Ninja (preferred)

Optional (development convenience):
- VS Code + C/C++ extensions
- OpenOCD or ST-Link CLI for flashing/debugging

Verify toolchain:
```bash
arm-none-eabi-gcc --version
cmake --version
```

## 2. Clone Repository

Full clone with every submodule (≈56 from STM32CubeF4):
```bash
git clone --recurse-submodules <repo_url> XCore407I-Firmware
cd XCore407I-Firmware
```

Already cloned without submodules?
```bash
cd XCore407I-Firmware
git submodule update --init --recursive
```

## 3. Minimal Submodule Initialization (Recommended for Faster Clone)

Only three submodules are required for this firmware build:
- `st/STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx`
- `st/STM32CubeF4/Drivers/STM32F4xx_HAL_Driver`
- `st/STM32CubeF4/Middlewares/Third_Party/LwIP`

If you performed a plain (non-recursive) clone:
```bash
git submodule update --init st/STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx \
  st/STM32CubeF4/Drivers/STM32F4xx_HAL_Driver \
  st/STM32CubeF4/Middlewares/Third_Party/LwIP
```

Helper script (does the same):
```bash
./scripts/init_minimal_submodules.sh
```

Add more later as needed (example FreeRTOS):
Using an existing local STM32CubeF4 instead of submodule
If you already have STM32CubeF4 downloaded (e.g. at `C:\Users\mario\OneDrive\Desktop\STM32CubeF4-master\STM32CubeF4-master`), you can point CMake to it to avoid cloning the submodule:

```powershell
cmake -S . -B build -DCUBE_ROOT="C:/Users/mario/OneDrive/Desktop/STM32CubeF4-master/STM32CubeF4-master"
cmake --build build -j
```

Note: The path must contain `Drivers/` and `Middlewares/` folders.

```bash
git submodule update --init st/STM32CubeF4/Middlewares/Third_Party/FreeRTOS
```

## 4. Optional: Sparse Clone (Space Efficient)
```bash
git clone --filter=blob:none <repo_url> XCore407I-Firmware
cd XCore407I-Firmware
git sparse-checkout init --cone  # Git 2.25+
# Optionally restrict to essential paths (advanced users)
```

## 5. Configure & Build

Out-of-source build:
```bash
mkdir -p build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/arm-gcc-stm32.cmake -DTARGET=XCORE407I ..
cmake --build . -j
```

Resulting artifacts (paths may vary by target):
- Firmware ELF: `XCORE407I_FIRMWARE.elf`
- Intel Hex: `XCORE407I_FIRMWARE.hex`
- Binary: `XCORE407I_FIRMWARE.bin`

## 6. Flashing (Examples)

ST-Link CLI:
```bash
st-flash write XCORE407I_FIRMWARE.bin 0x08000000
```

OpenOCD (adjust interface/cfg):
```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program XCORE407I_FIRMWARE.elf verify reset exit"
```

## 7. Network Configuration

The firmware configures Ethernet (RMII) + LwIP (NO_SYS) and exposes a custom Remora UDP protocol. Ensure your host is on the same subnet; default static IP (see code / README). Use a UDP client to send motion commands & retrieve feedback.

## 8. Troubleshooting

| Issue | Likely Cause | Resolution |
|-------|--------------|-----------|
| Missing `stm32f4xx_hal.h` | HAL submodule not initialized | Run minimal init or helper script |
| Undefined CMSIS symbols | CMSIS Device not initialized | Initialize CMSIS Device submodule |
| LwIP headers missing | LwIP submodule not initialized | Initialize LwIP submodule |
| Linker errors (vector table) | Wrong target or missing startup file | Pass `-DTARGET=XCORE407I` to CMake |
| Ethernet no link | PHY pins or clock config mismatch | Check board wiring & RMII clock; verify DP83848 strap | 
| UDP no responses | Firewall or IP mismatch | Disable host firewall / confirm IP addressing |
| Fault latched | Alarm inputs asserted | Clear hardware condition then use CLEAR_FAULTS opcode |
| E-Stop engaged | E-Stop pin low | Release E-Stop; device must re-enable motion |

## 9. Updating Submodules
```bash
git submodule update --remote --recursive
```
Commit the updated submodule SHAs to lock versions.

## 10. Cleaning Build
```bash
rm -rf build
```
Reconfigure and rebuild per section 5.

## 11. Contribution Notes
- Keep jog / motion separation (see `jog.c` after refactor).
- Avoid initializing unused heavy middleware to keep build lean.
- Add new hardware features behind clear modules and document pin usage.

## 12. Next Steps
After initial build & flash, test:
1. Power board & observe link LEDs.
2. Confirm UDP feedback packets (fault mask, estop, jog inputs).
3. Exercise jog inputs and verify acceleration behavior.

---
For deeper architecture and protocol details, see `README.md`.
