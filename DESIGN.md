# Circuit design — flybywire v0.1

Engineer-to-engineer. Subsystems below; each gets an ASCII schematic + design
notes + signal levels. Resistor values are starting points — tune at bench
rig time once the actual pedal and servo are in front of us.

## System block diagram

```
                 ┌─────────────────────── CABIN (cool, dry, dash) ─────────────────────┐
                 │                                                                     │
   Audi A4 B7    │                                                                     │   Engine bay
   DBW pedal     │      ┌──── Pico (RP2040, USB-C) ──── USB to laptop (tuning) ──┐    │   ┌──────────────────┐
   ┌───────┐     │      │  Core 0: PID loop                                       │   │   │ Pololu G2 18v17  │
   │ S1 ───┼──── 6-way ─┼─────► ADC0 (S1 raw)                                     │   │   │  inputs:         │
   │ S2 ───┼──── Bosch  ┼─────► ADC1 (S2 raw)                                     │   │   │   PWM ◀──────┐   │
   │ +5V ──┼──── pigtail┼◀──── 5V_sense (regulated)                              │   │   │   DIR ◀─────┼┐  │
   │ GND ──┼────        ┼◀──── signal GND                                         │   │   │   /SLP ◀───┼┼─ wired-OR
   └───────┘     │      │  Core 1: safety + watchdog                              │   │   │   FAULT ────┼┼─► Pico
                 │      │     ◄──── LM393 fault line (also Pico GPIO output)      │   │   │   VIN ◀──── 12 V battery
                 │      │     PWM, DIR, /SLP outputs (3.3 V logic)                │   │   │   OUTA ────────┐ │
                 │      └─────────────────────────────────────────────────────────┘   │   │   OUTB ────────┼─┼─► Rostra
                 │                                                                     │   └────────────────┘ │   servo
                 │      ┌──── LM393 cross-check ────► /SLP wired-OR ───────────────────┼───────────────────────┘   motor
                 │      │  IN+/IN- on S1/2 and S2 with ±100 mV window                  │
                 │      └──────────────────────────────────────────────────────────────┘
                 │                                                                     │
                 │      ┌──── 12 V → 5 V DC-DC (Recom R-78E5 or Mark's spare) ────► +5 V rail to Pico, LM393, pedal
                 │      └──── TVS + polyfuse + 3 A blade fuse on 12 V input ──────────┘
                 │                                                                     │
                 └─────────────────────────────────────────────────────────────────────┘
                                            position feedback
                                  Rostra servo pot ────────────────────► Pico ADC2
```

## Power supply

```
  12 V battery (key-switched feed) ──┬── 3 A blade fuse ──┬── (Mark's) 12→5 V DC-DC ──┬── +5 V rail
                                     │                   │                            │
                                     ▼                   ▼                            ├── 100 nF + 10 µF decoupling
                              (Mark's TVS diode)     (ferrite bead)                    │
                                     │                                                 ├── Pico VSYS (via Pico's own LDO to 3.3 V)
                                     ▼                                                 ├── LM393 VCC
                                  chassis GND                                          └── Pedal sensor supply (via polyfuse 500 mA)

   Signal GND (separate net from chassis GND): one star point at the
   controller box, tied to chassis with a single short strap.
```

Notes:
- TVS clamps load-dump (Mark says he has a better part than my SMBJ16A spec — fine, just needs to clamp around 18 V to protect downstream).
- Polyfuse on the 5 V to the pedal saves the regulator if a sensor wire shorts to chassis.
- Single-point ground: signal GND of pedal returns to the controller, NOT to chassis. Bosch sensors are sensitive to ground bounce on the chassis.

## Pedal interface + LM393 cross-check

```
  Pedal connector
     S1 (0.4–4.0 V) ─────┬────────────────────────────── Pico ADC0
                         │
                         R1 10 k
                         │
                         ├── S1_half (= S1/2, 0.2–2.0 V) ─┬── LM393_A IN-
                         │                                 │
                         R2 10 k                           └── LM393_B IN+
                         │                                       (via offset)
                        GND

     S2 (0.2–2.0 V) ────────────────────────────────┬── Pico ADC1
                                                    │
                                                    ├── LM393_A IN+
                                                    │      (via offset)
                                                    │
                                                    └── LM393_B IN-

  +5 V ──── 5 k ──┬── 100 mV reference ──── used as both offsets via 100 k injection
                  │                          (or use simpler: shift one input
                  R 100 Ω                     leg with a small resistor + 100 µA
                  │                           current source from the 5 V via 50 k)
                 GND
```

Truth table the LM393 enforces (open-collector wired-OR, active LOW = fault):

| Condition | LM393_A out | LM393_B out | Wired-OR | Meaning |
|---|---|---|---|---|
| S1/2 ≈ S2 (within ±100 mV) | hi-Z | hi-Z | HIGH (pull-up) | Healthy → /SLP high → driver enabled |
| S1/2 > S2 + 100 mV | LOW | hi-Z | LOW | Sensor 1 reading too high — fault |
| S1/2 < S2 − 100 mV | hi-Z | LOW | LOW | Sensor 2 reading too high — fault |

Single-point of failure analysis:
- Pedal connector unplugged → both ADCs read 0 V (or float) → S2 reads 0, S1 reads 0, S1/2 = 0 ≈ S2 → LM393 doesn't trip BUT Pico sees both rails at 0 → software trips (refuses to arm motor driver if pedal not in valid idle window 0.3–0.5 V).
- One sensor wire cut → that ADC reads 0, the other reads its normal value → cross-check trips, LM393 pulls /SLP low, motor driver disables.
- 5 V supply lost to pedal → both sensors read 0 → same as unplugged → software trip.

## Motor driver (Pololu G2 18v17)

```
   Pico GPIO outputs (3.3 V logic)
     GP10 ─── PWM (~20 kHz) ────────► G2 PWM
     GP11 ─── DIR ──────────────────► G2 DIR
     LM393 wired-OR ────────┬───────► G2 /SLP (open-collector, pulled up to 5 V)
     GP12 ── (open-drain) ──┘
     GP13 ◄─── G2 /FAULT
     ADC3 ◄─── G2 CS (current sense, 30 mV/A typ.)

   12 V battery ──── 470 µF / 25 V bulk ────► G2 VIN
   G2 OUTA / OUTB ─── servo motor (Rostra Bowden actuator)
   G2 GND ──── chassis (high-current return; separate from signal GND)
```

Why /SLP wired-OR:
- LM393 pulls /SLP low on cross-check fault.
- Pico can ALSO pull /SLP low (open-drain GPIO) for software-detected faults
  (over-current via CS, position-sensor unplugged, watchdog timeout).
- Either source pulls the line low; both have to release for the driver to
  run. Belt + braces.

## Servo (Bosch DV-E5 ETB, shaft-lever mod) — feedback + dual-pot cross-check

Six wires. The ETB has a 2-track Hall position sensor (same Bosch signature
as the Audi pedal), so we get a **second LM393 cross-check** on the ETB's
own position — completely free safety layer.

```
   Pin 1  Motor +        ──── Pololu G2 OUTA
   Pin 2  Motor −        ──── Pololu G2 OUTB
   Pin 3  Pot supply +5V ──── controller 5 V rail
   Pin 4  Pot signal 1   ──── Pico ADC2 (PID feedback)  +  LM393_C IN−   (rising, 0.4–4.0 V)
   Pin 5  Pot signal 2   ──── LM393_D IN+ only          (half-scale, 0.2–2.0 V) — not read by Pico; LM393 does the cross-check in hardware
   Pin 6  Pot ground     ──── controller signal GND
```

LM393_C and LM393_D form the **same window comparator pattern** we use on
the pedal — except this time on the ETB's two pot tracks. Same ±100 mV
threshold (or whatever bench-rig validation lands on). Output of THIS LM393
pair is also wired into the /SLP wired-OR network — so any ETB-pot fault
disables the motor driver, exactly like a pedal fault would.

That means we now have **TWO LM393 packages on the board**, both feeding
the same /SLP wired-OR:

```
   pedal LM393  ──┐
                  ├── wired-OR ──── /SLP on Pololu G2  (active LOW = disable)
   ETB LM393   ──┤
                  │
   Pico GP12 ────┘  (open-drain, software-detected faults)
```

If any one source pulls /SLP low, the driver disables. None of them is
"controlling" the others — pure parallel hardware safety.

Servo position PID: target = f(pedal map, S1_pedal), actual = ADC2 (ETB
pot signal 1), PWM duty = PID(target − actual). Loop at ~200 Hz on Core 0.

Pre-arm software check (every boot):
- Pico samples both ETB pot channels. They MUST agree per the cross-check
  rule AND must read "closed" position (within ±5 % of rest-state value,
  recorded once at bench-rig time and stored in flash).
- Pico drives a tiny test pulse (5 % duty for 10 ms), watches ETB pot
  move, watches G2 current. Confirms the motor + driver + sensor chain is
  all live before allowing the main control loop to take over.

### Mechanical conversion (where this differs from a stock ETB)

We don't disassemble the ETB at all — keeping the bearings/seals aligned.
The throttle shaft has a stub poking out of the non-motor side of the
housing on most DV-E5 variants. Machine a flat (or drill for a roll pin)
and bolt a 30–50 mm steel lever to it. Cable runs from the lever's outer
end to the ITB linkage. The butterfly continues to rotate inside its
housing for no functional reason but harms nothing.

### Pin map delta

Update from earlier draft — added one ADC channel and removed the clutch
GPIO (we don't have a clutch any more; the Subaru's gone to Plan B duty):

| Pico GP | Function | Direction |
|---|---|---|
| GP28 / ADC2 | ETB pot signal 1 | In |
| GP29 / ADC4 | ETB pot signal 2 (new) | In |
| GP9 | ~~clutch FET~~ — removed | — |

## Pico pin map (RP2040)

| Pico GP | Function | Direction | Notes |
|---|---|---|---|
| GP26 / ADC0 | Pedal S1 raw | In | 12-bit ADC |
| GP27 / ADC1 | Pedal S2 raw | In | 12-bit ADC |
| GP28 / ADC2 | Servo position pot | In | 12-bit ADC |
| GP29 / ADC3 | Motor driver CS (current sense) | In | 12-bit ADC |
| GP10 | Motor driver PWM | Out | ~20 kHz, 8-bit duty |
| GP11 | Motor driver DIR | Out | 3.3 V logic |
| GP12 | Motor driver /SLP override | Open-drain out | Wired-OR with LM393 |
| GP13 | Motor driver /FAULT | In, pulled up | LOW when G2 has tripped |
| GP9  | Clutch solenoid enable (low-side FET) | Out | Subaru servo clutch — drops on any fault |
| GP14 | Heartbeat to status LED | Out | Blinks at 1 Hz when healthy |
| GP15 | Watchdog kick — internal | n/a | RP2040 has on-chip watchdog timer |
| GP0  | UART TX (debug) | Out | Optional secondary tuning route |
| GP1  | UART RX (debug) | In | Optional secondary tuning route |
| USB  | CDC serial (tuning) | I/O | Native RP2040 USB |

Cross-core comms: Core 0 writes telemetry to a shared RAM ring buffer; Core 1
reads it for the safety logic and feeds tuning replies back via USB CDC.

## Signal levels (cheat sheet)

| Signal | Range | Notes |
|---|---|---|
| Pedal S1 | 0.4 V (idle) – 4.0 V (WOT) | Bosch dual-Hall spec |
| Pedal S2 | 0.2 V (idle) – 2.0 V (WOT) | Half-scale of S1 |
| S1/2 vs S2 | should match ± 5 % | LM393 enforces ±100 mV |
| Servo pot | 0–5 V across mechanical travel | Tune endpoints in software |
| PWM to driver | 3.3 V logic, ~20 kHz | Audible bands avoided |
| Motor current | typ. 1–3 A, peak 8 A | G2 has 17 A headroom, plenty |
| /SLP healthy | HIGH (5 V via pull-up) | Active-low to disable |
| /FAULT to Pico | HIGH = OK, LOW = G2 tripped | Open-drain, pulled to 3.3 V |

## What needs validating at bench-rig time

- LM393 trip threshold (100 mV is a starting point — may need 150 mV in
  practice if the pedal sensors have more inherent noise than the spec sheet
  suggests).
- 20 kHz PWM is above audible — verify no whine. Move to 25 kHz if needed.
- Servo current draw under spring load (Rostra is a cruise actuator built
  for very light loads — fighting an ITB return spring may be harder; if
  current saturates, may need to gear up the cable side or pick a stronger
  servo).
- PID gains. Start with P-only, add I/D once stable.

## Open questions for Mark

1. Has he reverse-engineered any Cerbera engine-bay environmental data (peak
   temps under bonnet at the inner wing)? Useful for sanity-checking the
   motor-driver heatsink size.
2. Any of his ECU work involve the AJP8's existing throttle position sensor?
   If yes — useful to log against our servo position at install time.
3. Does he have a preferred connector standard he's already been using on
   the TVR work? Match it so we have one spares bin, not two.
