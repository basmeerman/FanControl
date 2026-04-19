#pragma once

// Hardware TWDT + software sensor-stall watchdog + restart bookkeeping.
//
// Plan §F2: max 1 auto-restart per 5 min (cooldown in NVS), persistent
// restart counter, fail-safe fan to 100 % on sensor stall.

#include <Arduino.h>
#include "config.h"

namespace watchdog {

  // Initialise the TWDT and increment the persistent restart counter.
  // Logs a cooldown warning if the previous restart was within
  // RESTART_COOLDOWN_MS.
  void begin();

  // Register the calling task with the hardware TWDT. Call from the top
  // of each long-running task before its loop starts.
  void subscribeCurrentTask();

  // Reset the hardware TWDT for the current task. Call from every
  // registered task at least once per TWDT_TIMEOUT_S.
  void reset();

  // Record a successful sensor read. Called by the sensor task.
  void feedSensor();

  // True when the sensor hasn't been fed in SENSOR_STALL_TIMEOUT_MS.
  bool sensorStall();

  // One-shot: engages fan failsafe and flags the alarm. Safe to call
  // more than once; only logs the first transition.
  void handleSensorStall();

  // Ask for a controlled restart. Honours the 5-minute NVS cooldown:
  // repeated calls within the cooldown window are swallowed and logged.
  // On success the function does not return (ESP.restart()).
  void requestRestart(const char* reason);

  // Accessors for UI / MQTT status.
  bool     alarmActive();
  uint32_t restartCount();

} // namespace watchdog
