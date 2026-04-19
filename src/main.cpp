#include <Arduino.h>
#include "config.h"
#include "version.h"
#include "storage.h"
#include "sensor.h"
#include "fan.h"
#include "watchdog.h"

// Phase 1: real orchestration. All work happens in FreeRTOS tasks;
// loop() stays idle per CLAUDE.md (WebSocket + MQTT tasks arrive in
// later phases and plug into this same scheduler).

namespace {

FanCurve g_curve;

void sensorTask(void* /*arg*/) {
  watchdog::subscribeCurrentTask();
  const uint32_t interval = storage::loadSensorIntervalMs();
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    watchdog::reset();
    if (sensor::read()) {
      watchdog::feedSensor();
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(interval));
  }
}

void fanTask(void* /*arg*/) {
  watchdog::subscribeCurrentTask();
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    watchdog::reset();

    if (sensor::isStale()) {
      // SW-watchdog policy: engage failsafe regardless of temperature.
      watchdog::handleSensorStall();
    } else {
      const float   t   = sensor::getTemperatureC();
      const uint8_t pct = fan::computeFromTemperature(t, g_curve);
      fan::setPercent(pct);  // applies min-percent floor
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
  }
}

void watchdogTask(void* /*arg*/) {
  watchdog::subscribeCurrentTask();
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    watchdog::reset();
    if (watchdog::sensorStall()) {
      watchdog::handleSensorStall();
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  // Brief wait for USB CDC to come up before the first banner line.
  // In setup() this is allowed; loop() must stay non-blocking.
  const uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 500) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (!storage::begin()) {
    Serial.println("[FanControl] NVS init failed — continuing with defaults");
  }

  sensor::begin();
  fan::begin();
  watchdog::begin();

  // Load fan curve once; the fan task reads it every second.
  g_curve = storage::loadFanCurve();

  const String   dev      = storage::loadDeviceName();
  const uint32_t restarts = watchdog::restartCount();
  Serial.printf("\n[FanControl] version %s built %s\n", VERSION, BUILD_DATE);
  Serial.printf("[FanControl] device=%s  restarts=%u  pwm=%u Hz  min=%u%%\n",
                dev.c_str(), restarts, fan::currentFrequency(),
                storage::loadFanMinPercent());

  // Spawn tasks. Priorities: watchdog > sensor > fan > idle.
  xTaskCreatePinnedToCore(sensorTask,   "sensorTask",   4096, nullptr, 5, nullptr, 1);
  xTaskCreatePinnedToCore(fanTask,      "fanTask",      4096, nullptr, 4, nullptr, 1);
  xTaskCreatePinnedToCore(watchdogTask, "watchdogTask", 4096, nullptr, 6, nullptr, 0);
}

void loop() {
  // All real work runs in the tasks above. Park loop() indefinitely so
  // the Arduino loopTask doesn't spin.
  vTaskDelay(portMAX_DELAY);
}
