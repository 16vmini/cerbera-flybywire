# Loom BOM — wire + sleeving + connectors

Purchase list for building the production fly-by-wire loom. Designed around
**Vehicle Wiring Products** (vehiclewiringproducts.eu) as the primary supplier —
they stock all the thinwall sizes in solid colours by the metre, and same-day
dispatch from the UK. Their part-number scheme is `TWxxC` where `xx` is the
cross-section (`0.5`, `1.0`, `1.5`, `2.5`, `4.0`) and `C` is the colour code
(`RD` red, `BK` black, `YL` yellow, etc.) — easy to populate into their cart.

## Cable

### 0.5 mm² thinwall — sensor signals + supplies + grounds + logic

| Colour | Use | Approx length |
|---|---|---|
| Red | Sensor +5 V supply rail (run from controller to pedal AND to ETB pot) | **10 m** |
| Black | Signal grounds (sensor grounds, ADC ground references) | **10 m** |
| Yellow | Pedal sensor 1 signal | **5 m** |
| Green | Pedal sensor 2 signal | **5 m** |
| Orange | ETB pot signal 1 | **3 m** |
| Blue | ETB pot signal 2 | **3 m** |
| White | Pico → motor driver PWM | **3 m** |
| Grey | Pico → motor driver DIR + /SLP + /FAULT bundle (3 wires same colour, label both ends) | **5 m** |

### 1.5 mm² thinwall — controller power + ground

| Colour | Use | Approx length |
|---|---|---|
| Red | Switched +12 V feed to controller box (via fused tap) | **5 m** |
| Black | Controller box chassis ground return | **5 m** |

### 2.5 mm² thinwall — motor driver power + motor leads

| Colour | Use | Approx length |
|---|---|---|
| Red | +12 V to Pololu G2 motor driver (from battery via fuse) | **3 m** |
| Black | Motor driver power ground to chassis | **3 m** |
| Brown | G2 OUTA → ETB motor + | **2 m** |
| Blue | G2 OUTB → ETB motor − | **2 m** |

**Total cable spend: ~£25–35.**

## Sleeving + protection

| Item | Approx qty | Notes |
|---|---|---|
| **Convoluted (split) conduit, 10 mm** | 3 m | Engine-bay cable runs. Black, slit so cable goes in without cutting ends. |
| **Convoluted conduit, 6 mm** | 2 m | Tighter runs near connectors. |
| **Braided expandable sleeve, 12 mm** | 2 m | Optional — for the visible portion (e.g. under-dash or where it'll show). Looks tidy. |
| **Adhesive-lined heat shrink, 3:1 ratio, assorted sizes (3 mm → 12 mm)** | 1 pack | Critical for sealing crimp/solder joints against moisture. |
| **Self-amalgamating tape** | 1 roll | Final wrap on engine-bay sections. |
| **Cable ties, mixed 2.5 mm / 4.8 mm** | 1 pack | Routing + securing. |

## Enclosure approach: fly leads through cable glands

The 3D-printed controller box does **not** have panel-mount connectors —
the wires exit via three **sealed cable glands** drilled into the wall, and
the in-line connectors (Deutsch / Bosch) live at the **far ends** of those
fly leads, where they actually need to disconnect.

Why this is better for a one-off: half the cost, half the build complexity,
no expensive Deutsch crimp tool needed, no precision panel cutouts in the
3D print. Same disconnect-for-service benefit.

### Cable glands (on the box wall)

| Gland | For | Cable diameter range |
|---|---|---|
| 1× **M12 nylon cable gland** | Engine-bay 12-wire fly lead (in conduit) | 6-10 mm |
| 1× **M10 nylon cable gland** | Pedal-side 4-wire fly lead | 4-8 mm |
| 1× **M8 nylon cable gland** | 12 V switched power feed (2 wires) | 3-6 mm |

~£3-4 total. Screwfix, Amazon, or VWP all stock them.

### Connectors at the harness ends (not on the box)

| At | Connector | VWP / source |
|---|---|---|
| **Engine-bay end** of the 12-wire fly lead | **Deutsch DT04-12PA** plug + matching **DT06-12SA** in-line socket on the engine-bay harness section | VWP "Deutsch DT 12-way" range |
| **Pedal end** of the pedal fly lead | **Bosch 6-way DBW pedal pigtail** matching Audi A4 B7 `8E2721523J` | eBay search "Bosch DBW pedal connector kit" |
| **ETB end** of the engine-bay harness | **Bosch 6-way DV-E5 connector pigtail** matching Vauxhall Z18XER | eBay "Bosch DV-E5 connector" |
| **Power end** | Female blade + ring terminal pair for ignition tap and chassis ground | VWP "crimp terminals assortment" |
| **3-way blade fuse holders** + 3 A / 5 A / 15 A blade fuses | Inline fuses on power feeds (3 A for controller logic, 15 A on motor power feed) | VWP / Halfords |

### Crimp tooling

- **Sopoby HX-K1ND** ratcheting four-indent crimper (Amazon, ~£35) for the Deutsch terminals. Avoids the £300 Deutsch HDT-48-00 official tool — fine for hobby use.
- **Standard ratcheting crimpers** for thinwall blade / bullet / ring terminals — ~£20.
- **Wire stripper with thinwall gauge** — pliers crush thinwall insulation.

## Tooling (if you don't already have it)

| Item | Why |
|---|---|
| **Ratcheting crimp tool** for thinwall terminals | Hand-crimping with pliers gives unreliable joints. Cheap ratchet crimpers (~£20-30) make a massive difference. |
| **Heat-gun** for the heat-shrink | A lighter works but a heat-gun is night-and-day better for proper shrink + glue activation. |
| **Wire stripper with thinwall gauge** | Standard pliers crush thinwall insulation. |

## Notes on colour scheme

This map is chosen so that:
- **Red = always supply** (5 V or 12 V depending on gauge), **Black = always ground**. Universal automotive convention.
- **Sensor signals get distinct colours** so a probe on yellow always means "pedal sensor 1", no second-guessing.
- **Motor power leads use brown + blue** (matches Pololu G2's typical convention so the motor sees the same colours on both ends).
- **Pico logic signals are white/grey** — these go through a separate small sleeve from the sensors so motor PWM noise doesn't capacitively couple onto sensor signals.

When the loom is built, **photograph it before sleeving** and **label both ends of every wire** with a Sharpie or numbered shrink-tube. Future-you in 6 months when something needs diagnosing will love past-you for this.
