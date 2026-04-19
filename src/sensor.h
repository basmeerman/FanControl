#pragma once

// DHT22 driver wrapper. Keeps the last-good reading so the fan task can
// always consult a value, and exposes freshness so the watchdog can
// decide to failsafe when the sensor stalls.
//
// All access is single-threaded from the sensorTask (see main.cpp).
// Getter/freshness calls from other tasks are safe because they only
// read simple POD scalars after an atomic memcpy on 32-bit aligned
// floats/uint32 — good enough for monitoring purposes on ESP32.

#include <Arduino.h>
#include "config.h"

namespace sensor {

  // Set up the DHT library on PIN_DHT22. Safe to call once from setup().
  void begin();

  // Attempt a read. Returns true on success and updates the last-good
  // cached temperature/humidity and the freshness timestamp. A failed
  // read leaves the previous values intact — that is the whole point of
  // keeping a last-good reading for the fan curve.
  bool read();

  // Last-good readings. Returns NAN before the first successful read.
  float getTemperatureC();
  float getHumidityPct();

  // Milliseconds since the last successful read. Returns UINT32_MAX if
  // no successful read has happened yet, which naturally trips isStale().
  uint32_t lastReadAgeMs();

  // True when no successful read has occurred within SENSOR_STALL_TIMEOUT_MS.
  bool isStale();

} // namespace sensor
