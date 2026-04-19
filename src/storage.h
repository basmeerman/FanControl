#pragma once

// NVS-backed configuration. Thin wrapper around Preferences.h so that
// no other module includes Preferences directly (CLAUDE.md rule).
// Every writer returns a bool — callers MUST check and log failures.

#include <Arduino.h>
#include "config.h"

struct FanCurve {
  float   temps[FAN_CURVE_POINTS];
  uint8_t pwm[FAN_CURVE_POINTS];
};

struct WifiConfig {
  String ssid;
  String password;
};

struct MqttConfig {
  String   host;
  uint16_t port;
  String   user;
  String   password;
  String   prefix;
};

namespace storage {

  // Mount the NVS namespace. Call once from setup() before any getter.
  bool begin();

  // --- WiFi / network ---
  WifiConfig loadWifi();
  bool       saveWifi(const WifiConfig& cfg);

  // --- MQTT ---
  MqttConfig loadMqtt();
  bool       saveMqtt(const MqttConfig& cfg);

  // --- Fan curve + thresholds ---
  FanCurve loadFanCurve();
  bool     saveFanCurve(const FanCurve& curve);

  float   loadAlarmTemp();
  bool    saveAlarmTemp(float celsius);

  uint8_t loadFanMinPercent();
  bool    saveFanMinPercent(uint8_t pct);

  uint32_t loadFanPwmFreqHz();
  bool     saveFanPwmFreqHz(uint32_t hz);

  uint32_t loadSensorIntervalMs();
  bool     saveSensorIntervalMs(uint32_t ms);

  // --- Device identity ---
  String loadDeviceName();
  bool   saveDeviceName(const String& name);

  String loadOtaPassword();
  bool   saveOtaPassword(const String& pass);

  // --- Watchdog bookkeeping ---
  uint32_t loadRestartCount();
  bool     incrementRestartCount();

  uint32_t loadLastRestartEpochMs();
  bool     saveLastRestartEpochMs(uint32_t ms);

  // --- Factory reset: wipe the namespace. ---
  bool factoryReset();

} // namespace storage
