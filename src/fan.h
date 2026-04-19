#pragma once

// Fan PWM driver (LEDC) and temperature-curve resolver.
//
// Frequency is runtime-tunable per plan §F1.1 — setFrequency() must work
// without reboot and always clamps to [FAN_PWM_FREQ_MIN_HZ..MAX_HZ].
//
// Failsafe is 100 % (CLAUDE.md rule) and bypasses the min-percent floor.

#include <Arduino.h>
#include "config.h"
#include "storage.h"   // FanCurve

namespace fan {

  // One-shot init. Loads NVS-saved frequency + min-percent floor and
  // configures LEDC channel FAN_LEDC_CHANNEL on PIN_FAN_PWM.
  void begin();

  // Apply a percentage 0..100. Values are clamped. Under normal
  // operation the configured min-percent floor is applied so the fan
  // never drops to 0. Call failsafe() instead if you actually want 100 %.
  void setPercent(uint8_t pct);

  // Change the PWM frequency at runtime. Returns false on out-of-range.
  // Re-invokes ledcSetup on the channel and re-applies the last duty.
  bool setFrequency(uint32_t hz);

  // Force 100 % regardless of the min-percent floor. Used on sensor
  // stall / watchdog alarms.
  void failsafe();

  // Update the in-memory min-percent floor (not NVS-persisted here —
  // storage owns that). Affects subsequent setPercent() calls.
  void setMinPercent(uint8_t pct);

  // Last applied percentage (post-clamp / floor). Useful for MQTT + WS.
  uint8_t currentPercent();

  // Current PWM frequency in Hz.
  uint32_t currentFrequency();

  // Linear interpolation over the fan curve. Returns a PWM percentage.
  // Below curve.temps[0] → max(curve.pwm[0], fan_min) under normal ops.
  // Above curve.temps[N-1] → 100. The caller is responsible for
  // applying min-percent floor via setPercent(); this function returns
  // the raw curve value and only enforces the two end-clamps.
  uint8_t computeFromTemperature(float c, const FanCurve& curve);

} // namespace fan
