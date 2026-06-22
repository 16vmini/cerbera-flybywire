# Bench-rig wiring (sensors only — no motor driver yet)

For this phase: Pico + Audi pedal + Z18XER ETB on the kitchen table.
Goal: prove all four sensors read correctly, calibrate them, set the
throttle map curve. The Pololu G2 motor driver isn't here yet, so the
motor side is untouched.

We power the sensors from the **Pico's 3V3(OUT) pin**, not from 5 V.
Sensors are ratiometric, so they output 0 to (supply × travel-fraction).
Running them off 3.3 V means their signals max out at 3.3 V — within the
Pico's ADC range, no dividers needed.

⚠️ **Critical**: the RP2040 ADC pin absolute-maximum voltage is **~3.6 V
(Vdd + 0.3 V)** — feeding a higher voltage can damage the chip. A pedal
sensor on its FACTORY 5 V supply outputs **up to 4 V**, which is OUT OF
SPEC for a direct Pico ADC connection. Either power from 3.3 V (what this
bench rig does) OR use a 10 k / 22 k voltage divider on each signal line
before the ADC (production install). NEVER connect a 5 V-powered Bosch
sensor signal directly to a Pico ADC pin.

## Audi A4 B7 pedal — 6-way Bosch connector

**CONFIRMED via VAG workshop manual (2026-06-14) — sensor 1 working on
the bench rig.** Our first guess (without the OEM diagram) had it the
opposite of correct: pin 1 is the SUPPLY, not ground. Pin 5 is the
ground. Pin 6 (pure brown) is the actual sensor 1 signal. Verified by
seeing S1 read ~330 at rest and ~3200 at WOT, sweeping smoothly between.

| Pin | Colour | Function | Pico pin |
|---|---|---|---|
| 1 | **brown / green** | Sensor 1 +V supply | **3V3(OUT)** — pin 36 |
| 2 | **yellow / brown** | Sensor 2 +V supply | **3V3(OUT)** — pin 36 |
| 3 | **red / brown** | Sensor 2 ground | **GND** — pin 38 |
| 4 | **yellow / blue** | Sensor 2 signal | **GP27 / ADC1** — pin 32 |
| 5 | **grey / yellow** | Sensor 1 ground | **GND** — pin 38 |
| 6 | **brown** (pure, no stripe) | Sensor 1 signal | **GP26 / ADC0** — pin 31 |

So **Sensor 1 = pins 1 (supply) + 5 (ground) + 6 (signal)** — outer pins.
And **Sensor 2 = pins 2 (supply) + 3 (ground) + 4 (signal)** — inner pins.

*Per the VAG workshop manual voltage table: pin 1 to GND measures 5 V on
the OEM 5 V supply (so pin 1 IS the supply); pin 1 to pin 5 also measures
5 V (so pin 5 is ground). Same logic puts pin 2 as supply and pin 3 as
ground. Signals on the remaining pins (4 and 6) follow the convention
that sensor 1 uses the outer set and sensor 2 the inner set.*

*Powered on 3.3 V from the Pico. Actual measured values on this Audi A4
B7 2.0 TDI pedal (this variant has sensor 2 as the WIDE-range sensor,
opposite of the typical Bosch convention — both still redundant, just
with different gains):*

| Sensor | Pin | Idle raw | WOT raw | Approx. voltage range |
|---|---|---|---|---|
| **S1** (narrow) | pin 6, brown | 315 | 1906 | 0.25 V → 1.54 V |
| **S2** (wide) | pin 4, yellow/blue | 611 | 3822 | 0.49 V → 3.08 V |

*Cross-check rule: S2 ≈ 2 × S1 at any pedal position (verified at idle:
611 / 315 = 1.94, at WOT: 3822 / 1906 = 2.00). Firmware normalises both
to 0–100 % via per-sensor calibration, then the cross-check just checks
that the normalised pedal positions agree. No firmware changes needed.*

## Vauxhall Z18XER ETB — 6-way Bosch DV-E5 connector

**Pinout FUNCTIONALLY CONFIRMED (2026-06-16) — sensor 1 wired and reading 713 closed → 3830 open on the bench rig. Supply, ground, and signal 1 all proven correct. Signal 2 (pin 4) inferred by elimination — not yet wired (the Pico's GP29 / ADC3 is hard-wired to internal VSYS sensing, so we can't read a 4th ADC channel without adding an ADS1115 over I²C). Production install will add the 4th channel if redundant cross-check is wanted.**

**This donor ETB's actual wire colours:**

Connector has pins **2, 4, 6 moulded on the bottom row**, with 1, 3, 5
implied on the top row in a Z pattern.

| Pin | Colour | Function | Pico pin |
|---|---|---|---|
| 1 | **blue** | Pot signal 1 | **GP28 / ADC2** (pin 34) |
| 2 | **black / yellow** | Pot +5 V supply | **3V3(OUT)** (pin 36) |
| 3 | **thick black / white** | Motor + | *not yet — waiting for Pololu G2* |
| 4 | **blue** (the other one) | Pot signal 2 | *unavailable — GP29 / ADC3 isn't broken out on the Pico; add an ADS1115 for redundancy if needed* |
| 5 | **thick brown / white** | Motor − | *not yet — Pololu G2* |
| 6 | **brown / white (thin)** | Pot ground | **GND** (pin 38) |

**This pinout is the Vauxhall Z18XER variant** — pins 3 + 5 are the motor
pair (notably thicker wires, carrying motor current), pins 1, 2, 4, 6 are
the sensor cluster (thin signal wires). Confirmed by Matt's observation
that pins 3 and 5 were the only thick wires in the harness, then verified
on the bench rig by wiring pins 1, 2 and 6 and watching the sensor respond
correctly (713 closed → 3830 open).

**Verify the motor pair before applying power:** multimeter on Ω, probes
on **pins 3 and 5** — should read 1–5 Ω (DC motor winding). Any other pin
combination should read open / very high impedance.

**Signal-1 vs signal-2 identification:** the two blue wires are the
redundant pair. Wire pin 1 → ADC2, pin 4 → ADC3. Power on, slowly rotate
the butterfly by hand. Whichever ADC channel reads UP smoothly is
"signal 1" (rising). The other is "signal 2" (falling, or half-scale
rising depending on the variant). Firmware doesn't care which is
physically which — calibration captures the resting + open values and
the rest is just maths.

## Quick sanity-check wiring

```
                                  ┌──────────────────┐
                                  │ Raspberry Pi Pico│
   Audi pedal pin 4 (S1 +V) ────► │ 3V3(OUT) — pin 36│
   Audi pedal pin 2 (S2 +V) ────► │                  │
   Z18XER ETB pin 3 (pot +V) ───► │                  │
                                  │                  │
   Audi pedal pin 1 (S1 GND) ───► │ GND — pin 38     │
   Audi pedal pin 6 (S2 GND) ───► │                  │
   Z18XER ETB pin 6 (pot GND) ──► │                  │
                                  │                  │
   Audi pedal pin 6 (S1 signal)─► │ GP26/ADC0 — pin 31│
   Audi pedal pin 4 (S2 signal)─► │ GP27/ADC1 — pin 32│
   Z18XER ETB pin 1 (pot S1)────► │ GP28/ADC2 — pin 34│
   Z18XER ETB pin 4 (pot S2)────► │ unavailable — GP29 isn't broken out
                                  │                  │
   USB-C ◄────────────────────────│ USB              │
                                  └──────────────────┘
```

## Before you power on

- All four signal wires should read between 0 V and 3.3 V at the Pico ADC
  pins with the pedal/throttle at rest. None should be at 5 V, none
  negative.
- Continuity check: signal grounds should be connected, not floating.
- Look for accidental shorts: the Audi connector pins are tight, a stray
  whisker between two terminals will short supply to signal and confuse
  you all afternoon.

## Reading them in the tuner

Once you flash the firmware below:

1. Plug Pico into laptop.
2. Open `tuner/index.html` in Chrome / Edge.
3. Click **Connect to Pico**, pick the new COM port.
4. The four dials (Pedal S1, Pedal S2, ETB pot 1, ETB pot 2) light up.
5. Press the pedal: S1 and S2 dials move together (one rising, the other
   following at half-scale).
6. Push the throttle butterfly by hand: ETB dials do the same.
7. If something doesn't move when you expect it to — swap that wire (pedal
   signal pins are mirror-image symmetric to ground/supply pins, easy to
   put backwards on a breakaway connector).

## After cal: production wiring delta

When the Pololu G2 arrives and we move to 5 V supply + LM393 cross-check:

- Pedal supply moves from Pico 3V3(OUT) to the 5 V automotive rail.
- Signal voltages now span up to 4 V, so each signal needs a voltage
  divider before reaching the Pico ADC: **10 k upper / 22 k lower** gives
  a ratio of 0.687 → 4 V scales to 2.75 V, well inside the 3.3 V ADC range.
- The LM393 reads the un-divided 5 V signals (it doesn't care about the
  Pico's 3.3 V world).
- ETB pot pin 5 (signal 2) moves from Pico ADC3 to the LM393 only — Pico
  doesn't need to see it because LM393 enforces cross-check in hardware.
  ADC3 then frees up for the Pololu G2's current-sense pin.

For now: bench-rig phase, all four signals into the Pico, no dividers, no
LM393. Validate the sensors, build calibration confidence.
