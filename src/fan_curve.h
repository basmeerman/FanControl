#pragma once

// Pure fan-curve interpolation, extracted from fan.cpp so it can be unit
// tested on the host (pio test -e native) without dragging in Arduino /
// ESP32 dependencies.
//
// CONSTRAINT: this header must remain plain C++17 — do NOT include
// <Arduino.h> and do NOT reference any ESP32-specific symbol here. The
// firmware build pulls this in via fan.h / storage.h; the host test
// build pulls it in directly. Both need to compile.
//
// Behaviour (contract tested by test/test_fan_curve/):
//   - NaN input → 100 (failsafe bias; the caller still applies the
//     min-percent floor via fan::setPercent()).
//   - c <= curve.temps[0]                → curve.pwm[0]
//   - c >= curve.temps[FAN_CURVE_POINTS-1] → 100
//   - otherwise linear interpolation between the two bracketing points,
//     rounded to the nearest integer percent and clamped to 0..100.

#include <cmath>
#include <cstdint>

#include "config.h"   // FAN_CURVE_POINTS — plain constexpr, no Arduino deps

struct FanCurve {
  float   temps[FAN_CURVE_POINTS];
  uint8_t pwm[FAN_CURVE_POINTS];
};

namespace fan_curve {

  inline uint8_t computeFromTemperature(float c, const FanCurve& curve) {
    if (std::isnan(c)) {
      // Caller hasn't seen a reading yet; bias high but not failsafe.
      // The fan task will still apply the min-percent floor via setPercent.
      return 100;
    }

    // Below first point → the first point's PWM value (the caller lifts
    // it up to fan_min via setPercent()'s floor).
    if (c <= curve.temps[0]) {
      return curve.pwm[0];
    }
    // Above last point → 100 %.
    if (c >= curve.temps[FAN_CURVE_POINTS - 1]) {
      return 100;
    }

    // Find the segment containing c and linearly interpolate.
    for (uint8_t i = 0; i < FAN_CURVE_POINTS - 1; ++i) {
      const float t0 = curve.temps[i];
      const float t1 = curve.temps[i + 1];
      if (c >= t0 && c <= t1) {
        const float span = t1 - t0;
        if (span <= 0.0f) return curve.pwm[i];  // guard: badly sorted curve
        const float frac = (c - t0) / span;
        const float p0   = static_cast<float>(curve.pwm[i]);
        const float p1   = static_cast<float>(curve.pwm[i + 1]);
        float       pct  = p0 + frac * (p1 - p0);
        if (pct < 0.0f)   pct = 0.0f;
        if (pct > 100.0f) pct = 100.0f;
        return static_cast<uint8_t>(pct + 0.5f);
      }
    }
    // Unreachable given the two endpoint checks above, but keep the fan moving.
    return 100;
  }

} // namespace fan_curve
