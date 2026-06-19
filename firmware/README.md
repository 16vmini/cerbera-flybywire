# Firmware — flybywire throttle controller

Target: **Raspberry Pi Pico (RP2040)**, single-board, both cores in use.
Framework: **arduino-pico** by Earle Philhower, built via **PlatformIO**.

> ⚡ **First time?** See [../FIRST-FLASH.md](../FIRST-FLASH.md) for the
> end-to-end walkthrough from "PlatformIO installed" to "tuner showing live
> needles".

## Build & flash (Windows)

1. Install **VS Code** + the **PlatformIO** extension.
2. Open this folder (`flybywire/firmware`) in VS Code. PlatformIO will install
   the toolchain on first open (~5 min, one-time).
3. With the Pico unplugged, hold **BOOTSEL** on the board, plug it in. It
   shows up as a USB drive (`RPI-RP2`).
4. In VS Code, hit the **→** "Upload" arrow in the PlatformIO toolbar.
5. The Pico reboots into the firmware; the drive disappears and a new COM
   port shows up. That's the USB CDC serial Trevor — sorry, the tuner —
   will talk to.

Subsequent uploads don't need BOOTSEL — the Pico reboots itself into the
bootloader when PlatformIO uploads. If that ever fails, BOOTSEL + replug is
the backstop.

## What's where

| File | Purpose |
|---|---|
| `platformio.ini` | Build target, framework, USB descriptors |
| `src/config.h` | Pin map, loop rates, default thresholds, default PID gains |
| `src/main.cpp` | Boot self-check, PID loop (Core 0), safety + USB protocol (Core 1) |

## USB protocol

Line-oriented ASCII at 115200 baud. One command per line, one response.

| Send | Response | Meaning |
|---|---|---|
| `PING` | `PONG` | Liveness |
| `VERSION` | `V flybywire 0.1` | |
| `ARM` | `OK armed` / `ERR ...` | Enable motor driver if boot-safe |
| `DISARM` | `OK disarmed` | Drop enable, keep config |
| `CLEARFAULT` | `OK` | Manually clear latched fault |
| `SAVE` | `OK saved` / `ERR save` | Persist config to LittleFS |
| `STREAM ON\|OFF` | `OK` | Start/stop telemetry stream |
| `GET <param>` | `<param>=<value>` | Read a config item |
| `SET <param> <value>` | `OK` / `ERR ...` | Update a config item |
| `TEST pedal` | streamed `T s1=… s2=… etb=…` lines | 5 s of raw ADC capture for cal |
| `TEST sweep` | `OK swept` | Open/close ETB at 50 % duty. **Engine OFF only.** |

Telemetry stream (when STREAM is ON, ~50 Hz):

```
T pedal=0.421 target=0.180 etb=0.175 i=1.32 armed=1 fault=ok
```

Config parameters currently supported via `GET`/`SET`:
`kp`, `ki`, `kd`,
`pedal_s1_min`, `pedal_s1_max`, `pedal_s2_min`, `pedal_s2_max`,
`etb_pot_closed`, `etb_pot_open`.

## Boot self-check

Before the firmware will let `ARM` succeed, the following all have to be true:

1. Pedal sensors cross-check within tolerance (`|S1 − S2/0.5| < 5 %`).
2. Pedal reads inside the idle window (5 % – 20 % of calibrated range).
3. ETB position reads within ±10 % of its calibrated closed reference.
4. Pololu G2 `/FAULT` line is high (no over-current, no thermal trip).

Boot self-check is replayed every time you send `ARM` — so if anything has
since drifted (sensor unplugged, ETB stuck open) the firmware refuses to
arm and tells you why.

## Runtime safety

Three software trips on top of the LM393 hardware cross-check + spring:

- **Continuous pedal cross-check.** Every control loop pass. Trips → fault.
- **G2 /FAULT line.** Sampled every pass. Goes low → trips fault.
- **Watchdog.** RP2040 hardware watchdog set to 100 ms. If the loop hangs,
  the chip resets, motor driver enable floats, spring closes throttle.

A latched fault means `ARM` must be resent (and pass the boot check again)
to recover. Designed to be sticky so a transient glitch never silently
re-enables a throttle.

## Local serial monitor

If you want to talk to the Pico without using the WebSerial tuner page:

```
PlatformIO → "Serial Monitor" with the right COM port.
Send PING, expect PONG.
Send STREAM ON, watch telemetry roll past.
```
