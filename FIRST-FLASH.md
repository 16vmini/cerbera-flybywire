# First-flash walkthrough

Pre-conditions:
- Pico is plugged into USB (any port).
- Pico is in BOOTSEL mode (showed up as the `RPI-RP2` drive).
- VS Code is installed.
- PlatformIO IDE extension installed (alien-head icon in left sidebar).

## 1. Open the firmware folder

File → Open Folder → `c:\source\tvr\flybywire\firmware`

VS Code re-opens. The bottom of the window shows a blue PlatformIO toolbar
(home / build / upload / monitor icons). First time you open this folder,
PlatformIO downloads the arduino-pico toolchain — takes 2–5 minutes, shows
progress in the bottom-right. Let it finish.

## 2. Build (optional — confirms the toolchain works)

In the PlatformIO toolbar at the bottom, click the **✓ (Build)** tick.
You'll see lines like:
```
Building in release mode
Compiling .pio\build\pico\src\main.cpp.o
...
RAM:   [=         ]   8.4% (used 22568 bytes from 270336 bytes)
Flash: [          ]   3.1% (used 65232 bytes from 2093056 bytes)
========== [SUCCESS] Took 24.85 seconds ==========
```

If it says SUCCESS, the firmware compiled. If it errors, paste the message
into the chat and I'll sort it.

## 3. Flash to the Pico

Pico must STILL be in BOOTSEL mode (`RPI-RP2` drive visible in File
Explorer).

Click the **→ (Upload)** arrow in the PlatformIO toolbar. PlatformIO copies
the `.uf2` onto the drive. The drive disappears (Pico has rebooted).

Within 1–2 seconds, Windows pings (USB connect sound) and a new **COM
port** appears under Device Manager → Ports (COM & LPT) — that's the
Pico's USB CDC serial.

If the upload fails saying "no UF2 drive found", do this:
- Unplug Pico
- Hold BOOTSEL
- Plug back in (keep holding BOOTSEL ~2 sec)
- Release BOOTSEL
- Try Upload again

## 4. Open the tuner

Open `c:\source\tvr\flybywire\tuner\index.html` in Chrome or Edge.
(Double-click the file, or paste `file:///c:/source/tvr/flybywire/tuner/index.html`
into the URL bar.)

Click **Connect to Pico**. A browser dialog pops up listing serial ports —
pick the one that appeared when you flashed (it'll say "Raspberry Pi Pico"
or similar). Click Connect.

You should now see:
- Status pill: green dot, "Connected to Pico"
- Four needle gauges in the **Live sensors** card start moving
- The "Calibrated position" bars start moving
- The Serial log shows `> STREAM ON` then `< OK` and a steady stream of
  `T pedal=...` lines (telemetry)

## 5. Test sensors

With the **pedal wired** (see [BENCH-WIRING.md](BENCH-WIRING.md)):
- Pedal at rest: S1 needle at ~5 %, S2 needle at ~2 %
- Pedal fully pressed: S1 needle at ~95 %, S2 needle at ~48 %
- Both move together (one rising, one half-scale or falling)

With the **ETB wired**:
- Pushing the throttle butterfly by hand makes both ETB pot needles move
- One rises while the other falls

If a gauge doesn't move when you expect it to → that wire is reversed or
not connected. Swap pins as needed (Bosch connectors are mirror-symmetric,
easy to misread).

## 6. Calibrate

In the **Live sensors** card, each gauge has two buttons under it:

- Pedal at rest → click "Idle" on S1 and S2 (snaps raw ADC values into
  the calibration fields below)
- Pedal fully pressed → click "WOT"
- Throttle butterfly fully closed (resting) → click "Closed" on ETB
- Butterfly fully open (push by hand to mechanical stop) → click "Open"

Then click **Apply to Pico** in the Calibration card.
Then click **Save to flash** in the top bar.

The needles should now match the "Calibrated position" bars — pedal % and
ETB % should track properly.

## 7. Set the throttle map

The map editor is the wide chart at the bottom. 5 break-points connected by
lines. Drag them with the mouse / finger to shape the curve.

- Default: soft first 20 %, linear after — good general purpose.
- Linear: 1:1 pedal-to-throttle — twitchy but predictable.
- Soft start: very soft below 50 %, sharp above — good for traffic.

Click **Apply to Pico**, then **Save to flash**.

The orange dot on the curve shows where the pedal currently is. Press the
pedal and watch it move along the curve in real time.

## What works without the motor driver

Right now (no Pololu G2 yet) the firmware:
- Reads all 4 sensors and streams telemetry → ✅ works
- Calculates target from pedal + map → ✅ works
- Calculates PWM duty from PID → ✅ calculates but outputs go nowhere (no driver)
- Boot self-check → ✅ runs, doesn't trip without the motor wired
- ARM / DISARM → ✅ accepts the command but motor side does nothing

When the G2 arrives:
- Wire it up per [DESIGN.md](DESIGN.md).
- Move ETB pot 2 from Pico ADC3 to the LM393 (Pico now reads motor current
  on ADC3 instead).
- Update `motor_cs_v_per_a` calibration value if needed.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| "WebSerial not supported" | Using Firefox or Safari — switch to Chrome or Edge |
| No serial port shown in browser dialog | Pico hasn't enumerated — check Device Manager, unplug+replug |
| Connects but no telemetry | Send `PING` from the dropdown — if no PONG, firmware didn't flash; redo upload |
| Telemetry but gauge stays at 0 | That sensor pin is unconnected or floating; double-check the BENCH-WIRING table |
| Gauge stuck at 4095 | Pin shorted to 3.3 V supply, or floating with internal pull-up — check wiring |
| Fault pill shows "boot: ETB not closed" | ETB pot wired but butterfly isn't at idle — push it closed by hand, send `CLEARFAULT`, retry ARM |

## What to send me when you get back

A screenshot of the tuner with the four gauges moving when you press the
pedal / push the butterfly. That's the bench rig phase 1 done.
