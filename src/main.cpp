#include <Arduino.h>
#include "config.h"
#include "version.h"
#include "storage.h"
#include "sensor.h"
#include "fan.h"
#include "watchdog.h"
#include "wifi_manager.h"
#include "webserver.h"
#include "websocket.h"
#include "mqtt.h"

// Phase 4 integration: real orchestration. All work happens in FreeRTOS
// tasks per CLAUDE.md (no blocking calls in loop(); WS + MQTT in
// separate tasks). Network/IO tasks pin to core 0 (WiFi/AsyncTCP live
// there); sensor/fan compute pins to core 1.

namespace {

FanCurve g_curve;

// ----- compute tasks (core 1) -----

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
      watchdog::handleSensorStall();
    } else {
      const float   t   = sensor::getTemperatureC();
      const uint8_t pct = fan::computeFromTemperature(t, g_curve);
      fan::setPercent(pct);
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
  wifi_manager::begin();
  webserver::begin();          // creates AsyncWebServer + mounts ElegantOTA + magic URLs
  ws::attach(webserver::instance());  // mount /ws on the same server
  mqtt::begin();               // safe even if WiFi not yet up — task handles it

  g_curve = storage::loadFanCurve();

  const String   dev      = storage::loadDeviceName();
  const uint32_t restarts = watchdog::restartCount();
  Serial.printf("\n[FanControl] version %s built %s\n", VERSION, BUILD_DATE);
  Serial.printf("[FanControl] device=%s  restarts=%u  pwm=%u Hz  min=%u%%\n",
                dev.c_str(), restarts, fan::currentFrequency(),
                storage::loadFanMinPercent());
  Serial.printf("[FanControl] wifi=%s  ip=%s  portal=%s\n",
                wifi_manager::currentSsid().c_str(),
                wifi_manager::ipAddress().c_str(),
                wifi_manager::isPortalActive() ? "active" : "off");

  // Compute tasks on core 1.
  xTaskCreatePinnedToCore(sensorTask,         "sensorTask",   4096, nullptr, 5, nullptr, 1);
  xTaskCreatePinnedToCore(fanTask,            "fanTask",      4096, nullptr, 4, nullptr, 1);
  // Watchdog stays on core 0 (PRO core), highest priority.
  xTaskCreatePinnedToCore(watchdogTask,       "watchdogTask", 4096, nullptr, 6, nullptr, 0);
  // Network tasks on core 0 next to the WiFi/AsyncTCP stack.
  // Larger stacks for JSON building + TLS handshakes.
  xTaskCreatePinnedToCore(wifi_manager::task, "wifiTask",     4096, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(mqtt::task,         "mqttTask",     6144, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(ws::task,           "wsTask",       6144, nullptr, 3, nullptr, 0);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
