# Remora UDP Protocol

See README for overview; this file provides precise binary layout and version history.

Header (12 bytes LE):

```text
uint32 magic   0x524D5241
uint32 seq
uint16 payloadLen
uint16 version (REMORA_PROTOCOL_VERSION)
```


Protocol v2 Feedback Payload:

```text
int32  jointFeedback[4]
float  processVariable[4]
uint16 inputs
uint16 padding (if any)
uint32 faultMask
uint32 estop
uint32 jogSpeeds[4]
uint32 jogTargets[4]
uint32 jogDirs[4]
uint32 probe
```

Protocol v3 Extensions (appended):

```text
uint32 firmwareVersion  (MAJ<<24 | MIN<<16 | PATCH<<8 | proto)
uint32 buildHash        (truncated FNV1a of build date/time)
```

Protocol v4 Extensions (appended after v3):

```text
uint32 heartbeat       (increments each feedback frame)
uint32 uptimeMs        (HAL tick milliseconds)
uint32 statusFlags     (bitfield, see below)
uint32 seqGapEvents    (cumulative RX sequence gap count)
uint32 crc32           (CRC-32 (poly 0xEDB88320, init 0xFFFFFFFF, reflected) over all prior payload bytes for this version)
```

statusFlags bit mapping (v4):

```text
bit 0 : estop asserted (1 = estop active)
bit 1 : any motion fault present
bit 2 : inactivity failsafe tripped
bit 3 : homing sequence active
bit 4 : DHCP active (lease acquired)
bits8..11  : homedMask (axis 0..3) 1 = axis homed
bits12..15 : limitMask (axis 0..3) 1 = soft limit currently inhibiting motion
```

Bits not listed are reserved (report 0) and must be ignored by hosts (forward compatibility). Future expansions append new uint32 fields after crc32 with a version bump.

Never reorder/remove; only append and bump version.

## v4 Optional Telemetry Extension (Negotiated)

To preserve backward compatibility with existing v4 hosts, the firmware appends an OPTIONAL extension block *after* the base v4 `crc32` field. Legacy clients should stop parsing at the CRC using the `payloadLen` from the header. New clients MAY negotiate extended telemetry (enabled by default) or explicitly disable it.

Layout after base `crc32`:

```text
uint32 extLen          (number of bytes following; 0 means no extension present)
uint32 crcErrors       (currently counts prospective inbound CRC mismatches; reserved if RX CRC disabled)
uint32 authFailures    (failed or missing authentication tag on protected opcodes)
uint32 estopEdges      (number of rising edge events of the E-Stop input since power-on)
uint32 loopIntervalLast (ms between last two feedback packets)
uint32 loopIntervalMin  (minimum observed loop interval ms)
uint32 loopIntervalMax  (maximum observed loop interval ms)
uint32 reserved0
uint32 reserved1
```

If `extLen` is zero the remainder of the datagram ends immediately after that field. If non‑zero it MUST equal 8 * sizeof(uint32) (currently 32) for this revision. Future expansions may increase `extLen` while still allowing hosts to skip unknown trailing bytes.

### Negotiation Opcode

The host can send an optional negotiation command to explicitly enable/disable the extension block:
```text
opcode: REMORA_OPCODE_NEGOTIATE_EXT
value : bit0 = 1 enable extension (default), 0 disable (suppress extension, extLen field still present but = 0)
```
Negotiation affects all subsequent feedback frames for that client session (tracked by last sender IP/port). The device remembers only the last negotiated state; no timeout presently.

#### Authentication (HMAC) for Protected Opcodes

Critical opcodes (e.g. CLEAR_FAULTS, ENTER_DFU, future CONFIG writes) are guarded by a truncated HMAC-SHA256 (first 8 bytes). Current framing places the HMAC *immediately after* the opcode/value pair in the command packet. On authentication failure the protected action is not performed and `authFailures` increments. A future revision may formalize RX CRC + auth framing; current implementation is stable for these opcodes.

Security notes:
* Truncated 8-byte tag provides basic integrity against accidental or casual tampering; not a substitute for transport security.
* Shared secret key is currently static (set at build or via future config persistence).
* Hosts should rate-limit retries on failure to avoid incrementing `authFailures` excessively.

#### Host Parsing Guidance

1. Parse header; ensure `magic` and check `version` >= 4.
2. Read exactly `payloadLen` bytes following the header into a buffer.
3. Parse base v4 fields through `crc32`; verify CRC.
4. If bytes remain:
   * Read `extLen`.
   * If `extLen == 0`, stop.
   * If `extLen >= 32`, parse first 8 uint32 fields listed above; ignore any trailing bytes beyond known fields.

Hosts that do not implement extensions can ignore any surplus using `payloadLen` to skip.


## Control Opcodes Summary (current set)

```text
REMORA_OPCODE_CLEAR_FAULTS   (protected) value ignored
REMORA_OPCODE_SET_JOG_SPEED  value = steps/s target for all axes
REMORA_OPCODE_SET_JOG_ACCEL  value = steps/s^2 accel for all axes
REMORA_OPCODE_HOME_AXIS      value = axis index (0..3)
REMORA_OPCODE_ABORT_HOMING   value ignored
REMORA_OPCODE_SAVE_CONFIG    value ignored (persist to flash)
REMORA_OPCODE_LOAD_CONFIG    value ignored (reload from flash)
REMORA_OPCODE_NEGOTIATE_EXT  value bit0: 1 enable, 0 disable telemetry extension
REMORA_OPCODE_ENTER_DFU      (protected) value ignored; authenticated jump to ROM DFU bootloader

```
`ENTER_DFU` causes the firmware to deinitialize clocks/peripherals and jump to the STM32F4 system memory bootloader (USB FS DFU). Host should close its session; device will re-enumerate in DFU mode. Requires valid HMAC tag like other protected opcodes.
