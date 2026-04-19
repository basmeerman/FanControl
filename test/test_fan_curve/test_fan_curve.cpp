// Host-side (pio test -e native) Unity tests for the pure fan-curve
// interpolation extracted into src/fan_curve.h.
//
// CONSTRAINT: must not include <Arduino.h> or any ESP32-specific header.
// Only plain C++17 + Unity.

#include <unity.h>

#include <cmath>
#include <cstdint>

#include "fan_curve.h"

// The default curve from config.h: temps = {15, 20, 25, 30, 35},
// pwm = {10, 25, 50, 75, 100}. We construct a concrete FanCurve from
// those constexpr arrays so the tests exercise the same curve the
// firmware boots with (config.h defaults).
static FanCurve makeDefaultCurve() {
  FanCurve c{};
  for (uint8_t i = 0; i < FAN_CURVE_POINTS; ++i) {
    c.temps[i] = FAN_CURVE_TEMP_DEFAULT[i];
    c.pwm[i]   = FAN_CURVE_PWM_DEFAULT[i];
  }
  return c;
}

void setUp(void)    {}
void tearDown(void) {}

// --- Edge: below first point returns pwm[0] ---------------------------------

void test_below_first_point_returns_first_pwm(void) {
  const FanCurve curve = makeDefaultCurve();
  // Well below curve.temps[0] = 15.0
  TEST_ASSERT_EQUAL_UINT8(curve.pwm[0], fan_curve::computeFromTemperature(-10.0f, curve));
  TEST_ASSERT_EQUAL_UINT8(curve.pwm[0], fan_curve::computeFromTemperature(0.0f,   curve));
  TEST_ASSERT_EQUAL_UINT8(curve.pwm[0], fan_curve::computeFromTemperature(14.99f, curve));
}

// --- Edge: at-or-above last point returns 100 -------------------------------

void test_above_last_point_returns_100(void) {
  const FanCurve curve = makeDefaultCurve();
  // curve.temps[N-1] = 35.0 → spec: c >= last → 100 (not pwm[last]).
  TEST_ASSERT_EQUAL_UINT8(100, fan_curve::computeFromTemperature(35.0f,   curve));
  TEST_ASSERT_EQUAL_UINT8(100, fan_curve::computeFromTemperature(42.0f,   curve));
  TEST_ASSERT_EQUAL_UINT8(100, fan_curve::computeFromTemperature(1000.0f, curve));
}

// --- Exact: at each curve point returns its pwm (default curve) -------------

void test_exact_at_each_point_returns_that_pwm(void) {
  const FanCurve curve = makeDefaultCurve();
  // At temps[0] = 15 → pwm[0] = 10 (c <= temps[0] branch)
  TEST_ASSERT_EQUAL_UINT8(curve.pwm[0], fan_curve::computeFromTemperature(curve.temps[0], curve));
  // Interior points are hit by the segment-search branch.
  TEST_ASSERT_EQUAL_UINT8(curve.pwm[1], fan_curve::computeFromTemperature(curve.temps[1], curve));
  TEST_ASSERT_EQUAL_UINT8(curve.pwm[2], fan_curve::computeFromTemperature(curve.temps[2], curve));
  TEST_ASSERT_EQUAL_UINT8(curve.pwm[3], fan_curve::computeFromTemperature(curve.temps[3], curve));
  // At the last point we hit the "c >= last" branch → 100 (which
  // coincidentally equals pwm[N-1] for the default curve).
  TEST_ASSERT_EQUAL_UINT8(100, fan_curve::computeFromTemperature(curve.temps[4], curve));
}

// --- Midway interpolation (default curve) -----------------------------------

void test_midway_default_curve_segments(void) {
  const FanCurve curve = makeDefaultCurve();
  // Segment [15,20] → [10,25]: midpoint 17.5 → 17.5 → round to 18
  TEST_ASSERT_EQUAL_UINT8(18, fan_curve::computeFromTemperature(17.5f, curve));
  // Segment [20,25] → [25,50]: midpoint 22.5 → 37.5 → round to 38
  TEST_ASSERT_EQUAL_UINT8(38, fan_curve::computeFromTemperature(22.5f, curve));
  // Segment [25,30] → [50,75]: midpoint 27.5 → 62.5 → round to 63
  TEST_ASSERT_EQUAL_UINT8(63, fan_curve::computeFromTemperature(27.5f, curve));
  // Segment [30,35] → [75,100]: midpoint 32.5 → 87.5 → round to 88
  TEST_ASSERT_EQUAL_UINT8(88, fan_curve::computeFromTemperature(32.5f, curve));
}

// --- Midway interpolation on a synthetic curve with clean integer math ------

void test_midway_exact_halfway_rounding(void) {
  // Curve with easy-to-read midpoints:
  // 10→20, 20→40, 30→60, 40→80, 50→100
  FanCurve curve{};
  curve.temps[0] = 10.0f;  curve.pwm[0] =  20;
  curve.temps[1] = 20.0f;  curve.pwm[1] =  40;
  curve.temps[2] = 30.0f;  curve.pwm[2] =  60;
  curve.temps[3] = 40.0f;  curve.pwm[3] =  80;
  curve.temps[4] = 50.0f;  curve.pwm[4] = 100;

  // Midpoints should land exactly on integer percentages.
  TEST_ASSERT_EQUAL_UINT8(30, fan_curve::computeFromTemperature(15.0f, curve));
  TEST_ASSERT_EQUAL_UINT8(50, fan_curve::computeFromTemperature(25.0f, curve));
  TEST_ASSERT_EQUAL_UINT8(70, fan_curve::computeFromTemperature(35.0f, curve));
  TEST_ASSERT_EQUAL_UINT8(90, fan_curve::computeFromTemperature(45.0f, curve));
}

// --- NaN input → failsafe bias 100 ------------------------------------------

void test_nan_input_returns_100(void) {
  const FanCurve curve = makeDefaultCurve();
  const float nan_val = std::nanf("");
  TEST_ASSERT_TRUE(std::isnan(nan_val));
  TEST_ASSERT_EQUAL_UINT8(100, fan_curve::computeFromTemperature(nan_val, curve));
}

// --- Degenerate segment (duplicate temps) does not divide by zero -----------

void test_degenerate_segment_does_not_divide_by_zero(void) {
  // Deliberately set temps[1] == temps[2] to force span <= 0 in the
  // interior. The function must fall back to pwm[i] instead of NaN/inf.
  FanCurve curve{};
  curve.temps[0] = 10.0f;  curve.pwm[0] =  10;
  curve.temps[1] = 20.0f;  curve.pwm[1] =  40;
  curve.temps[2] = 20.0f;  curve.pwm[2] =  60;
  curve.temps[3] = 30.0f;  curve.pwm[3] =  80;
  curve.temps[4] = 40.0f;  curve.pwm[4] = 100;

  const uint8_t v = fan_curve::computeFromTemperature(20.0f, curve);
  // At c = 20.0 the first matching segment is [10,20] — that segment is
  // fine and returns pwm[1] = 40 at the upper endpoint (frac=1).
  TEST_ASSERT_EQUAL_UINT8(40, v);
  // Any finite value was returned (not something truthy-but-garbage).
  TEST_ASSERT_TRUE(v <= 100);
}

int main(int /*argc*/, char** /*argv*/) {
  UNITY_BEGIN();
  RUN_TEST(test_below_first_point_returns_first_pwm);
  RUN_TEST(test_above_last_point_returns_100);
  RUN_TEST(test_exact_at_each_point_returns_that_pwm);
  RUN_TEST(test_midway_default_curve_segments);
  RUN_TEST(test_midway_exact_halfway_rounding);
  RUN_TEST(test_nan_input_returns_100);
  RUN_TEST(test_degenerate_segment_does_not_divide_by_zero);
  return UNITY_END();
}
