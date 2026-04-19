#include "fan.h"

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
  // Pure logic lives in fan_curve.h so it can be unit tested on the host.
  // This thin wrapper exists so callers can keep using fan::computeFromTemperature().
  return fan_curve::computeFromTemperature(c, curve);
}

} // namespace fan
