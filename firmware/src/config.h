// Pin assignments and tunable defaults.
// All compile-time so the optimiser can fold them into the hot path.
#pragma once
#include <Arduino.h>

// --- Pin map (matches DESIGN.md) ---
// Bench-rig phase: all 4 ADCs read sensors (no motor driver yet).
// Production phase: ADC3 moves to motor driver current sense; ETB pot 2 goes
// to the LM393 only (Pico stops reading it directly).
namespace pin {
  // ADC inputs — GP26/GP27/GP28 are the three external ADC channels.
  // GP29 / ADC3 is NOT broken out on a standard Pico (it's hardwired internally
  // to VSYS voltage sensing), so we can't read ETB Pot 2 or motor current with
  // just the Pico — would need an ADS1115 over I²C if redundant cross-check or
  // current sense becomes essential.
  constexpr uint8_t pedal_s1   = 26;  // ADC0 — pedal sensor 1
  constexpr uint8_t pedal_s2   = 27;  // ADC1 — pedal sensor 2
  constexpr uint8_t etb_pot1   = 28;  // ADC2 — ETB position pot signal 1 (PID feedback)

  // Motor driver control (IBT-4 H-bridge — dual-PWM protocol).
  //
  // Mark recommended the IBT-4 over the Pololu G2: 50 A continuous, optical
  // isolation on the inputs (cleaner sensor readings in a noisy engine bay),
  // 3.3 V to 12 V logic compatible, MC34063 onboard regulator. Trade-offs vs
  // the G2: no /SLP enable pin (use both-IN-low for coast), no /FAULT output,
  // no current-sense pin.
  //
  // Protocol: IN1 PWM-driven → motor forward (open throttle).
  //           IN2 PWM-driven → motor reverse (close throttle, helps PID
  //                            settle past overshoot; spring is always present
  //                            as the master mechanical default).
  //           BOTH LOW       → coast → spring closes throttle. The safe state.
  //           BOTH HIGH      → electrical brake (avoid, except for active
  //                            stop scenarios).
  constexpr uint8_t motor_in1  = 10;  // open-throttle PWM (was motor_pwm for G2)
  constexpr uint8_t motor_in2  = 11;  // close-throttle PWM (was motor_dir for G2)

  // Status
  constexpr uint8_t status_led = 14;  // blinks at 1 Hz when healthy
}

// --- ADC reference ---
constexpr float adc_vref       = 3.30f;  // Pico runs ADCs against 3.3 V
constexpr uint16_t adc_resol   = 4095;   // 12-bit
//
// !!! IMPORTANT — the RP2040 ADC pin absolute-maximum voltage is ~Vdd + 0.3 V
//   (i.e. ~3.6 V). EXCEEDING THIS CAN DAMAGE THE CHIP.
//
// A Bosch DBW pedal sensor on its FACTORY 5 V supply outputs 0.4–4.0 V,
// which is OUT OF SPEC for a direct Pico ADC connection. Two safe ways to
// wire it:
//
//   (a) Power the sensor from the Pico's own 3.3 V rail (ratiometric Hall
//       sensors scale their output to the supply, so signals stay within
//       0–3.3 V automatically). This is what the bench rig does — simple
//       and self-protecting. Slightly less ADC resolution but plenty of
//       counts in 12 bits to nail pedal % accurately.
//
//   (b) Keep the OEM 5 V supply, then put a voltage divider on EACH signal
//       line before the Pico ADC pin. 10 k / 22 k pair gives a ratio of
//       0.687 → 4 V external scales to 2.75 V at the ADC, well inside spec.
//       Use this for the production install (5 V gives the LM393
//       cross-check, if we ever wire one, better signal-to-noise margin).
//
// NEVER feed a sensor signal directly to a Pico ADC if that signal can
// exceed 3.3 V — even briefly. Spotted by Mark on review, 2026-06-22.

// --- Loop rates ---
constexpr uint32_t control_loop_hz  = 200;
constexpr uint32_t safety_loop_hz   = 500;
constexpr uint32_t telemetry_hz     = 50;
constexpr uint32_t watchdog_ms      = 100;

// --- Safety thresholds ---
struct SafetyCfg {
  // Pedal cross-check: |S1/2 - S2| must stay below this fraction of full scale.
  // ±5% of S1 full (4.0 V) = ±100 mV. LM393 enforces this in hardware too;
  // the software check is a belt-and-braces backup.
  float pedal_xcheck_tol = 0.05f;

  // Pedal must be in "idle window" before arming. Lower bound 0.00f means
  // "anything from genuine rest up to lightly-touched". Upper bound 0.20f
  // catches "pedal pressed at boot" which is the real safety case.
  // (Was 0.05f historically; lowered after we found Mark's "cal-with-margin"
  // approach conflicts with the simpler "rest = 0% on display" preference.)
  float pedal_idle_min = 0.00f;
  float pedal_idle_max = 0.20f;

  // ETB position at boot — must be near closed.
  float etb_closed_tol = 0.10f;  // ±10 % of recorded closed value

  // Motor current trip — sustained over-current trips the safety state.
  // (Not currently used — IBT-4 has no current sense pin. Kept for forward
  // compat with a future external shunt.)
  float motor_current_trip_a = 6.0f;
  uint32_t motor_current_trip_ms = 200;

  // Stuck-linkage detector — Mark's review 2026-06-22.
  // If the firmware is commanding significant motor effort but the ETB
  // position isn't responding (seized cable, motor failure, sensor stuck,
  // broken linkage), latch a fault and let the spring close the throttle.
  // The motor's electrical time constant is short and the throttle's
  // mechanical inertia is small — under any duty > stuck_min_duty we
  // expect at least stuck_min_movement of position change within
  // stuck_timeout_ms.
  float stuck_min_duty = 0.30f;        // only check when commanded duty > 30%
  float stuck_min_movement = 0.05f;    // expect ≥ 5% ETB position change
  uint32_t stuck_timeout_ms = 500;     // within this window
};

// --- Throttle map ---
// Pedal-to-target curve. Stored as 5 break-points (pedal 0–100 % → target 0–100 %).
// Default = softer first 20 % (good in traffic), linear beyond.
struct ThrottleMap {
  float pts[5][2] = {
    {0.00f, 0.00f},
    {0.20f, 0.08f},   // soft initial
    {0.50f, 0.40f},
    {0.80f, 0.78f},
    {1.00f, 1.00f},
  };
};

// --- PID gains (start conservative) ---
struct PidCfg {
  float kp = 1.5f;
  float ki = 0.2f;
  float kd = 0.05f;
  float i_max = 0.5f;   // anti-windup clamp on integral term
  float out_max = 1.0f; // PWM duty cap (1.0 = 100 %)
};

// --- Calibration (learned at bench rig, stored in flash) ---
struct Calibration {
  // Pedal sensor min/max raw ADC values (idle / WOT positions).
  uint16_t pedal_s1_min = 200, pedal_s1_max = 3900;
  uint16_t pedal_s2_min = 100, pedal_s2_max = 1950;
  // ETB position pot at mechanical stops (signal 1 only — pot signal 2 needs
  // an ADS1115 to read, deferred). Pot 2 calibration kept here for forward
  // compat with that future enhancement.
  uint16_t etb_pot1_closed = 250, etb_pot1_open = 3800;
  uint16_t etb_pot2_closed = 3800, etb_pot2_open = 250;   // not currently used
};

struct Config {
  Calibration cal;
  ThrottleMap map;
  PidCfg pid;
  SafetyCfg safety;
  uint32_t magic = 0xFB1Cf00d;  // sentinel to detect uninitialised flash
  uint16_t version = 1;
};
