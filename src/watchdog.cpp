#include "watchdog.h"
#include "storage.h"
#include "fan.h"
#include "sensor.h"

#include <esp_task_wdt.h>

namespace {

uint32_t s_lastSensorFeedMs = 0;
bool     s_sensorEverFed    = false;
bool     s_alarmActive      = false;
uint32_t s_restartCount     = 0;
bool     s_cooldownActive   = false;

} // namespace

namespace watchdog {

void begin() {
  // Increment persistent restart counter. Check cooldown window.
  if (!storage::incrementRestartCount()) {
    log_e("Could not increment restart counter in NVS");
  }
  s_restartCount = storage::loadRestartCount();

  const uint32_t lastRestart = storage::loadLastRestartEpochMs();
  const uint32_t now         = millis();
  // lastRestart stored as millis() uptime from a previous run — after
  // reboot millis() starts at 0 so any non-zero stored value is "past
  // boot of previous run". The cooldown check is therefore based on a
  // coarse wall-clock proxy: we treat any stored value within the
  // cooldown window *ahead* of now as the signal that another restart
  // happened very recently.
  //
  // Without RTC/NTP we can only approximate. If the previous run stored
  // a value and we are within RESTART_COOLDOWN_MS of boot, warn and
  // refuse further auto-restarts until cooldown elapses.
  if (lastRestart != 0 && now < RESTART_COOLDOWN_MS) {
    log_w("Rapid restart detected (count=%u) — cooldown active for %u ms",
          s_restartCount, (unsigned)(RESTART_COOLDOWN_MS - now));
    s_cooldownActive = true;
  }

  // Hardware TWDT. Prefer the modern config-struct API; fall back to
  // the legacy overload if the struct field isn't available.
#if defined(ESP_TASK_WDT_DEFAULT_CONFIG)
  const esp_task_wdt_config_t cfg = {
    .timeout_ms    = TWDT_TIMEOUT_S * 1000U,
    .idle_core_mask = 0,   // don't subscribe idle tasks; we register our own
    .trigger_panic = true,
  };
  esp_err_t err = esp_task_wdt_init(&cfg);
  if (err == ESP_ERR_INVALID_STATE) {
    // Already initialised by Arduino core — reconfigure instead.
    err = esp_task_wdt_reconfigure(&cfg);
  }
  if (err != ESP_OK) {
    log_e("esp_task_wdt_init failed: %d", (int)err);
  }
#else
  // Legacy signature (older cores): (timeout_seconds, panic_on_timeout)
  const esp_err_t err = esp_task_wdt_init(TWDT_TIMEOUT_S, true);
  if (err != ESP_OK) {
    log_e("esp_task_wdt_init(legacy) failed: %d", (int)err);
  }
#endif

  log_i("Watchdog ready: TWDT=%u s  restarts=%u  cooldown=%s",
        TWDT_TIMEOUT_S, s_restartCount, s_cooldownActive ? "yes" : "no");
}

void subscribeCurrentTask() {
  const esp_err_t err = esp_task_wdt_add(nullptr);
  if (err != ESP_OK && err != ESP_ERR_INVALID_ARG) {
    log_w("esp_task_wdt_add returned %d", (int)err);
  }
}

void reset() {
  esp_task_wdt_reset();
}

void feedSensor() {
  s_lastSensorFeedMs = millis();
  s_sensorEverFed    = true;
  if (s_alarmActive) {
    log_i("Sensor recovered — clearing stall alarm");
    s_alarmActive = false;
  }
}

bool sensorStall() {
  if (!s_sensorEverFed) {
    // Treat pre-first-read as potentially stale only after the timeout
    // has elapsed since boot — avoids a spurious alarm during warm-up.
    return millis() > SENSOR_STALL_TIMEOUT_MS;
  }
  return (millis() - s_lastSensorFeedMs) > SENSOR_STALL_TIMEOUT_MS;
}

void handleSensorStall() {
  if (!s_alarmActive) {
    log_e("Sensor stall (>%u ms) — engaging fan failsafe", SENSOR_STALL_TIMEOUT_MS);
    s_alarmActive = true;
  }
  fan::failsafe();
}

void requestRestart(const char* reason) {
  if (s_cooldownActive) {
    log_w("Auto-restart suppressed (cooldown): reason=%s", reason ? reason : "?");
    return;
  }
  log_w("Restart requested: %s", reason ? reason : "(none)");
  // Record the restart timestamp so the next boot can detect rapid cycling.
  if (!storage::saveLastRestartEpochMs(millis())) {
    log_e("Failed to persist last-restart timestamp");
  }
  // Brief flush opportunity for any pending serial output.
  Serial.flush();
  ESP.restart();
}

bool alarmActive() {
  return s_alarmActive;
}

uint32_t restartCount() {
  return s_restartCount;
}

} // namespace watchdog
