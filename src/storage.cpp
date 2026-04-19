#include "storage.h"
#include <Preferences.h>

namespace {

Preferences prefs;

// Open RW, run op, close. Returns op's result.
// Logs the NVS key on any failed write.
template <typename Fn>
bool withRW(const char* tag, Fn op) {
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) {
    log_e("NVS begin(RW) failed for %s", tag);
    return false;
  }
  const bool ok = op(prefs);
  prefs.end();
  if (!ok) log_e("NVS write failed: %s", tag);
  return ok;
}

template <typename Fn>
auto withRO(Fn op) -> decltype(op(prefs)) {
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
  auto v = op(prefs);
  prefs.end();
  return v;
}

} // namespace

namespace storage {

bool begin() {
  // Ensure namespace exists by opening RW once.
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    log_e("NVS namespace init failed");
    return false;
  }
  prefs.end();
  return true;
}

// -------- WiFi --------
WifiConfig loadWifi() {
  return withRO([](Preferences& p) {
    WifiConfig c;
    c.ssid     = p.getString(nvs_key::WIFI_SSID, "");
    c.password = p.getString(nvs_key::WIFI_PASS, "");
    return c;
  });
}

bool saveWifi(const WifiConfig& cfg) {
  return withRW("wifi", [&](Preferences& p) {
    const bool a = p.putString(nvs_key::WIFI_SSID, cfg.ssid)     == cfg.ssid.length();
    const bool b = p.putString(nvs_key::WIFI_PASS, cfg.password) == cfg.password.length();
    return a && b;
  });
}

// -------- MQTT --------
MqttConfig loadMqtt() {
  return withRO([](Preferences& p) {
    MqttConfig c;
    c.host     = p.getString(nvs_key::MQTT_HOST, "");
    c.port     = p.getUShort(nvs_key::MQTT_PORT, DEFAULT_MQTT_PORT);
    c.user     = p.getString(nvs_key::MQTT_USER, "");
    c.password = p.getString(nvs_key::MQTT_PASS, "");
    c.prefix   = p.getString(nvs_key::MQTT_PREFIX, DEFAULT_MQTT_PREFIX);
    return c;
  });
}

bool saveMqtt(const MqttConfig& cfg) {
  return withRW("mqtt", [&](Preferences& p) {
    const size_t n1 = p.putString(nvs_key::MQTT_HOST,   cfg.host);
    const bool   ok2 = p.putUShort(nvs_key::MQTT_PORT,  cfg.port) == sizeof(uint16_t);
    const size_t n3 = p.putString(nvs_key::MQTT_USER,   cfg.user);
    const size_t n4 = p.putString(nvs_key::MQTT_PASS,   cfg.password);
    const size_t n5 = p.putString(nvs_key::MQTT_PREFIX, cfg.prefix);
    return n1 == cfg.host.length() && ok2 &&
           n3 == cfg.user.length() && n4 == cfg.password.length() &&
           n5 == cfg.prefix.length();
  });
}

// -------- Fan curve --------
FanCurve loadFanCurve() {
  FanCurve c{};
  for (uint8_t i = 0; i < FAN_CURVE_POINTS; ++i) {
    c.temps[i] = FAN_CURVE_TEMP_DEFAULT[i];
    c.pwm[i]   = FAN_CURVE_PWM_DEFAULT[i];
  }
  withRO([&](Preferences& p) {
    p.getBytes(nvs_key::FAN_CURVE_T, c.temps, sizeof(c.temps));
    p.getBytes(nvs_key::FAN_CURVE_P, c.pwm,   sizeof(c.pwm));
    return 0;
  });
  return c;
}

bool saveFanCurve(const FanCurve& curve) {
  return withRW("fan_curve", [&](Preferences& p) {
    const size_t nt = p.putBytes(nvs_key::FAN_CURVE_T, curve.temps, sizeof(curve.temps));
    const size_t np = p.putBytes(nvs_key::FAN_CURVE_P, curve.pwm,   sizeof(curve.pwm));
    return nt == sizeof(curve.temps) && np == sizeof(curve.pwm);
  });
}

// -------- Scalar settings --------
float loadAlarmTemp() {
  return withRO([](Preferences& p) {
    return p.getFloat(nvs_key::ALARM_TEMP, ALARM_TEMP_DEFAULT);
  });
}
bool saveAlarmTemp(float c) {
  return withRW("alarm_temp", [&](Preferences& p) {
    return p.putFloat(nvs_key::ALARM_TEMP, c) == sizeof(float);
  });
}

uint8_t loadFanMinPercent() {
  return withRO([](Preferences& p) {
    return p.getUChar(nvs_key::FAN_MIN_PCT, FAN_MIN_PERCENT_DEFAULT);
  });
}
bool saveFanMinPercent(uint8_t pct) {
  return withRW("fan_min", [&](Preferences& p) {
    return p.putUChar(nvs_key::FAN_MIN_PCT, pct) == sizeof(uint8_t);
  });
}

uint32_t loadFanPwmFreqHz() {
  uint32_t hz = withRO([](Preferences& p) {
    return p.getUInt(nvs_key::FAN_PWM_FREQ, FAN_PWM_FREQ_DEFAULT_HZ);
  });
  if (hz < FAN_PWM_FREQ_MIN_HZ) hz = FAN_PWM_FREQ_MIN_HZ;
  if (hz > FAN_PWM_FREQ_MAX_HZ) hz = FAN_PWM_FREQ_MAX_HZ;
  return hz;
}
bool saveFanPwmFreqHz(uint32_t hz) {
  if (hz < FAN_PWM_FREQ_MIN_HZ || hz > FAN_PWM_FREQ_MAX_HZ) return false;
  return withRW("fan_pwm_freq", [&](Preferences& p) {
    return p.putUInt(nvs_key::FAN_PWM_FREQ, hz) == sizeof(uint32_t);
  });
}

uint32_t loadSensorIntervalMs() {
  return withRO([](Preferences& p) {
    return p.getUInt(nvs_key::SENSOR_INT_MS, SENSOR_READ_INTERVAL_MS);
  });
}
bool saveSensorIntervalMs(uint32_t ms) {
  return withRW("sensor_int", [&](Preferences& p) {
    return p.putUInt(nvs_key::SENSOR_INT_MS, ms) == sizeof(uint32_t);
  });
}

// -------- Identity --------
String loadDeviceName() {
  return withRO([](Preferences& p) {
    return p.getString(nvs_key::DEVICE_NAME, DEFAULT_DEVICE_NAME);
  });
}
bool saveDeviceName(const String& name) {
  return withRW("dev_name", [&](Preferences& p) {
    return p.putString(nvs_key::DEVICE_NAME, name) == name.length();
  });
}

String loadOtaPassword() {
  return withRO([](Preferences& p) {
    return p.getString(nvs_key::OTA_PASS, DEFAULT_OTA_PASSWORD);
  });
}
bool saveOtaPassword(const String& pass) {
  return withRW("ota_pass", [&](Preferences& p) {
    return p.putString(nvs_key::OTA_PASS, pass) == pass.length();
  });
}

// -------- Watchdog bookkeeping --------
uint32_t loadRestartCount() {
  return withRO([](Preferences& p) {
    return p.getUInt(nvs_key::RESTART_COUNT, 0);
  });
}
bool incrementRestartCount() {
  return withRW("restart_cnt", [](Preferences& p) {
    const uint32_t n = p.getUInt(nvs_key::RESTART_COUNT, 0) + 1;
    return p.putUInt(nvs_key::RESTART_COUNT, n) == sizeof(uint32_t);
  });
}

uint32_t loadLastRestartEpochMs() {
  return withRO([](Preferences& p) {
    return p.getUInt(nvs_key::LAST_RESTART_MS, 0);
  });
}
bool saveLastRestartEpochMs(uint32_t ms) {
  return withRW("last_restart", [&](Preferences& p) {
    return p.putUInt(nvs_key::LAST_RESTART_MS, ms) == sizeof(uint32_t);
  });
}

// -------- Factory reset --------
bool factoryReset() {
  return withRW("factory_reset", [](Preferences& p) {
    return p.clear();
  });
}

} // namespace storage
