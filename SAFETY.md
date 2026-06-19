# Safety — failsafes

The throttle is the one thing where "controller crashed" must equal "engine
returns to idle." This document is the contract every part of the design has
to meet.

## Hard rule

**Any single fault must result in the throttle returning to its closed
(idle) position.** That includes:

- Primary MCU hang / power loss / brown-out
- One pedal sensor disagreeing with the other
- Servo position sensor unplugged or stuck
- Motor driver fault (overheat, short, undervoltage)
- Wiring loom break (any wire, anywhere)

## The architecture that delivers it

```
       pedal sensor 1 ──┐                ┌── pedal sensor 1 (parallel)
       pedal sensor 2 ──┼──▶ PRIMARY ────│
                        │    PICO        │    SAFETY PICO ◀───── pedal sensor 2 (parallel)
                        │                │
                        │  PWM + DIR     │  enable line
                        │  to motor      │  to motor driver
                        │  driver        │  (active-high; floats to disable)
                        ▼                ▼
                  ┌──────────────────────────┐
                  │  Motor driver (Pololu G2)│
                  │  inputs: PWM, DIR, !EN   │
                  └────────────┬─────────────┘
                               │
                               ▼
                    DC motor + gearbox
                    (ex-Bosch ETB)
                               │
                               ▼
                       cable + linkage ─── return spring (mechanical)
```

## Layers

1. **Mechanical return spring** on the linkage. Default state = closed. If
   the motor driver goes high-impedance, the servo offers no resistance and
   the spring pulls the throttle closed.

2. **Motor driver enable line** held high by the **safety MCU**. Anything
   that pulls it low (or the line floats) disables the driver in < 1 ms.

3. **Software cross-check on Core 1** of the Pico. The dual-core RP2040
   runs the PID loop on Core 0 and the safety logic on Core 1 — they're
   independent CPUs sharing one chip. Core 1 reads both pedal sensors,
   verifies them against the cross-check rule (after normalisation, both
   should report the same pedal % within ±5%), and drops the motor driver
   enable line if they diverge.

4. **(Optional, dropped from MVP)** A hardware-only LM393 comparator was
   designed as a 5th layer — pure analog cross-check on the two pedal
   signals, with output directly to the motor driver's enable line. Not
   built. Can be retrofitted later if the safety case ever demands it.

5. **Hardware watchdog** (RP2040 has one built in). Main loop kicks the
   watchdog every iteration; if the loop hangs the chip resets within
   100 ms. On reset, all GPIO defaults back to high-impedance, motor driver
   enable line floats low, spring closes throttle. *(See `setup()` in
   `firmware/src/main.cpp` — `watchdog_enable(100, 1)`.)*

   *(Mark's review, 2026-06-07: explicitly noted as a must-have. Confirmed
   present in firmware v0.2.)*

6. **Brown-out detector** on the 5 V regulator. Below 4.5 V the primary
   resets and re-runs its boot self-check (pedal closed at idle, both
   sensors within spec, servo position sensor reads zero before enabling the
   motor driver).

7. **Position cross-check at startup.** Before enabling the driver: read
   the servo position sensor — it must read "closed" (matches the spring's
   resting state). Read the pedal — it must be at or near zero. Refuse to
   arm if either is wrong.

8. **Motor outputs forced to zero before pinMode flip.** Mark's review
   2026-06-07: the firmware sets the motor PWM duty cycle to 0 and the
   motor enable line to LOW *before* configuring the pins as outputs, so
   the tiny window between power-on and the first control loop pass cannot
   ever produce a momentary half-second of motor drive. See `setup()` in
   `firmware/src/main.cpp`.

9. **"Wait politely" arm refusal.** Mark's review 2026-06-07: if the
   throttle is sat open (> 10 % of its closed reference) when the operator
   tries to ARM, the firmware *does not* drive the motor and *does not*
   latch a fault — it just refuses to enable the driver with the reason
   "waiting for throttle to be closed". Same for pedal not at idle. The
   motor driver stays disabled (spring keeps throttle closed) until both
   conditions are met, then ARM works on the next try. Hard sensor faults
   (pedal cross-check failure, motor driver /FAULT) still latch as before.
   See `safe_to_arm()` in `firmware/src/main.cpp`.

8. **Loom redundancy.** Pedal sensors 1 and 2 are wired through SEPARATE
   plugs to the controller box (not via the same connector), so a single
   contact failure can't take both signals out.

## What we deliberately do NOT do

- **Don't share signal ground with chassis.** Bosch sensors use a dedicated
  signal ground that goes back to the controller; chassis-grounding causes
  noise on the position signal.

- **Don't trust software-only failsafes.** The enable line and the spring
  are hardware-defaulted-safe. Software watchdogs are an extra layer, not
  the primary.

- **Don't drive the motor at full PWM duty at startup.** Ramp the duty cycle
  up over 100 ms so a wiring fault that would cause runaway gets caught by
  the position cross-check before the motor wins against the spring.

## Bench-rig test plan (before the car)

Every one of the following must demonstrably return the throttle to closed:

- [ ] Unplug primary Pico mid-drive
- [ ] Unplug safety Pico mid-drive
- [ ] Cut sensor 1 wire mid-drive
- [ ] Cut sensor 2 wire mid-drive
- [ ] Short sensor 1 to +5 V mid-drive
- [ ] Short sensor 1 to ground mid-drive
- [ ] Drop supply voltage from 5.0 V to 3.5 V (brown-out)
- [ ] Yank motor power mid-drive
- [ ] Unplug servo position sensor mid-drive
- [ ] Hold pedal at WOT, kill primary's heartbeat in software

Only when all 10 demonstrably idle the throttle does the rig get fitted to
the car.
