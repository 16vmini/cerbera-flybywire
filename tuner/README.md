# Tuner — flybywire WebSerial config + telemetry page

Single static HTML file. Open it in **Chrome or Edge** (WebSerial isn't in
Firefox/Safari yet). No install, no build step.

## Use

1. Build + flash the firmware onto the Pico (see `../firmware/README.md`).
2. Plug the Pico into the laptop's USB.
3. Open `index.html` directly (`file:///c:/source/tvr/flybywire/tuner/index.html`)
   OR host it anywhere — even tvr-wiki.co.uk/flybywire/tuner/ would work.
4. Click **Connect to Pico**, pick the COM port that appeared when you
   plugged the Pico in. Live telemetry should start streaming.
5. **Calibrate** in this order:
   - Pedal at rest → click "Capture idle" for the S1 / S2 min values.
   - Pedal pressed fully → "Capture WOT" for the max values.
   - Move the ETB linkage by hand to the closed stop → "Capture closed".
   - Move it to the open stop → "Capture open".
   - **Apply** → **Save to flash**.
6. **Arm** to enable the motor driver. With the engine OFF, gently push the
   pedal — the ETB should track it.
7. Test the failure modes from `../SAFETY.md` BEFORE the bench rig ever
   bolts to the car.

## What's where

- `index.html` — the entire app. WebSerial connect, live telemetry bars,
  calibration form, PID gains form, two test mode buttons.

## v0.1 limitations

- "Capture" buttons currently just prompt you to type the raw ADC value.
  v0.2 will pull the live telemetry and write it directly.
- No throttle map editor in the UI yet — defaults in the firmware are
  sensible (soft first 20 %, linear after).
- Connection state survives until the laptop sleeps. After resume, click
  Disconnect + Connect to recover.
