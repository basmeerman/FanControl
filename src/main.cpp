#include <Arduino.h>
#include "config.h"
#include "version.h"
#include "storage.h"

// Phase 1 scaffold — wires up NVS init only.
// Fan, sensor, watchdog, MQTT, web come online in subsequent phases.

void setup() {
  Serial.begin(115200);
  delay(200);  // allow USB CDC to enumerate; safe in setup(), never in loop().
  Serial.printf("\n[FanControl] version %s built %s\n", VERSION, BUILD_DATE);

  if (!storage::begin()) {
    Serial.println("[FanControl] NVS init failed — continuing with defaults");
  }

  const String dev = storage::loadDeviceName();
  const uint32_t restarts = storage::loadRestartCount();
  Serial.printf("[FanControl] device=%s  restarts=%u\n", dev.c_str(), restarts);
}

void loop() {
  // Phase 1: idle. Real task orchestration arrives with watchdog + FreeRTOS tasks.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
