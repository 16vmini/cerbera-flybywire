# Cerbera fly-by-wire throttle

A drop-in replacement for the awful pedal-box-to-AJP cable run on the Cerbera.
The ITBs stay mechanically synced through their existing shared linkage — we
only replace the cable between **pedal** and **linkage** with **wires +
controller + servo**. Sometimes called "cable-output DBW" or "Bowden DBW".

## Goals

- Bin the original cable run (tight bulkhead penetration, sticky in use).
- Replace pedal-to-linkage cable with **6 wires** through the existing grommet.
- Keep the existing mechanical linkage between the ITBs (already sync'd).
- Add as side-effects: cruise control, programmable throttle map, anti-stall.
- Default-closed failsafe — anything goes wrong, return spring closes throttle.

## Architecture

```
 cabin                                              engine bay
+----------------+      6 wires        +-------------------------+
| DBW pedal      | ─────────────────▶  | Motor driver + servo    |
| (dual Hall)    |   2 signals + V+G   | (drives cable to ITBs)  |
+----------------+                     +-------------------------+
        ▲                                          ▲
        │                                          │ position feedback
        │                                          │ (pot or encoder)
        │              CABIN                       │
        │       +-----------------+                │
        └─────▶ | Pico controller | ───────────────┘
                |  + safety MCU   |   PWM + dir
                +-----------------+
```

- **Pedal:** Audi A4 B7 2.0 TDI pedal assembly, part `8E2721523J`. Bosch
  dual-Hall, steel arm we can cut and weld to fit the Cerbera pedal box,
  documented pinout, ~£30 on eBay. See [HARDWARE.md](HARDWARE.md).
- **Controller:** single Raspberry Pi Pico (RP2040) **mounted in the cabin**,
  not the engine bay. RP2040 is rated to +85°C with non-automotive-grade
  passives; engine bay temps regularly exceed that. Cabin under-dash is
  fine. Core 0 runs the PID loop, Core 1 the safety watchdog + cross-check.
  Four safety layers (mechanical spring + motor driver enable behaviour +
  hardware watchdog + software cross-check). A hardware LM393 comparator
  was considered as a 5th layer but dropped — software is sufficient for
  the application. See [SAFETY.md](SAFETY.md).
- **Servo:** **Bosch DV-E5 ETB** (VW/Audi 2.0T or BMW N52 donor). Don't
  disassemble — bolt a lever onto the external end of the throttle shaft
  and run a cable from the lever to the ITB linkage. Full sweep ~80–120 ms
  (cruise actuators do ~250 ms — too slow for a real throttle). Built-in
  2-track Hall position sensor lets the Pico cross-check ETB position the
  same way it cross-checks the pedal — extra hardware safety layer for
  free. See [HARDWARE.md](HARDWARE.md). The Subaru cruise actuator stays
  in the pile for the bench rig + future cruise control project.
- **Motor driver:** Pololu G2 / Cytron / Toshiba TB67H — anything that
  goes high-impedance when its enable pin floats. The safety MCU can yank
  enable to fail-safe in <1 ms.
- **Return spring:** strong, on the ITB linkage end. Default state = closed.

## Mounting (where each part lives)

| Part | Location | Why |
|---|---|---|
| **Pedal** | Cabin, replacing existing throttle pedal | Donor pedal hangs from the firewall like the original |
| **Controller (both Picos)** | Cabin, under the dash | RP2040 not rated for engine-bay temps; cabin is cool and dry |
| **USB tuning port** | Cabin, panel-mount under dash | So a laptop can plug in without taking the dash apart |
| **Motor driver (Pololu G2)** | Engine bay, inner wing on small heatsink | Pulls high current; needs ventilation, not engine heat |
| **Servo (cruise actuator)** | Engine bay, **chassis-mounted** (bulkhead or inner wing) — NOT bolted to the engine | Engine vibration + 200 °C heat-soak kills connectors; chassis mount is cool & rigid; short Bowden absorbs engine-rocking motion to the linkage |
| **Bowden cable** | Engine bay, ~150–200 mm from servo to existing ITB linkage shaft | Short = stiff = accurate; long enough to let engine rock on its mounts without binding |
| **Return spring** | On the ITB linkage end | Default state = closed; ALL failsafes assume this spring is there and strong |

## Build plan

1. **Sourcing** — pedal, MCU pair, motor driver, ETB scrap, connectors. See
   [HARDWARE.md](HARDWARE.md).
2. **Bench rig** — pedal + controller + ETB on the kitchen table, no car. Get
   the PID loop tracking pedal position cleanly before anything goes near the
   Cerbera. Demonstrate failsafes (yank pedal sensor wire, kill power to
   primary MCU, etc.) all return throttle to closed.
3. **Cerbera install** — pedal box bracket, cable routing, ETB mount, wiring.
4. **Drive-train tune** — pedal map (soft first 20%, linear after), cruise.

## Code

- **[firmware/](firmware/README.md)** — Pico firmware (Arduino-pico + PlatformIO).
  Dual-core: Core 0 = PID loop, Core 1 = safety + USB CDC tuning protocol.
  Boot self-check, latched faults, hardware watchdog.
- **[tuner/](tuner/README.md)** — Single-file WebSerial tuning page. Open in
  Chrome / Edge, plug Pico into laptop, calibrate + arm + run bench tests.

## Next step → [FIRST-FLASH.md](FIRST-FLASH.md)

Step-by-step from "PlatformIO installed" to "tuner showing live sensor needles" —
build, flash, connect, calibrate, set the throttle map. Read this when you sit
down to do the bench rig.

## Wiring → [BENCH-WIRING.md](BENCH-WIRING.md)

Pin-by-pin guide for plugging the Audi pedal + Z18XER ETB directly into the
Pico for sensor-only bench testing (no motor driver yet). Powers the sensors
from the Pico's 3.3 V rail so no voltage dividers are needed for first light.

## Status

- [x] Architecture agreed: cable-output DBW with Pico controller in cabin
- [x] Pedal: Audi A4 B7 `8E2721523J` (~£30)
- [x] Controller: 1× Raspberry Pi Pico (USB-C) (~£5) — software cross-check, no LM393
- [x] Motor driver: Pololu G2 18v17 (~£25)
- [x] Servo bench unit: Vauxhall Z18XER throttle body (Bosch `0280750036`), £20 — incoming
- [ ] Servo final unit (post bench-rig): clean VW/Audi 2.0 TFSI `06F133062A` if Z18XER turns out fine, or upgrade if needed
- [x] Servo Plan B: Subaru cruise actuator `87012FE020` (~£40) — kept for future cruise control project
- [x] Mounting locations agreed (servo chassis-mounted, not engine-mounted)
- [x] Firmware v0.1 scaffolded (dual-core PID + USB CDC + boot self-check) — see [firmware/](firmware/)
- [x] WebSerial tuner v0.1 scaffolded (live telemetry + calibration + ARM/sweep) — see [tuner/](tuner/)
- [ ] Buy remaining parts (Pololu G2, pigtails)
- [ ] Bench rig with all 10 failsafe tests passing ([SAFETY.md](SAFETY.md))
- [ ] WebSerial tuning page
- [ ] Install in car
