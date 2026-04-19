#include "fan.h"
#include <math.h>

namespace {

uint32_t s_freqHz        = FAN_PWM_FREQ_DEFAULT_HZ;
uint8_t  s_minPercent    = FAN_MIN_PERCENT_DEFAULT;
uint8_t  s_lastPercent   = 0;
bool     s_failsafeActive = false;

inline uint32_t dutyForPercent(uint8_t pct) {
  // Resolution is FAN_PWM_RES_BITS → full scale = (1 << bits) - 1.
  const uint32_t maxDuty = (1UL << FAN_PWM_RES_BITS) - 1UL;
  return (uint32_t)pct * maxDuty / 100UL;
}

void applyDuty(uint8_t pct) {
  ledcWrite(FAN_LEDC_CHANNEL, dutyForPercent(pct));
  s_lastPercent = pct;
}

} // namespace

namespace fan {

void begin() {
  s_freqHz     = storage::loadFanPwmFreqHz();
  s_minPercent = storage::loadFanMinPercent();
  if (s_minPercent > 100) s_minPercent = FAN_MIN_PERCENT_DEFAULT;

  ledcSetup(FAN_LEDC_CHANNEL, s_freqHz, FAN_PWM_RES_BITS);
  ledcAttachPin(PIN_FAN_PWM, FAN_LEDC_CHANNEL);
  applyDuty(s_minPercent);
  log_i("Fan PWM: pin=%u ch=%u freq=%u Hz res=%u bits min=%u%%",
        PIN_FAN_PWM, FAN_LEDC_CHANNEL, s_freqHz, FAN_PWM_RES_BITS, s_minPercent);
}

void setPercent(uint8_t pct) {
  if (s_failsafeActive) {
    // A previous failsafe() latched — releasing it is an explicit action.
    // Exiting failsafe requires a successful setPercent() call *after*
    // clearing: we clear here on first non-failsafe call to let the
    // fan task resume automatic control once the sensor recovers.
    s_failsafeActive = false;
  }
  if (pct > 100) pct = 100;
  if (pct < s_minPercent) pct = s_minPercent;
  applyDuty(pct);
}

bool setFrequency(uint32_t hz) {
  if (hz < FAN_PWM_FREQ_MIN_HZ || hz > FAN_PWM_FREQ_MAX_HZ) {
    log_w("setFrequency: %u Hz out of range [%u..%u]", hz,
          FAN_PWM_FREQ_MIN_HZ, FAN_PWM_FREQ_MAX_HZ);
    return false;
  }
  const uint8_t keep = s_lastPercent;
  s_freqHz = hz;
  ledcSetup(FAN_LEDC_CHANNEL, s_freqHz, FAN_PWM_RES_BITS);
  // ledcSetup resets the duty; reapply. No reboot required.
  applyDuty(keep);
  log_i("Fan PWM frequency now %u Hz", s_freqHz);
  return true;
}

void failsafe() {
  s_failsafeActive = true;
  ledcWrite(FAN_LEDC_CHANNEL, dutyForPercent(FAN_FAILSAFE_PERCENT));
  s_lastPercent = FAN_FAILSAFE_PERCENT;
  log_w("Fan FAILSAFE engaged -> %u %%", FAN_FAILSAFE_PERCENT);
}

void setMinPercent(uint8_t pct) {
  if (pct > 100) pct = 100;
  s_minPercent = pct;
}

uint8_t currentPercent() {
  return s_lastPercent;
}

uint32_t currentFrequency() {
  return s_freqHz;
}

uint8_t computeFromTemperature(float c, const FanCurve& curve) {
  if (isnan(c)) {
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
      const float p0   = (float)curve.pwm[i];
      const float p1   = (float)curve.pwm[i + 1];
      float       pct  = p0 + frac * (p1 - p0);
      if (pct < 0.0f)   pct = 0.0f;
      if (pct > 100.0f) pct = 100.0f;
      return (uint8_t)(pct + 0.5f);
    }
  }
  // Unreachable given the two endpoint checks above, but keep the fan moving.
  return 100;
}

} // namespace fan
