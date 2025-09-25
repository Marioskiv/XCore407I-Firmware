# Contributing Guide

Thank you for your interest in contributing to the XCore407I Remora Firmware.

## Build Overview

Toolchain: `arm-none-eabi-gcc` (Cortex-M4F hard-float). Minimum CMake 3.16.

```bash
mkdir build && cd build
cmake -DREMORA_STRICT=ON ../targets/TARGET_XCORE407I
cmake --build . -j
```
Artifacts: `XCORE407I_FIRMWARE`, `XCORE407I_FIRMWARE.bin`, `XCORE407I_FIRMWARE.map`.

## Strict Mode / Warnings
`REMORA_STRICT=ON` (default) elevates warnings to errors for project sources while suppressing vendor HAL noise. Keep builds warning-free; PRs introducing new warnings will be requested to fix them.

## Code Style
A project `.clang-format` is provided. Please format changed files:

```bash
clang-format -i <file.c>
```

## Protocol Evolution Rules

* Protocol is append-only per version: never reorder or shrink existing fields.
* Extensions for v4 live after the base `crc32` and are gated by negotiation.
* New telemetry appended inside the extension block SHOULD increment its `extLen` but MUST preserve prior ordering.
* Authentication framing: protected opcodes include an 8-byte truncated HMAC-SHA256 tag after opcode/value.

## Authentication

* HMAC key presently static (compile-time or default). Future work may add persistent configuration.
* Truncated tag (8 bytes) balances bandwidth and basic integrity. Do **not** expand without measuring packet size impact.

## Static Analysis

Use helper script:

```bash
./scripts/run_static_analysis.sh
```
Review `cppcheck-report.txt` and `clang-tidy-report.txt` for actionable warnings (excluding vendor code).

## Tests

Host-side Python tests (`tests/`) cover:

* Jog ramp progression logic approximation.
* Packet layout size expectations (base vs extension).

Add new tests for serialization or motion algorithms when changing related code.

Run all:

```bash
python3 tests/test_jog_ramp.py
python3 tests/test_packet_layout.py
```

## GitHub Actions CI (Overview)

Workflow (see `.github/workflows/build.yml` once added) will:

1. Checkout
2. Install toolchain & python deps
3. Build firmware (`REMORA_STRICT=ON`)
4. Run tests
5. Run static analysis
6. Upload binary/map as artifacts

## Commit Messages

Use conventional style where possible:

```text
feat(protocol): add negotiated telemetry extension
fix(motion): prevent limit overshoot edge-case
```


## Pull Request Checklist

* [ ] Build passes (strict)
* [ ] Tests updated / added
* [ ] Static analysis issues reviewed / addressed
* [ ] Protocol doc updated if wire format changes
* [ ] No unrelated formatting churn

## Questions

Open a Discussion or issue for design proposals (e.g., new opcodes, security model changes, persistent key provisioning).

Happy hacking!
