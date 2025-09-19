# XCore407I Remora Firmware

Production-ready firmware for the STM32F407 (XCore407I) implementing:

- 168 MHz system clock (PLL from 8 MHz HSE)
- STM32CubeF4 HAL (minimal modules: RCC, GPIO, ETH, DMA, TIM, FLASH, PWR, CORTEX)
- RMII Ethernet (DP83848 or compatible PHY) with lwIP (NO_SYS=1)
- Remora UDP protocol (port 27181) with unicast feedback & sequence gap detection
- Motion control hooks (step generator + encoder feedback)
- Diagnostics counters (Ethernet RX, link events, Remora sequence gaps)
- Complete newlib syscall stubs

## Directory Highlights

- `targets/TARGET_XCORE407I/` – Target specific build, linker script, port glue
- `Remora-OS6/Firmware/.../lwip/` – Network interface (`ethernetif.c`), Remora UDP (`remora_udp.c`)
- `st/hal_conf/stm32f4xx_hal_conf.h` – Pruned HAL configuration
- `syscalls.c` – Minimal libc syscalls

## Build

Requires `arm-none-eabi-gcc`, CMake, and Make.

```bash
mkdir -p targets/TARGET_XCORE407I/build
cd targets/TARGET_XCORE407I/build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

Resulting artifacts:

- ELF: `XCORE407I_FIRMWARE`
- Binary: `XCORE407I_FIRMWARE.bin`
- Map: `XCORE407I_FIRMWARE.map`

## Submodules

This project embeds the upstream `STM32CubeF4` distribution, which itself references many (≈56) Git submodules. They cover:

* Core device support (CMSIS device headers)
* HAL driver sources
* Board Support Packages (Nucleo, Discovery, Eval boards)
* Peripheral component drivers (sensors, codecs, Ethernet PHYs, displays, etc.)
* Middleware (FreeRTOS, LwIP, FatFs, USB Host/Device, mbedTLS, LibJPEG)

For a minimal build of this firmware you only need these submodules initialized:

* `Drivers/CMSIS/Device/ST/STM32F4xx`
* `Drivers/STM32F4xx_HAL_Driver`
* `Middlewares/Third_Party/LwIP`

All other submodules are optional unless you plan to enable their corresponding features.

### Clone With All Submodules

```bash
git clone --recurse-submodules <your_fork_url_or_origin>
```

If you already cloned without them:

```bash
git submodule update --init --recursive
```

### Initialize Only Required Submodules (Minimal Build)

From the repository root:

```bash
git submodule update --init st/STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx \
  st/STM32CubeF4/Drivers/STM32F4xx_HAL_Driver \
  st/STM32CubeF4/Middlewares/Third_Party/LwIP
```

You can add others later, for example FreeRTOS or FatFS:

```bash
git submodule update --init st/STM32CubeF4/Middlewares/Third_Party/FreeRTOS
git submodule update --init st/STM32CubeF4/Middlewares/Third_Party/FatFs
```

To update everything to the referenced commits:

```bash
git submodule update --remote --recursive
```

### Why So Many Submodules?

STMicroelectronics decomposes the ecosystem into granular repositories so each peripheral driver / middleware stack can evolve independently and be reused across device families. This avoids monolithic updates when only a component changes.

### IDE Notes

Some IDEs (VS Code, CLion, Eclipse) do not automatically index unopened submodule folders. If you need code completion for a specific middleware, open a file inside that submodule or add it to your workspace explicitly.

### Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Missing `stm32f4xx_hal.h` | `STM32F4xx_HAL_Driver` submodule not initialized | Run minimal init command above |
| Undefined CMSIS symbols | CMSIS device submodule missing | Initialize `Drivers/CMSIS/Device/ST/STM32F4xx` |
| LwIP headers not found | LwIP submodule missing | Initialize `Middlewares/Third_Party/LwIP` |
| Build errors for USB / FatFS | Middleware not initialized | Init specific middleware or remove feature from build |

### Keeping Submodules Updated

To fetch the latest remote revisions (respecting the branches defined in `.gitmodules`):

```bash
git submodule update --remote --recursive
```

Lock a specific revision by committing the updated submodule SHA in the parent repository.

### Pruning Unused Submodules (Optional)

If storage is a concern, you can perform a sparse clone (Git 2.25+):

```bash
git clone --filter=blob:none --recurse-submodules <url>
```

Or remove unneeded working directories (NOT recommended if you may need updates later):

```bash
rm -rf st/STM32CubeF4/Middlewares/Third_Party/FreeRTOS
```

> Note: Removing a submodule folder without adjusting `.gitmodules` will cause `git status` to show modifications; prefer leaving them uninitialized instead of deleting.

### Quick Start Script & Extended Guide

For a streamlined minimal setup run:

```bash
./scripts/init_minimal_submodules.sh
```

See `SETUP.md` for a full onboarding walkthrough (toolchain prerequisites, build, flashing, troubleshooting).

## Jog Control Module

The jog system is modular (`jog.c` / `jog.h`) and provides per-axis acceleration ramps and telemetry.

API naming convention:

```c
void jog_init(void);
void jog_poll(void);
void jog_set_speed_axis(int axis, uint32_t steps_per_sec);
void jog_set_accel_axis(int axis, uint32_t steps_per_sec2);
void jog_set_speed(uint32_t steps_per_sec);   // applies to all axes
void jog_set_accel(uint32_t steps_per_sec2);  // applies to all axes
uint32_t jog_get_speed_axis(int axis);        // configured target
uint32_t jog_get_accel_axis(int axis);
uint32_t jog_get_current_speed_axis(int axis); // instantaneous ramped speed
```

Feedback telemetry now appends four `uint32_t` values (current per-axis jog speeds in steps/s) plus a `uint32_t` probe field at the end of the payload.

Feedback field order (after UDP header) – Protocol Version 2:

1. `jointFeedback[JOINTS]` (encoder counts)
2. `processVariable[JOINTS]`
3. `inputs` (legacy input bitfield)
4. `faultMask` (uint32)
5. `estop` (uint32, 0/1)
6. `jogSpeeds[4]` (current ramped speeds, steps/s)
7. `jogTargets[4]` (configured target speeds, steps/s)
8. `jogDirs[4]` (0 idle, 1 positive, 2 negative)
9. `probe` (uint32: 0 = triggered, 1 = not triggered)

### Protocol Version Negotiation

The `reserved` field in the UDP header carries `REMORA_PROTOCOL_VERSION` (currently `2`). A host must read this value before assuming the presence or ordering of extended fields (jogTargets, jogDirs, probe). Hosts encountering an unexpected version should either:

1. Fallback to a prior known parsing layout, or
2. Reject the packet and request a firmware update / compatibility mode.

No backward compatibility shim is provided inside the firmware; version increments indicate a structural change in payload layout.

Example host unpack (pseudo-C) for Protocol Version 2:

```c
uint32_t *p = (uint32_t*)payload; // after header
int idx = 0;
int32_t *jointFeedback = (int32_t*)&p[idx]; idx += JOINTS; // if int32 counts
int32_t *processVar    = (int32_t*)&p[idx]; idx += JOINTS;
uint32_t inputs        = p[idx++];
uint32_t faultMask     = p[idx++];
uint32_t estop         = p[idx++];
uint32_t jogSpeed[4];
for(int a=0;a<4;++a) jogSpeed[a] = p[idx++];
uint32_t probe         = p[idx++];
```

## Probe / Calibration Sensor

The firmware supports a single probe input for tool length, surface touch-off, or bed leveling workflows.

Pin assignment:

- `PROBE` = `PG0` (active low, internal pull-up enabled)

Electrical behavior:

- Idle (not triggered): input reads high -> feedback `probe = 1`.
- Triggered (contact closed to ground): input reads low -> feedback `probe = 0`.

Integration details:

- Probe state is a dedicated 32-bit field appended after jog speed telemetry in the feedback packet.
- Motion planner / host can poll this value each cycle for edge detection (watch for transition 1 -> 0).
- The probe is polled (no interrupt) to keep logic simple; typical cycle time (10 ms feedback) suffices for manual or slow probing. For high-speed probing, reduce feedback interval.

Example host probe edge detection snippet:

```c
static uint32_t lastProbe = 1; // assume not triggered
void handle_feedback(const uint8_t *buf){
  const RemoraUdpHeader *hdr = (const RemoraUdpHeader*)buf;
  const uint8_t *pl = buf + sizeof(RemoraUdpHeader);
  const uint32_t *u = (const uint32_t*)pl;
  int idx = 0;
  idx += JOINTS; // jointFeedback
  idx += JOINTS; // processVariable
  uint32_t inputs    = u[idx++];
  uint32_t faultMask = u[idx++];
  uint32_t estop     = u[idx++];
  uint32_t jogSpeed[4];
  for(int a=0;a<4;++a) jogSpeed[a] = u[idx++];
  uint32_t jogTarget[4];
  for(int a=0;a<4;++a) jogTarget[a] = u[idx++];
  uint32_t jogDir[4];
  for(int a=0;a<4;++a) jogDir[a] = u[idx++];
  uint32_t probe = u[idx++];
  if (lastProbe == 1 && probe == 0) {
    // probe just triggered
  }
  lastProbe = probe;
}
```

Safety interactions:

- Probe reporting is suppressed only by hardware failure; E-Stop/fault do not mask the probe field (host may need last-known state during fault analysis).


## Flashing

Using OpenOCD (adjust interface if not ST-Link):

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program XCORE407I_FIRMWARE.bin 0x08000000 verify reset exit"
```

Or using `st-flash`:

```bash
st-flash --reset write XCORE407I_FIRMWARE.bin 0x8000000
```

## Network Configuration

Static IP is set in `lwip_port.c` (update as needed). Ensure your host is on the same subnet.

Default (example) configuration:

- IP: 192.168.2.50
- Netmask: 255.255.255.0
- Gateway: 192.168.2.1

## Remora UDP Protocol

Port: `27181`

Header (little endian):

```text
uint32_t magic  = 0x524D5241 ("RMRA")
uint32_t seq    = incrementing sequence
uint16_t length = payload bytes
uint16_t reserved = 0
```

Command Payload Layout (Tx Host -> MCU):

```text
int32_t  jointFreqCmd[JOINTS]
int32_t  setPoint[VARIABLES]
uint32_t jointEnable (bitmask)
uint32_t outputs (bitmask)
```

Feedback Payload Layout (MCU -> Host):

```text
int32_t  jointFeedback[JOINTS]
int32_t  processVariable[VARIABLES]
uint32_t inputs (bitmask)
uint32_t faultMask (bit0=X, bit1=Y, bit2=Z, bit3=A)
uint32_t estop (0 = normal, 1 = E-Stop latched)
```

The first valid command received establishes the unicast source (IP/port) used for all subsequent feedback packets. Broadcast is no longer used for feedback.

### Sequence Gap Detection

If an incoming sequence is not exactly `last+1` (and not the first packet), the internal gap counter increments. Retrieve via diagnostics API.

## Diagnostics

APIs:

- `const void *ethernetif_get_stats(void);` – Returns struct with: `rx_ok`, `rx_dropped`, `rx_alloc_fail`, `rx_errors`, `link_up_events`, `link_down_events`.
- `const struct remora_diag_stats *remora_get_diag(void);` – Returns: `last_rx_seq`, `seq_gap_events`.

(You can temporarily instrument prints or expose these over a debug interface.)

## Final Pin Map

### Motion Axes (X=0, Y=1, Z=2, A=3)

| Axis | STEP Pin (Timer/AF)        | DIR Pin | ENA Pin | ALM Pin (EXTI) | Encoder CH1 | Encoder CH2 | Encoder Timer |
|------|---------------------------|---------|---------|----------------|-------------|-------------|---------------|
| X    | PB6 (TIM4_CH1 AF2)        | PB7     | PD0     | PE0 (EXTI0)    | PB4         | PB5         | TIM3          |
| Y    | PB8 (TIM4_CH3 AF2)        | PB9     | PD1     | PE1 (EXTI1)    | PA8         | PA9         | TIM1          |
| Z    | PA6 (TIM3_CH1 AF2)        | PB14    | PD2     | PE2 (EXTI2)    | PA15        | PB3         | TIM2          |
| A    | PC6 (TIM8_CH1 AF3)        | PC7     | PD3     | PE3 (EXTI3)    | (unused)    | (unused)    | (future)      |

ENA outputs are driven high (active) at initialization. ALM inputs are configured with pull-ups and falling-edge EXTI interrupts (modify to suit driver polarity). Add fault handling inside `HAL_GPIO_EXTI_Callback()`.

### Ethernet RMII

| Signal  | Pin  |
|---------|------|
| REF_CLK | PA1  |
| MDIO    | PA2  |
| CRS_DV  | PA7  |
| MDC     | PC1  |
| RXD0    | PC4  |
| RXD1    | PC5  |
| TX_EN   | PG11 |
| TXD0    | PB12 |
| TXD1    | PB13 |
| PHY_RST | PG13 |

Pins PB12/PB13 reserved for RMII TX (they conflict with potential ULPI D5/D6 if USB HS ULPI were added; Ethernet prioritized).

### Notes

- Alarm ISR currently stubs fault handling; integrate motion abort logic as needed.
- Add a fourth encoder later (avoid PA1/PA7 due to RMII). Consider TIM9 on PE5/PE6 if free.
- Feedback now includes a 32-bit `faultMask`. Any asserted bit indicates motion halt & drivers disabled.

### Fault Handling (ALM Inputs)

ALM pins (PE0..PE3) assert a fault (falling edge). On detection the firmware:

1. Sets a per-axis bit in a global fault mask (bit 0=X, 1=Y, 2=Z, 3=A).
2. Disables all step timers (TIM4, TIM3, TIM8) halting motion immediately.
3. Drives all ENA outputs low, removing drive power (configure hardware so low = disabled).
4. Leaves the system in a latched fault state until reset (future enhancement: remote clear command).

Accessor: `uint32_t motion_get_fault_mask(void);`

Integrate reporting into Remora feedback by extending the payload or mapping bits into an inputs field as needed.

## Manual Control & E-Stop

### Emergency Stop (E-Stop)

- Pin: PE4 (EXTI4), active low with internal pull-up.
- On assertion: sets `estop_active`, stops all step timers, drives ENA outputs low, halts motion. Latched until reset (no clear command yet).
- Accessor: `uint32_t motion_get_estop(void);` returns 1 if e-stop has occurred.

### Jog Inputs

| Function | Pin  | Active Level |
|----------|------|--------------|
| +X       | PF0  | Low          |
| -X       | PF1  | Low          |
| +Y       | PF2  | Low          |
| -Y       | PF3  | Low          |
| +Z       | PF4  | Low          |
| -Z       | PF5  | Low          |
| +A       | PF6  | Low          |
| -A       | PF7  | Low          |

Behavior:
 
1. Inputs use pull-ups; grounding a pin activates the jog.
2. Fixed jog speed: `JOG_FIXED_FREQ` (default 1000 steps/s) applied while input held.
3. Direction pin forced accordingly before enabling step timer channel; negative jog sets DIR low.
4. Jogging inhibited if any fault (`faultMask != 0`) or E-Stop active.
5. Multiple simultaneous jog inputs on the same axis undefined; first matching condition in polling order wins.

Integration:
 
- `jog_poll()` is invoked each main loop iteration. Adjust rate or move to a periodic timer if tighter control needed.
- For smoother jog motion (acceleration profiles) replace direct `stepgen_apply_command()` usage with a ramp planner.

Safety Precedence: E-Stop > Alarm Fault > Jog > Host Command.

### Extended UDP Opcodes

Optional 8-byte extension (two uint32_t little-endian) may follow the base command payload:

| Opcode | Name              | Value Meaning          |
|--------|-------------------|------------------------|
| 0      | NOP               | Ignored                |
| 1      | CLEAR_FAULTS      | Value ignored          |
| 2      | SET_JOG_SPEED     | Steps/s target         |
| 3      | SET_JOG_ACCEL     | Steps/s^2 acceleration |

Behavior:
 
- CLEAR_FAULTS succeeds only if E-Stop not active. Returns to normal enabling (drivers re-enabled) but motion remains halted until host issues new freq commands or jog input.
- SET_JOG_SPEED & SET_JOG_ACCEL update internal parameters used by `jog_poll()` ramp logic.

### Jog Debounce & Acceleration

- Debounce: Each jog input must be stable active for `JOG_DEBOUNCE_COUNT` polls (default 5) before it is considered asserted.
- Ramp: A global jog speed ramps toward configured target using `jog_accel` (default `JOG_ACCEL_DEFAULT`). Approximate update: `speed += accel/1000 * delta_ms`.
- Direction changes immediately update DIR pin; ramp continues toward target speed.
- Host can dynamically adjust speed/accel via opcodes without resetting motion.


## Timing

`sys_now()` provided by `sys_port.c` wraps `HAL_GetTick()`; ensure SysTick runs at 1 kHz (default HAL init).

## Extending

- Enable zero-copy by mapping DMA Rx buffers directly into custom `pbuf`s.
- Add step pulse scheduling / acceleration planner.
- Implement persistent configuration storage (Flash sector).

## Testing Procedure

1. Flash firmware.
2. Connect board Ethernet to LAN (ensure link LEDs active).
3. From host, `ping <board-ip>` (should respond if ICMP enabled in lwIP build).
4. Send a valid Remora command packet (structure above). Example using Python (pseudo-code) to set joint 0 frequency:

```python
import socket, struct
JOINTS=4; VARS=4
magic=0x524D5241; seq=1
jointFreq=[1000,0,0,0]
setPoint=[0]*VARS
jointEnable=1
outputs=0
payload=struct.pack('<'+('i'*JOINTS)+('i'*VARS)+'II', *jointFreq,*setPoint,jointEnable,outputs)
hdr=struct.pack('<IIHH', magic, seq, len(payload), 0)
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
s.sendto(hdr+payload, ('192.168.2.50',27181))
# Receive feedback
data,_=s.recvfrom(512)
```

1. Inspect received feedback counts.
2. Send multiple packets with incrementing sequence; verify no gap events.

## License

Refer to upstream STM32CubeF4 licensing for HAL components. Project-specific additions released under your chosen terms.
