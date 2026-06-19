// flybywire — Cerbera throttle controller firmware.
// Single Pico, dual core: Core 0 runs the PID control loop, Core 1 runs the
// safety state machine + USB tuning protocol. Hardware safety (LM393 wired
// into Pololu G2 /SLP, return spring on linkage) is the real failsafe;
// firmware is one of several layers, not the only one.
//
// Build with PlatformIO (Earle Philhower's arduino-pico core). See platformio.ini.

#include <Arduino.h>
#include <LittleFS.h>
#include <hardware/watchdog.h>
#include "config.h"

// ----- Globals (shared between cores via atomic / volatile) -----
Config cfg;
volatile bool armed = false;        // true => motor driver may be enabled
volatile bool fault_latched = false;
volatile char fault_reason[48] = "";

struct Telemetry {
  volatile float pedal_pct;    // 0–1.0 — pedal S1 after calibration
  volatile float pedal2_pct;   // 0–1.0 — pedal S2 after calibration
  volatile float target_pct;   // 0–1.0 — what we're asking the throttle to do
  volatile float etb_pct;      // 0–1.0 — ETB pot signal 1 after cal
  volatile float etb2_pct;     // 0–1.0 — ETB pot signal 2 after cal
  volatile uint16_t s1_raw, s2_raw, e1_raw, e2_raw;  // raw 12-bit ADC for cal/diag
  volatile float motor_a;
  volatile uint32_t loop_us;
  volatile uint32_t uptime_s;
} tel;

// ----- Helpers -----
static inline uint16_t read_adc_avg(uint8_t pin, uint8_t n = 4) {
  uint32_t s = 0;
  for (uint8_t i = 0; i < n; i++) s += analogRead(pin);
  return s / n;
}

static inline float raw_to_unit(uint16_t raw, uint16_t lo, uint16_t hi) {
  if (hi <= lo) return 0.0f;
  float v = (float)(raw - lo) / (float)(hi - lo);
  return constrain(v, 0.0f, 1.0f);
}

// Linear interpolation across the throttle map break-points.
static float apply_map(float pedal) {
  const auto& pts = cfg.map.pts;
  for (int i = 1; i < 5; i++) {
    if (pedal <= pts[i][0]) {
      float t = (pedal - pts[i-1][0]) / (pts[i][0] - pts[i-1][0]);
      return pts[i-1][1] + t * (pts[i][1] - pts[i-1][1]);
    }
  }
  return pts[4][1];
}

// ----- Safety -----
static void latch_fault(const char* why) {
  noInterrupts();
  if (!fault_latched) {
    fault_latched = true;
    strncpy((char*)fault_reason, why, sizeof(fault_reason) - 1);
  }
  armed = false;
  digitalWrite(pin::motor_slp, LOW);  // open-drain low = driver disabled
  analogWrite(pin::motor_pwm, 0);
  interrupts();
}

static void clear_fault() {
  noInterrupts();
  fault_latched = false;
  fault_reason[0] = 0;
  interrupts();
}

// Cross-check pedal sensors in software (LM393 also does this in hardware).
// Returns true if healthy.
static bool pedal_xcheck_ok(uint16_t s1, uint16_t s2) {
  float v1 = raw_to_unit(s1, cfg.cal.pedal_s1_min, cfg.cal.pedal_s1_max);
  float v2 = raw_to_unit(s2, cfg.cal.pedal_s2_min, cfg.cal.pedal_s2_max);
  return fabsf(v1 - v2) <= cfg.safety.pedal_xcheck_tol;
}

// Is it safe to ARM right now? Two failure tiers:
//   - HARD failures (sensor cross-check fault, motor driver fault) → latch
//     fault, requires CLEARFAULT + retry to recover.
//   - SOFT refusals (pedal not at idle, throttle not closed) → just refuse,
//     no latched fault. Operator does the right thing, ARM works.
//
// Mark's review 2026-06-07: it's wrong to latch a fault just because the
// throttle was sat open at power-on — that's just "operator hasn't keyed
// from rest yet". The motor stays disabled (default state) so the spring
// holds throttle closed regardless; we just won't let the user enable the
// driver until things look right.
static bool safe_to_arm(const char** reason_out) {
  uint16_t s1 = read_adc_avg(pin::pedal_s1, 8);
  uint16_t s2 = read_adc_avg(pin::pedal_s2, 8);
  uint16_t etb = read_adc_avg(pin::etb_pot1, 8);

  // HARD: sensor cross-check
  if (!pedal_xcheck_ok(s1, s2)) {
    latch_fault("pedal cross-check");
    if (reason_out) *reason_out = "pedal sensors disagree — check wiring";
    return false;
  }

  // HARD: motor driver in fault state
  if (digitalRead(pin::motor_flt) == LOW) {
    latch_fault("motor driver /FAULT");
    if (reason_out) *reason_out = "motor driver reports fault";
    return false;
  }

  // SOFT: operator hasn't keyed from rest yet — don't latch, just wait
  float pedal = raw_to_unit(s1, cfg.cal.pedal_s1_min, cfg.cal.pedal_s1_max);
  if (pedal < cfg.safety.pedal_idle_min || pedal > cfg.safety.pedal_idle_max) {
    if (reason_out) *reason_out = "waiting for pedal to be at idle";
    return false;
  }

  // SOFT: throttle not closed — wait for it to settle (spring) before driving
  float etb_pct = raw_to_unit(etb, cfg.cal.etb_pot1_closed, cfg.cal.etb_pot1_open);
  if (etb_pct > cfg.safety.etb_closed_tol) {
    if (reason_out) *reason_out = "waiting for throttle to be closed";
    return false;
  }

  if (reason_out) *reason_out = "ok";
  return true;
}

// ----- Control loop (Core 0) -----
static float pid_i = 0.0f;
static float pid_prev_err = 0.0f;

static void control_step(uint32_t now_us, uint32_t dt_us) {
  uint16_t s1 = analogRead(pin::pedal_s1);
  uint16_t s2 = analogRead(pin::pedal_s2);
  uint16_t etb = analogRead(pin::etb_pot1);
  uint16_t etb2 = analogRead(pin::etb_pot2);

  // Record raw values for the tuner to display & calibrate against.
  tel.s1_raw = s1; tel.s2_raw = s2; tel.e1_raw = etb; tel.e2_raw = etb2;
  tel.pedal2_pct = raw_to_unit(s2, cfg.cal.pedal_s2_min, cfg.cal.pedal_s2_max);
  tel.etb2_pct  = raw_to_unit(etb2, cfg.cal.etb_pot2_closed, cfg.cal.etb_pot2_open);

  if (!pedal_xcheck_ok(s1, s2)) {
    // Bench rig phase: don't latch fault on cross-check until calibration is
    // in — otherwise nothing works until cal is done. Software-side guard
    // (the LM393 will still enforce in hardware once present.)
    if (armed) latch_fault("runtime: pedal cross-check");
  }

  float pedal_unit = raw_to_unit(s1, cfg.cal.pedal_s1_min, cfg.cal.pedal_s1_max);
  float target = apply_map(pedal_unit);
  float actual = raw_to_unit(etb, cfg.cal.etb_pot1_closed, cfg.cal.etb_pot1_open);

  float err = target - actual;
  float dt = dt_us * 1e-6f;

  pid_i += cfg.pid.ki * err * dt;
  pid_i = constrain(pid_i, -cfg.pid.i_max, cfg.pid.i_max);
  float d = (err - pid_prev_err) / max(dt, 1e-4f);
  pid_prev_err = err;

  float u = cfg.pid.kp * err + pid_i + cfg.pid.kd * d;
  // Convert signed control effort to PWM duty + DIR.
  bool dir_open = (u >= 0);
  float duty = constrain(fabsf(u), 0.0f, cfg.pid.out_max);

  if (armed && !fault_latched) {
    digitalWrite(pin::motor_dir, dir_open ? HIGH : LOW);
    analogWrite(pin::motor_pwm, (int)(duty * 255));
    digitalWrite(pin::motor_slp, HIGH);  // open-drain hi-Z = enabled (pull-up wins)
  } else {
    analogWrite(pin::motor_pwm, 0);
    digitalWrite(pin::motor_slp, LOW);   // open-drain low = disabled
  }

  tel.pedal_pct = pedal_unit;
  tel.target_pct = target;
  tel.etb_pct = actual;
  tel.motor_a = analogRead(pin::motor_cs) * (adc_vref / adc_resol) / cfg.cal.motor_cs_v_per_a;
}

// ----- USB CDC protocol (Core 1) -----
// Line-oriented, ASCII. One command per line, response = one line.
// Commands:
//   PING                              → "PONG"
//   GET <param>                       → "<param>=<value>"
//   SET <param> <value>               → "OK" or "ERR <msg>"
//   ARM                               → enable motor driver if boot-safe
//   DISARM                            → drop enable, clear nothing
//   CLEARFAULT                        → clear latched fault (manual)
//   SAVE                              → persist config to flash
//   STREAM ON|OFF                     → emit telemetry lines at telemetry_hz
//   TEST <name>                       → run a bench test (see below)
//
// Telemetry stream format (when STREAM ON):
//   T pedal=<0.000-1.000> target=<...> etb=<...> i=<motor_a> armed=<0|1> fault=<...>

static bool streaming = false;
static char line_buf[160];
static uint8_t line_len = 0;

static void emit(const char* s) { Serial.println(s); }

static void handle_command(char* line);

static void poll_serial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n' || line_len >= sizeof(line_buf) - 1) {
      line_buf[line_len] = 0;
      if (line_len > 0) handle_command(line_buf);
      line_len = 0;
    } else {
      line_buf[line_len++] = c;
    }
  }
}

static void emit_telemetry() {
  char buf[240];
  // Calibrated percentages (for the gauges) AND raw ADC readings (for the
  // calibration capture buttons). Tuner parses either.
  snprintf(buf, sizeof(buf),
    "T pedal=%.3f pedal2=%.3f target=%.3f etb=%.3f etb2=%.3f "
    "s1=%u s2=%u e1=%u e2=%u i=%.2f armed=%d fault=%s",
    tel.pedal_pct, tel.pedal2_pct, tel.target_pct, tel.etb_pct, tel.etb2_pct,
    (unsigned)tel.s1_raw, (unsigned)tel.s2_raw,
    (unsigned)tel.e1_raw, (unsigned)tel.e2_raw,
    tel.motor_a, armed ? 1 : 0,
    fault_latched ? (const char*)fault_reason : "ok");
  Serial.println(buf);
}

// ----- Persistent config -----
static void load_config() {
  if (!LittleFS.begin()) { LittleFS.format(); LittleFS.begin(); }
  File f = LittleFS.open("/config.bin", "r");
  if (!f) return;
  Config tmp;
  if (f.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp) && tmp.magic == 0xFB1Cf00d) {
    cfg = tmp;
  }
  f.close();
}

static bool save_config() {
  File f = LittleFS.open("/config.bin", "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)&cfg, sizeof(cfg));
  f.close();
  return n == sizeof(cfg);
}

// ----- Setup / loops -----
void setup() {
  Serial.begin(115200);

  pinMode(pin::pedal_s1, INPUT);
  pinMode(pin::pedal_s2, INPUT);
  pinMode(pin::etb_pot1, INPUT);
  pinMode(pin::motor_cs, INPUT);
  analogReadResolution(12);

  // Mark's design review — set throttle to ZERO before anything else, so the
  // window between power-on and the first control loop pass cannot ever drive
  // the motor. Order matters: configure pins as inputs (high-impedance) by
  // default, then explicitly drive outputs to "safe" before turning them into
  // outputs.
  digitalWrite(pin::motor_slp, LOW);   // pre-write before output mode
  pinMode(pin::motor_slp, OUTPUT_OPENDRAIN);
  digitalWrite(pin::motor_slp, LOW);   // driver disabled (open-drain = pull low)

  analogWriteFreq(20000);
  analogWriteRange(255);
  // Configure PWM at 0 duty BEFORE flipping pinMode to OUTPUT so the very
  // first edge is a known-zero, not a momentary half-second of high.
  pinMode(pin::motor_dir, OUTPUT);
  digitalWrite(pin::motor_dir, LOW);
  pinMode(pin::motor_pwm, OUTPUT);
  analogWrite(pin::motor_pwm, 0);      // PWM duty = 0%, throttle stays closed

  pinMode(pin::motor_flt, INPUT_PULLUP);
  pinMode(pin::status_led, OUTPUT);

  armed = false;                       // start DISARMED — explicit
  fault_latched = false;

  load_config();
  watchdog_enable(watchdog_ms, 1);     // hardware watchdog; resets chip on hang

  // No-op boot check — we deliberately don't call safe_to_arm() at setup
  // any more, because we don't want to latch a fault just because the
  // throttle is open at boot. The motor driver is disabled (above), so the
  // throttle stays where the spring puts it (closed). Operator sends ARM
  // via the tuner; safe_to_arm() runs at that point.
}

// Core 0: control loop at control_loop_hz
void loop() {
  static uint32_t last_ctrl = 0;
  static uint32_t last_led = 0;
  uint32_t now = micros();

  watchdog_update();

  if (now - last_ctrl >= 1000000UL / control_loop_hz) {
    uint32_t dt = now - last_ctrl;
    last_ctrl = now;
    uint32_t t0 = micros();
    control_step(now, dt);
    tel.loop_us = micros() - t0;
  }

  if (millis() - last_led > 500) {
    last_led = millis();
    digitalWrite(pin::status_led, fault_latched ? HIGH : !digitalRead(pin::status_led));
    tel.uptime_s = millis() / 1000;
  }

  // Safety: check G2 /FAULT line every pass.
  if (armed && digitalRead(pin::motor_flt) == LOW) {
    latch_fault("runtime: G2 /FAULT asserted");
  }
}

// Core 1: USB CDC protocol + telemetry stream.
void setup1() {}
void loop1() {
  static uint32_t last_telem = 0;
  poll_serial();
  if (streaming && millis() - last_telem >= 1000 / telemetry_hz) {
    last_telem = millis();
    emit_telemetry();
  }
}

// ----- Command handling -----
static bool match(char*& s, const char* kw) {
  size_t n = strlen(kw);
  if (strncasecmp(s, kw, n) == 0 && (s[n] == ' ' || s[n] == 0)) { s += n; while (*s == ' ') s++; return true; }
  return false;
}

static char* next_token(char*& s) {
  while (*s == ' ') s++;
  char* t = s;
  while (*s && *s != ' ') s++;
  if (*s) { *s++ = 0; while (*s == ' ') s++; }
  return t;
}

static void handle_command(char* line) {
  char* p = line;
  while (*p == ' ') p++;

  if (match(p, "PING")) { emit("PONG"); return; }
  if (match(p, "VERSION")) { emit("V flybywire 0.1"); return; }
  if (match(p, "ARM")) {
    if (fault_latched) {
      char buf[120];
      snprintf(buf, sizeof(buf), "ERR fault latched: %s", (const char*)fault_reason);
      emit(buf);
      return;
    }
    const char* why = "ok";
    if (!safe_to_arm(&why)) {
      char buf[160]; snprintf(buf, sizeof(buf), "ERR %s", why); emit(buf);
      return;
    }
    armed = true; emit("OK armed"); return;
  }
  if (match(p, "DISARM")) { armed = false; emit("OK disarmed"); return; }
  if (match(p, "CLEARFAULT")) { clear_fault(); emit("OK"); return; }
  if (match(p, "SAVE")) { emit(save_config() ? "OK saved" : "ERR save"); return; }
  if (match(p, "STREAM")) {
    char* arg = next_token(p);
    streaming = (strcasecmp(arg, "ON") == 0);
    emit("OK");
    return;
  }
  if (match(p, "GET")) {
    char* k = next_token(p);
    char buf[120];
    if (strcasecmp(k, "kp") == 0) { snprintf(buf, sizeof(buf), "kp=%.4f", cfg.pid.kp); emit(buf); return; }
    if (strcasecmp(k, "ki") == 0) { snprintf(buf, sizeof(buf), "ki=%.4f", cfg.pid.ki); emit(buf); return; }
    if (strcasecmp(k, "kd") == 0) { snprintf(buf, sizeof(buf), "kd=%.4f", cfg.pid.kd); emit(buf); return; }
    if (strcasecmp(k, "pedal_s1_min") == 0) { snprintf(buf, sizeof(buf), "pedal_s1_min=%u", cfg.cal.pedal_s1_min); emit(buf); return; }
    if (strcasecmp(k, "pedal_s1_max") == 0) { snprintf(buf, sizeof(buf), "pedal_s1_max=%u", cfg.cal.pedal_s1_max); emit(buf); return; }
    if (strcasecmp(k, "pedal_s2_min") == 0) { snprintf(buf, sizeof(buf), "pedal_s2_min=%u", cfg.cal.pedal_s2_min); emit(buf); return; }
    if (strcasecmp(k, "pedal_s2_max") == 0) { snprintf(buf, sizeof(buf), "pedal_s2_max=%u", cfg.cal.pedal_s2_max); emit(buf); return; }
    if (strcasecmp(k, "etb_pot1_closed") == 0) { snprintf(buf, sizeof(buf), "etb_pot1_closed=%u", cfg.cal.etb_pot1_closed); emit(buf); return; }
    if (strcasecmp(k, "etb_pot1_open")   == 0) { snprintf(buf, sizeof(buf), "etb_pot1_open=%u",   cfg.cal.etb_pot1_open); emit(buf); return; }
    if (strcasecmp(k, "etb_pot2_closed") == 0) { snprintf(buf, sizeof(buf), "etb_pot2_closed=%u", cfg.cal.etb_pot2_closed); emit(buf); return; }
    if (strcasecmp(k, "etb_pot2_open")   == 0) { snprintf(buf, sizeof(buf), "etb_pot2_open=%u",   cfg.cal.etb_pot2_open); emit(buf); return; }
    // Throttle map points (5 break-points, x = pedal %, y = throttle %)
    for (int i = 0; i < 5; i++) {
      char nx[12], ny[12]; snprintf(nx, sizeof(nx), "map_x%d", i); snprintf(ny, sizeof(ny), "map_y%d", i);
      if (strcasecmp(k, nx) == 0) { snprintf(buf, sizeof(buf), "%s=%.4f", nx, cfg.map.pts[i][0]); emit(buf); return; }
      if (strcasecmp(k, ny) == 0) { snprintf(buf, sizeof(buf), "%s=%.4f", ny, cfg.map.pts[i][1]); emit(buf); return; }
    }
    emit("ERR unknown param");
    return;
  }
  if (match(p, "SET")) {
    char* k = next_token(p);
    char* v = next_token(p);
    float fv = atof(v);
    int iv = atoi(v);
    if (strcasecmp(k, "kp") == 0)             { cfg.pid.kp = fv; emit("OK"); return; }
    if (strcasecmp(k, "ki") == 0)             { cfg.pid.ki = fv; emit("OK"); return; }
    if (strcasecmp(k, "kd") == 0)             { cfg.pid.kd = fv; emit("OK"); return; }
    if (strcasecmp(k, "pedal_s1_min") == 0)   { cfg.cal.pedal_s1_min = iv; emit("OK"); return; }
    if (strcasecmp(k, "pedal_s1_max") == 0)   { cfg.cal.pedal_s1_max = iv; emit("OK"); return; }
    if (strcasecmp(k, "pedal_s2_min") == 0)   { cfg.cal.pedal_s2_min = iv; emit("OK"); return; }
    if (strcasecmp(k, "pedal_s2_max") == 0)   { cfg.cal.pedal_s2_max = iv; emit("OK"); return; }
    if (strcasecmp(k, "etb_pot1_closed") == 0) { cfg.cal.etb_pot1_closed = iv; emit("OK"); return; }
    if (strcasecmp(k, "etb_pot1_open") == 0)   { cfg.cal.etb_pot1_open = iv;   emit("OK"); return; }
    if (strcasecmp(k, "etb_pot2_closed") == 0) { cfg.cal.etb_pot2_closed = iv; emit("OK"); return; }
    if (strcasecmp(k, "etb_pot2_open") == 0)   { cfg.cal.etb_pot2_open = iv;   emit("OK"); return; }
    // Throttle map points
    for (int i = 0; i < 5; i++) {
      char nx[12], ny[12]; snprintf(nx, sizeof(nx), "map_x%d", i); snprintf(ny, sizeof(ny), "map_y%d", i);
      if (strcasecmp(k, nx) == 0) { cfg.map.pts[i][0] = fv; emit("OK"); return; }
      if (strcasecmp(k, ny) == 0) { cfg.map.pts[i][1] = fv; emit("OK"); return; }
    }
    emit("ERR unknown param");
    return;
  }
  if (match(p, "TEST")) {
    char* name = next_token(p);
    if (strcasecmp(name, "pedal") == 0) {
      // Print live pedal + ETB readings for 5 s without touching the motor.
      armed = false;
      uint32_t end = millis() + 5000;
      while (millis() < end) {
        char buf[120];
        snprintf(buf, sizeof(buf), "T s1=%u s2=%u etb=%u",
          analogRead(pin::pedal_s1),
          analogRead(pin::pedal_s2),
          analogRead(pin::etb_pot1));
        emit(buf);
        delay(50);
        watchdog_update();
      }
      emit("OK done");
      return;
    }
    if (strcasecmp(name, "sweep") == 0) {
      // Drive ETB through a slow open-close sweep so you can verify the linkage moves.
      // Caller must be sure the engine isn't running before doing this.
      if (fault_latched) { emit("ERR fault latched"); return; }
      armed = true;
      digitalWrite(pin::motor_slp, HIGH);
      for (int duty = 0; duty <= 180; duty += 10) {
        digitalWrite(pin::motor_dir, HIGH);
        analogWrite(pin::motor_pwm, duty);
        delay(80);
        watchdog_update();
      }
      analogWrite(pin::motor_pwm, 0);
      delay(300);
      for (int duty = 0; duty <= 180; duty += 10) {
        digitalWrite(pin::motor_dir, LOW);
        analogWrite(pin::motor_pwm, duty);
        delay(80);
        watchdog_update();
      }
      analogWrite(pin::motor_pwm, 0);
      digitalWrite(pin::motor_slp, LOW);
      armed = false;
      emit("OK swept");
      return;
    }
    emit("ERR unknown test");
    return;
  }

  emit("ERR unknown command");
}
