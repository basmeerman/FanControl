#pragma once

// FanControl — compile-time defaults and pin map.
// Single source of truth per CLAUDE.md. Runtime-tunable values are
// mirrored in NVS (see storage.h) and loaded at boot.

#include <stdint.h>

// ---------- GPIO (LOLIN D32) ----------
// DHT22 data line (any GPIO with pull-up; avoid strapping pins 0/2/12/15)
static constexpr uint8_t PIN_DHT22        = 4;
// PWM output to fan driver (level shifter → MT3608 → 9V PWM)
static constexpr uint8_t PIN_FAN_PWM      = 25;
// On-board LED (LOLIN D32 active-low)
static constexpr uint8_t PIN_STATUS_LED   = 5;
// Boot button used for factory reset (hold >5s at boot)
static constexpr uint8_t PIN_FACTORY_RESET = 0;

// ---------- Fan PWM ----------
static constexpr uint8_t  FAN_LEDC_CHANNEL = 0;
static constexpr uint32_t FAN_PWM_FREQ_HZ  = 2000;   // 1–5 kHz range per spec
static constexpr uint8_t  FAN_PWM_RES_BITS = 10;     // 0..1023
static constexpr uint8_t  FAN_MIN_PERCENT_DEFAULT = 10;
static constexpr uint8_t  FAN_FAILSAFE_PERCENT    = 100;

// ---------- Temperature curve (°C → % PWM) ----------
// Linear interpolation between points. Below T0 → MIN, above T4 → 100.
static constexpr uint8_t FAN_CURVE_POINTS = 5;
static constexpr float   FAN_CURVE_TEMP_DEFAULT[FAN_CURVE_POINTS] = {15, 20, 25, 30, 35};
static constexpr uint8_t FAN_CURVE_PWM_DEFAULT[FAN_CURVE_POINTS]  = {10, 25, 50, 75, 100};
static constexpr float   ALARM_TEMP_DEFAULT = 35.0f;

// ---------- Timing (ms) ----------
static constexpr uint32_t SENSOR_READ_INTERVAL_MS   = 5000;
static constexpr uint32_t SENSOR_STALL_TIMEOUT_MS   = 60000;  // SW watchdog
static constexpr uint32_t WS_PUSH_INTERVAL_MS       = 2000;
static constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS  = 15000;
static constexpr uint32_t MQTT_RECONNECT_MIN_MS     = 1000;
static constexpr uint32_t MQTT_RECONNECT_MAX_MS     = 60000;
static constexpr uint32_t WIFI_RETRY_INTERVAL_MS    = 30000;
static constexpr uint32_t RESTART_COOLDOWN_MS       = 5UL * 60UL * 1000UL;

// ---------- Hardware watchdog ----------
static constexpr uint32_t TWDT_TIMEOUT_S = 30;

// ---------- Defaults for NVS-backed config ----------
static constexpr char     DEFAULT_DEVICE_NAME[]   = "fancontrol";
static constexpr char     DEFAULT_MDNS_HOSTNAME[] = "fancontrol";
static constexpr char     DEFAULT_MQTT_PREFIX[]   = "fancontrol";
static constexpr uint16_t DEFAULT_MQTT_PORT       = 1883;
static constexpr char     DEFAULT_OTA_PASSWORD[]  = "changeme";

// ---------- NVS namespaces + keys ----------
// Keep keys ≤15 chars (NVS limit).
static constexpr char NVS_NAMESPACE[] = "fancontrol";

namespace nvs_key {
  static constexpr char WIFI_SSID[]      = "wifi_ssid";
  static constexpr char WIFI_PASS[]      = "wifi_pass";
  static constexpr char MQTT_HOST[]      = "mqtt_host";
  static constexpr char MQTT_PORT[]      = "mqtt_port";
  static constexpr char MQTT_USER[]      = "mqtt_user";
  static constexpr char MQTT_PASS[]      = "mqtt_pass";
  static constexpr char MQTT_PREFIX[]    = "mqtt_prefix";
  static constexpr char DEVICE_NAME[]    = "dev_name";
  static constexpr char OTA_PASS[]       = "ota_pass";
  static constexpr char FAN_CURVE_T[]    = "fan_curve_t";   // blob: float[5]
  static constexpr char FAN_CURVE_P[]    = "fan_curve_p";   // blob: uint8_t[5]
  static constexpr char ALARM_TEMP[]     = "alarm_temp";
  static constexpr char FAN_MIN_PCT[]    = "fan_min_pct";
  static constexpr char SENSOR_INT_MS[]  = "sensor_int";
  static constexpr char RESTART_COUNT[]  = "restart_cnt";
  static constexpr char LAST_RESTART_MS[] = "last_restart";
}
