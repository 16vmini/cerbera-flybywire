# Hardware — sourcing notes

## Pedal — Audi A4 B7 2.0 TDI (Bosch DBW)

**Part: `8E2721523J`** — accelerator pedal assembly from the Audi A4 B7
(2005–2008), 2.0 TDI variant. Identical pedal module across the VAG family
(Golf Mk5, Audi A3 8P, etc.) — same part family that Mighty Car Mods, Link
ECU, Haltech and MaxxECU all document because it gets used so often.

Why it fits:
- **Steel pedal arm** — cut and weld to suit the Cerbera pedal box.
- **Hangs from the firewall** — matches the Cerbera's existing pedal-box
  mounting style.
- **Bosch dual Hall sensor** — two independent tracks, standard Bosch
  cross-check pattern (one rising 0.4 → 4.0 V, the other half-scale 0.2 → 2.0 V
  over pedal travel; controller refuses to open if they disagree).
- **6-way Bosch connector** — cheap pigtail kit available (~£15).
- **eBay UK breakers** — ~£20–40 second-hand.

Connector pinout (6-way Bosch, standard across VAG B7-era DBW pedals):

| Pin | Function | Notes |
|-----|----------|-------|
| 1 | Sensor 1 ground | tie to controller signal ground, NOT chassis |
| 2 | Sensor 2 supply (+5 V) | from controller |
| 3 | Sensor 2 signal | half-scale rising (~0.2 V idle → 2.0 V full) |
| 4 | Sensor 1 supply (+5 V) | from controller |
| 5 | Sensor 1 signal | full-scale rising (~0.4 V idle → 4.0 V full) |
| 6 | Sensor 2 ground | tie to controller signal ground, NOT chassis |

(Confirm against the actual donor pedal at install time — pin order varies by
year on some variants; check with a multimeter before powering up.)

Cross-check rule: at any pedal position, **sensor1 ≈ 2 × sensor2**. If they
diverge by more than ~5%, controller treats it as a fault and idles the
throttle.

## Microcontroller

One **Raspberry Pi Pico** (~£5), using both cores: Core 0 runs the PID loop;
Core 1 runs the safety cross-check and watchdog.

- **Mounted in cabin** (under dash, near pedal). RP2040 rated to +85 °C,
  engine bay exceeds that.
- Hardware watchdog (built-in) + brown-out detect (built-in).
- ADC: 12-bit, 4 channels — reads both pedal sensors + servo position.
- PIO state machine reads quadrature encoder if we go with one over a pot.
- **Native USB** for laptop tuning — no extra USB-serial chip needed.

### Why not two Picos, and why no LM393?

Earlier drafts had a second Pico as a safety watchdog, then evolved to use
a 30p LM393 dual analog comparator as a hardware-only cross-check.

Final decision: **neither.** The Pico's dual-core architecture gives us
software safety on Core 1 that's independent of the PID loop on Core 0,
and combined with the hardware watchdog + motor driver enable behaviour +
mechanical spring, we already have 4 layers of safety. The LM393 would
have been a belt-and-braces 5th layer with no software in the path — nice
but not required for a hobby Cerbera build. Can be retrofitted later on a
small perfboard if the safety case ever changes.

**Tuning interface (built-in to the Pico's native USB):**
- USB-C variant of the Pico (Pico 2 H, or one of the WaveShare USB-C clones)
  — nicer cable than micro-USB for a permanent in-cabin install.
- Panel-mount USB extension brings the port to a tidy spot under the dash.
- Pico firmware exposes USB CDC serial; plug a laptop in, talk to it.
- Tuning UI: a small **WebSerial** page (Chrome/Edge — no app install).
  Live sliders for pedal map curve, deadzone, PID gains, cruise behaviour.
  "Save to flash" writes the config; next boot uses it.
- Live telemetry: pedal % and servo % stream over the same serial in real
  time so you can see the map applied as you press the pedal on the bench.

## Motor driver

Anything that goes high-impedance when its enable pin floats — so the safety
MCU can fail-safe with a single GPIO pull.

Candidates:
- **Pololu G2 High-Power Motor Driver 18v17** — £25. Good headroom, well
  documented, used in lots of similar builds.
- **Cytron MD13S** — £15. Cheaper, slightly less headroom.
- **Toshiba TB67H420FTG** — through a dev board.

## Servo — Bosch DV-E5 ETB (shaft-lever mod)

**Bench-rig unit ordered.** Vauxhall Z18XER throttle body (Vectra C / Astra H
1.8 petrol). Bosch part `0280750036`. £20 from a UK eBay breaker — cheap
enough to sacrifice while we work out the shaft-lever fabrication. Once the
mod is proven, can upgrade to a clean VW/Audi 2.0 TFSI `06F133062A` (~£20)
for the final install, or keep the Z18XER if it tests fine.

Strip nothing — machine a flat or drill a hole on the *external* end of the
throttle shaft, bolt a lever to it, run a cable from the lever to the
existing ITB linkage. The butterfly carries on spinning uselessly inside
the housing; we don't care.

Why this over a cruise actuator:
- **Full sweep ~80–120 ms** (matches every factory DBW car). A cruise
  actuator does the same in ~250 ms — fine for tracking road speed, feels
  rubbery as a throttle.
- **Designed to fight a spring + atmospheric pressure** across a butterfly.
  Our ITB-linkage return-spring load is well within spec.
- **Built-in 2-track Hall position sensor.** Same pattern as the Audi pedal,
  so Pico can cross-check the ETB's own position via the LM393 the same way
  it cross-checks the pedal — *another* hardware safety layer, free.
- **£15–25 from a scrapyard** — cheaper than the cruise actuator.

### Best donor candidates

All use the Bosch DV-E5 motor family — same connector, same pot signal.

| Donor | Bore | Example part | £ | Notes |
|---|---|---|---|---|
| **VW/Audi 1.8T / 2.0T** | 60–68 mm | `06F133062A`, `06J133062B` | 15–25 | The motorsport go-to. Most documentation. |
| **BMW N52 / N54** | 70 mm | `13547556118` | 25–40 | Slightly nicer, slightly pricier. |
| **Mercedes M271** | 65 mm | Bosch `0280750082` | 20–35 | Quality unit, less common. |
| **Vauxhall Z-series** | 60 mm | Bosch `0280750036` | 10–20 | Cheapest. Plentiful in UK breakers. |
| **Ford 1.6 EcoBoost / Sigma** | ~55 mm | various | 15–25 | Smaller bore but same internals. |

### Standard Bosch DV-E5 pinout (6-way)

| Pin | Function | Goes to |
|---|---|---|
| 1 | Motor + | Pololu G2 OUTA |
| 2 | Motor − | Pololu G2 OUTB |
| 3 | Pot supply +5 V | from controller |
| 4 | Pot signal 1 (rising) | Pico ADC2 + LM393 input |
| 5 | Pot signal 2 (half-scale, falling on some) | Pico ADC4 + LM393 input |
| 6 | Pot ground | controller signal GND |

Confirm with multimeter on arrival — pin order varies by year on some
variants, and signal 2 may be half-scale rising vs falling depending on
donor. Sweep the throttle by hand and watch both ADC channels to learn the
real signature.

### Mechanical conversion

```
                ┌─────────────────────────────────────┐
                │  Bosch ETB housing (keep intact)    │
                │                                     │
   intake side  │  [butterfly stays — does nothing]   │  motor side
                │                                     │
                │           shaft ──────────────────► │── shaft stub
                └─────────────────────────────────────┘     │
                                                            │
                                            lever (mild steel, drilled
                                            for shaft taper + cable clevis)
                                                            │
                                                            ▼
                                               Bowden cable → ITB linkage
```

- Don't dismantle the ETB. The internal bearings are aligned and lubricated;
  any disassembly risks alignment shift.
- Machine a flat on the protruding shaft stub if there isn't one, or drill
  for a roll pin.
- Lever length ~30–50 mm — gives reasonable mechanical advantage without
  exceeding the motor's torque envelope.

### Plan B (already in the parts pile)

- **Subaru WRX/STI cruise actuator `87012FE020`** — already bought (~£40).
  Too slow for our throttle (~250 ms vs the ETB's ~80–120 ms). Keep it for
  the bench rig (early firmware development without risking the real ETB)
  and/or as the basis for a future cruise control project. Not the primary
  servo any more.

### What we actually use from it

| Component | We use? | Why |
|---|---|---|
| DC motor + reduction gearing | Yes | The whole point |
| Bowden cable + threaded end fitting | Yes | Connects to ITB linkage |
| Position potentiometer | Yes | Closed-loop feedback to the Pico |
| Safety clutch | Yes | Extra layer of failsafe; disengages cable if motor stalls |
| Bundled Rostra controller box | **No** | Pico replaces it |
| Bundled brake-pedal switch / vacuum hose | **No** | Cruise add-ons not needed |

### Power & control interface

- 12 V power (always-on + ignition-switched separately)
- 2-wire motor drive from the Pololu G2 (PWM + DIR — the Rostra's internal
  motor is just a brushed DC motor)
- 3-wire position pot back to the Pico's ADC (+5 V, signal, ground)
- Clutch engage line — pull high to engage, low to disengage. Wire to the
  safety Pico so it can fully disengage cable in any failure mode.

### Alternatives we considered

- **Strip a Bosch ETB.** Works, costs £30, but it's a hack — has to be
  remounted, no native cable pull, harder to source a clean used one. Keeping
  it on the "if Rostra unobtainable" list.
- **Bosch Motorsport DBW throttle body.** £600+, overkill, doesn't have a
  cable output anyway.
- **DIY motor + gearbox.** Possible (Maxon DC + planetary), but you re-invent
  the cable end, the clutch, and the pot housing. Not worth it.

## Connectors & loom

- **Pedal:** OEM Bosch/Hella pedal connector matching the chosen donor.
- **ETB-servo:** OEM Bosch 6-way ETB connector + pigtail kit (~£15 on eBay).
- **Cabin → engine bay:** existing throttle-cable grommet.
- **All wiring:** automotive-grade thinwall, soldered + heat-shrink, no scotch
  locks. Belt-and-braces because it's the throttle.

## Cost estimate

| Item | £ |
|---|---|
| DBW pedal (TBD) | ~50 |
| 2× Raspberry Pi Pico | 8 |
| Pololu G2 motor driver | 25 |
| Bosch ETB (scrap) | 30 |
| Connectors + wire + heatshrink | 30 |
| Misc (pulley, return spring, brackets) | 20 |
| **Total** | **~£165** |

(Versus £600+ for a proper aftermarket motorsport ECU with DBW outputs.)
