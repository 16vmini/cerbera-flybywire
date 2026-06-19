// Pin assignments and tunable defaults.
// All compile-time so the optimiser can fold them into the hot path.
#pragma once
#include <Arduino.h>

// --- Pin map (matches DESIGN.md) ---
// Bench-rig phase: all 4 ADCs read sensors (no motor driver yet).
// Production phase: ADC3 moves to motor driver current sense; ETB pot 2 goes
// to the LM393 only (Pico stops reading it directly).
namespace pin {
  // ADC inputs
  constexpr uint8_t pedal_s1   = 26;  // ADC0 — pedal sensor 1
  constexpr uint8_t pedal_s2   = 27;  // ADC1 — pedal sensor 2
  constexpr uint8_t etb_pot1   = 28;  // ADC2 — ETB position pot signal 1 (PID feedback)
  constexpr uint8_t etb_pot2   = 29;  // ADC3 — ETB position pot signal 2 (bench cross-check)
  constexpr uint8_t motor_cs   = 29;  // alias — same pin reused once the driver is wired

  // Motor driver control (Pololu G2)
  constexpr uint8_t motor_pwm  = 10;  // PWM, ~20 kHz
  constexpr uint8_t motor_dir  = 11;  // direction
  constexpr uint8_t motor_slp  = 12;  // open-drain override of /SLP (wired-OR with LM393)
  constexpr uint8_t motor_flt  = 13;  // /FAULT from G2 (pulled high; LOW = fault)

  // Status
  constexpr uint8_t status_led = 14;  // blinks at 1 Hz when healthy
}

// --- ADC reference ---
constexpr float adc_vref       = 3.30f;  // Pico runs ADCs against 3.3 V
constexpr uint16_t adc_resol   = 4095;   // 12-bit
// Pedal sensors run on 5 V but signals are 0.2–4.0 V max so they fit the
// Pico's 0–3.3 V ADC range with no divider. Confirm at bench rig.

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

  // Pedal must be in "idle window" at boot before arming.
  float pedal_idle_min = 0.05f;  // 5 % of pedal range
  float pedal_idle_max = 0.20f;  // 20 %

  // ETB position at boot — must be near closed.
  float etb_closed_tol = 0.10f;  // ±10 % of recorded closed value

  // Motor current trip — sustained over-current trips the safety state.
  float motor_current_trip_a = 6.0f;
  uint32_t motor_current_trip_ms = 200;  // must exceed trip current for this long
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
  // ETB position pot at mechanical stops (signal 1 / signal 2).
  uint16_t etb_pot1_closed = 250, etb_pot1_open = 3800;
  uint16_t etb_pot2_closed = 3800, etb_pot2_open = 250;   // typically inverse of pot1
  // Motor current sense scale (V at ADC per Amp through motor).
  float motor_cs_v_per_a = 0.030f; // Pololu G2 typ.
};

struct Config {
  Calibration cal;
  ThrottleMap map;
  PidCfg pid;
  SafetyCfg safety;
  uint32_t magic = 0xFB1Cf00d;  // sentinel to detect uninitialised flash
  uint16_t version = 1;
};
