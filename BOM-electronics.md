# Electronics BOM — flybywire

Just the electronic bits. Pedal, servo, mechanical brackets and cables are
elsewhere — this list is for "Mark, do you already have…?"

Total: ~£40–50 if buying new from Mouser/RS/eBay. Probably £15 from Mark's bin.

## Controller

| # | Part | Notes |
|---|------|-------|
| 1 | **Raspberry Pi Pico** (USB-C variant ideal — Pico 2 H, or Waveshare RP2040-Zero USB-C) | The brain. 3.3 V logic, native USB. Software cross-check on Core 1. |
| 2 | 100 nF ceramic decoupling caps | Next to the Pico's 3.3 V rail. |
| 1 | 10 µF electrolytic | Bulk decoupling on the 5 V rail. |

*LM393 hardware comparator (formerly here) dropped from the BOM — software
cross-check on the Pico's Core 1 is sufficient for the application. Can
be retrofitted later on a tiny perfboard if desired.*

## Power supply (cabin side)

| # | Part | Notes |
|---|------|-------|
| 1 | **Automotive 12 V → 5 V DC-DC buck regulator**, ≥1 A (Recom R-78E5.0-1.0 or similar) | Feeds the Pico, LM393, and the pedal sensors. Switching, not linear — linear would cook at 7 V drop × 0.5 A. |
| 1 | TVS diode, SMBJ16A or similar | Across the 12 V input. Eats load-dump spikes — non-negotiable on an automotive 12 V rail. |
| 1 | Polyfuse (resettable), 500 mA – 1 A | On the 5 V output to the pedal. Stops a pedal-loom short from killing the regulator. |
| 1 | Inline fuse holder + 3 A blade fuse | On the 12 V feed at the battery / fusebox tap. |
| 1 | Common-mode choke or ferrite bead on the 12 V input | EMI cleanup. Optional but cheap. |

## Motor drive (engine-bay side)

| # | Part | Notes |
|---|------|-------|
| 1 | **Pololu G2 High-Power Motor Driver 18v17** (or 24v13) | The muscle. Takes PWM + DIR + !EN from the Pico, switches 12 V at up to 17 A to the servo motor. Has current-sense output for over-current detection. |
| 1 | Heatsink + thermal pad to suit the G2's tab | Mounted on an inner-wing bracket or similar. The G2 needs to dump heat. |
| 1 | Big electrolytic cap, 470 µF / 25 V minimum | On the 12 V at the motor driver — soaks up motor commutation transients. |
| 1 | Schottky flyback diode, 60 V / 5 A (e.g. SB560) | Across the motor terminals — extra protection on top of the G2's internal diodes. |

## Wiring & connectors

| # | Part | Notes |
|---|------|-------|
| 1 | **Bosch 6-way DBW pedal pigtail** | Matches the Audi A4 B7 8E2721523J pedal connector. eBay search "Bosch DBW pedal connector kit". |
| 1 | Servo connector pigtail | Matches the Rostra (or whichever) servo. Usually a Packard / Delphi / OE-style — sort once we have the servo. |
| 1 | Cabin-bay bulkhead pass-through grommet | Reuse the original throttle-cable hole. |
| ~10 m | Automotive thinwall cable, 0.5 mm² (sensors) + 2.5 mm² (motor power) | Mixed colours. |
| 1 | DT06-12S or similar 12-way Deutsch connector | The "controller box" main connector — pedal, servo, motor, ignition, ground all break out here. Field-serviceable. |
| 1 | Shrink-tube assortment + adhesive-lined | For every joint. |
| 1 | Project box, IP65, aluminium ideally | Houses the Pico + LM393 + DC-DC + USB extension. Aluminium so the USB cable can earth through the chassis if needed. |

## Optional / nice-to-have

| # | Part | Notes |
|---|------|-------|
| 1 | Panel-mount USB-C extension cable (~30 cm) | Brings the Pico's USB to a tidy spot under the dash for laptop tuning. |
| 1 | Small status LED (red/green) for the controller box | Solid green = healthy, red = fault. Easier to diagnose than peering at the Pico's onboard LED. |
| 1 | INA226 current/voltage monitor I²C breakout | Lets the Pico log motor current — useful for "is the linkage binding?" diagnostics. |
| 1 | 3.3 V 5-pin Hall-effect cabin temperature sensor | If we want to know how hot the controller box gets in summer. |

## What Mark almost certainly already has (from his ECU work)

- **Connectors** — pigtails, terminals, crimps. Worth asking before buying.
- **Decoupling caps and 1% resistors** — bin of these.
- **Heat-shrink, automotive cable, fuses** — bench stash.
- **Project box / IP-rated enclosure** — maybe.
- **A spare Pico or two** — possible.
- **TVS diode / Schottky** — likely.

Things he probably doesn't have:
- Pololu G2 motor driver (specific aftermarket part)
- Rostra cruise actuator (specific aftermarket part)
- Bosch DBW pedal connector pigtail (specific OE form factor)
