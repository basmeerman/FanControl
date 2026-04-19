#include "websocket.h"

#include "config.h"
#include "version.h"
#include "storage.h"
#include "sensor.h"
#include "fan.h"
#include "watchdog.h"
#include "mqtt.h"
#include "wifi_manager.h"
#include "webserver.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// WebSocket at /ws.
//
// Outbound (every WS_PUSH_INTERVAL_MS = 2 s):
//   {"type":"status", temperature, humidity, fan_speed, fan_pwm_freq,
//    alarm:{temperature,sensor,watchdog}, mqtt:{connected,broker},
//    wifi:{ssid,rssi,ip,portal},
//    system:{uptime, heap_free, restarts, version, build_date,
//            first_boot_change_password}}
//
// Inbound (from browser):
//   set_fan          → save curve + alarm/min/interval/freq, apply runtime
//   set_network      → save WiFi + MQTT + device name (reboot required)
//   set_ota_password → saveOtaPassword + webserver::applyOtaPassword
//   restart          → watchdog::requestRestart("user")
//   factory_reset    → storage::factoryReset + reboot
//
// Acknowledgements are sent back as {"type":"saved","section":"…",
// "ok":true/false,"error"?, "reboot_required"?}.

namespace {

AsyncWebSocket s_ws("/ws");
constexpr size_t JSON_BUFFER_BYTES = 1024;

// Serialize the status snapshot into a String. We pull all fields on the
// ws task so we never race against MQTT/sensor tasks for mutable state
// (reads of scalars and String copies are cheap and safe enough here).
String buildStatusJson() {
  JsonDocument doc;
  doc["type"] = "status";

  const float t = sensor::getTemperatureC();
  const float h = sensor::getHumidityPct();
  if (isnan(t)) doc["temperature"] = nullptr; else doc["temperature"] = t;
  if (isnan(h)) doc["humidity"]    = nullptr; else doc["humidity"]    = h;

  doc["fan_speed"]    = fan::currentPercent();
  doc["fan_pwm_freq"] = fan::currentFrequency();

  const float alarmThresh = storage::loadAlarmTemp();
  JsonObject alarm = doc["alarm"].to<JsonObject>();
  alarm["temperature"] = !isnan(t) && (t >= alarmThresh);
  alarm["sensor"]      = sensor::isStale();
  alarm["watchdog"]    = watchdog::alarmActive();

  JsonObject mqttObj = doc["mqtt"].to<JsonObject>();
  mqttObj["connected"] = mqtt::isConnected();
  mqttObj["broker"]    = storage::loadMqtt().host;

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"]   = wifi_manager::currentSsid();
  wifi["rssi"]   = wifi_manager::rssi();
  wifi["ip"]     = wifi_manager::ipAddress();
  wifi["portal"] = wifi_manager::isPortalActive();

  JsonObject sys = doc["system"].to<JsonObject>();
  sys["uptime"]    = millis() / 1000UL;
  sys["heap_free"] = ESP.getFreeHeap();
  sys["restarts"]  = watchdog::restartCount();
  sys["version"]   = VERSION;
  sys["build_date"] = BUILD_DATE;
  sys["first_boot_change_password"] =
      (storage::loadOtaPassword() == DEFAULT_OTA_PASSWORD);

  String out;
  out.reserve(512);
  serializeJson(doc, out);
  return out;
}

// Reply helper: push a single frame to a specific client.
void sendAck(AsyncWebSocketClient* client,
             const char* section, bool ok,
             const char* error = nullptr,
             bool rebootRequired = false) {
  if (!client) return;
  JsonDocument doc;
  doc["type"]    = "saved";
  doc["section"] = section;
  doc["ok"]      = ok;
  if (error)         doc["error"]           = error;
  if (rebootRequired) doc["reboot_required"] = true;
  String out;
  serializeJson(doc, out);
  client->text(out);
}

// ---------- Dispatch handlers ----------

void handleSetFan(AsyncWebSocketClient* client, JsonDocument& doc) {
  // Thresholds + PWM levels: two arrays of FAN_CURVE_POINTS values.
  JsonArray tArr = doc["thresholds"].as<JsonArray>();
  JsonArray pArr = doc["pwm_levels"].as<JsonArray>();
  if (tArr.size() != FAN_CURVE_POINTS || pArr.size() != FAN_CURVE_POINTS) {
    sendAck(client, "fan", false, "invalid curve size");
    return;
  }

  FanCurve curve{};
  for (uint8_t i = 0; i < FAN_CURVE_POINTS; ++i) {
    curve.temps[i] = tArr[i].as<float>();
    const int pct  = pArr[i].as<int>();
    if (pct < 0 || pct > 100) {
      sendAck(client, "fan", false, "pwm out of range");
      return;
    }
    curve.pwm[i] = static_cast<uint8_t>(pct);
  }
  // Curve temperatures must be monotonic increasing.
  for (uint8_t i = 1; i < FAN_CURVE_POINTS; ++i) {
    if (!(curve.temps[i] > curve.temps[i - 1])) {
      sendAck(client, "fan", false, "thresholds must be strictly increasing");
      return;
    }
  }

  const float    alarmTemp = doc["alarm_temp"] | storage::loadAlarmTemp();
  const int      minFan    = doc["min_fan"]    | static_cast<int>(storage::loadFanMinPercent());
  const uint32_t interval  = doc["sensor_interval_ms"] | storage::loadSensorIntervalMs();
  const uint32_t freqHz    = doc["pwm_freq_hz"]        | storage::loadFanPwmFreqHz();

  if (alarmTemp < 0.0f || alarmTemp > 80.0f) {
    sendAck(client, "fan", false, "alarm_temp out of range"); return;
  }
  if (minFan < 0 || minFan > 100) {
    sendAck(client, "fan", false, "min_fan out of range"); return;
  }
  if (interval < 1000 || interval > 60000) {
    sendAck(client, "fan", false, "sensor_interval_ms out of range"); return;
  }
  if (freqHz < FAN_PWM_FREQ_MIN_HZ || freqHz > FAN_PWM_FREQ_MAX_HZ) {
    sendAck(client, "fan", false, "pwm_freq_hz out of range"); return;
  }

  bool ok = true;
  ok &= storage::saveFanCurve(curve);
  ok &= storage::saveAlarmTemp(alarmTemp);
  ok &= storage::saveFanMinPercent(static_cast<uint8_t>(minFan));
  ok &= storage::saveSensorIntervalMs(interval);
  ok &= storage::saveFanPwmFreqHz(freqHz);

  if (ok) {
    // Apply runtime changes so the user sees an immediate effect.
    fan::setMinPercent(static_cast<uint8_t>(minFan));
    fan::setFrequency(freqHz);
  }

  sendAck(client, "fan", ok, ok ? nullptr : "nvs save failed");
}

void handleSetNetwork(AsyncWebSocketClient* client, JsonDocument& doc) {
  JsonObject wifiObj = doc["wifi"].as<JsonObject>();
  JsonObject mqttObj = doc["mqtt"].as<JsonObject>();

  WifiConfig wifi;
  wifi.ssid     = wifiObj["ssid"]     | "";
  wifi.password = wifiObj["password"] | "";
  if (wifi.ssid.length() == 0 || wifi.ssid.length() > 32) {
    sendAck(client, "network", false, "wifi ssid invalid"); return;
  }
  if (wifi.password.length() > 0 && wifi.password.length() < 8) {
    sendAck(client, "network", false, "wifi password too short (min 8)"); return;
  }

  MqttConfig mqttCfg;
  mqttCfg.host     = mqttObj["host"]     | "";
  mqttCfg.port     = mqttObj["port"]     | DEFAULT_MQTT_PORT;
  mqttCfg.user     = mqttObj["user"]     | "";
  mqttCfg.password = mqttObj["password"] | "";
  mqttCfg.prefix   = mqttObj["prefix"]   | DEFAULT_MQTT_PREFIX;

  const String deviceName = doc["device_name"] | DEFAULT_DEVICE_NAME;

  bool ok = true;
  ok &= storage::saveWifi(wifi);
  ok &= storage::saveMqtt(mqttCfg);
  ok &= storage::saveDeviceName(deviceName);

  sendAck(client, "network", ok, ok ? nullptr : "nvs save failed",
          /*rebootRequired=*/true);
}

void handleSetOtaPassword(AsyncWebSocketClient* client, JsonDocument& doc) {
  const String pwd = doc["password"] | "";
  if (pwd.length() < 8) {
    sendAck(client, "ota_password", false, "password must be >= 8 chars");
    return;
  }
  const bool ok = storage::saveOtaPassword(pwd);
  if (ok) {
    webserver::applyOtaPassword(pwd);
  }
  sendAck(client, "ota_password", ok, ok ? nullptr : "nvs save failed");
}

void handleRestart(AsyncWebSocketClient* client, JsonDocument& /*doc*/) {
  sendAck(client, "restart", true);
  // Yield so the ack has a chance to flush before the reset.
  vTaskDelay(pdMS_TO_TICKS(200));
  watchdog::requestRestart("user");
}

void handleFactoryReset(AsyncWebSocketClient* client, JsonDocument& doc) {
  if (!(doc["confirm"] | false)) {
    sendAck(client, "factory_reset", false, "confirm=true required"); return;
  }
  const bool ok = storage::factoryReset();
  sendAck(client, "factory_reset", ok, ok ? nullptr : "wipe failed");
  if (ok) {
    vTaskDelay(pdMS_TO_TICKS(200));
    watchdog::requestRestart("factory_reset");
  }
}

// ---------- Event handler ----------

void onEvent(AsyncWebSocket* /*server*/,
             AsyncWebSocketClient* client,
             AwsEventType type,
             void* /*arg*/,
             uint8_t* data,
             size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      log_i("WS client #%u connected from %s",
            client->id(), client->remoteIP().toString().c_str());
      // Push an immediate status frame so the UI doesn't wait for the 2s tick.
      client->text(buildStatusJson());
      break;

    case WS_EVT_DISCONNECT:
      log_i("WS client #%u disconnected", client->id());
      break;

    case WS_EVT_DATA: {
      // Only handle single-frame text messages.
      if (len == 0 || len > JSON_BUFFER_BYTES) {
        sendAck(client, "unknown", false, "payload too large"); return;
      }
      JsonDocument doc;
      DeserializationError err = deserializeJson(
          doc, reinterpret_cast<const char*>(data), len);
      if (err) {
        log_w("WS JSON parse error: %s", err.c_str());
        sendAck(client, "unknown", false, "invalid json"); return;
      }
      const String cmd = doc["type"] | "";
      if      (cmd == "set_fan")          handleSetFan(client, doc);
      else if (cmd == "set_network")      handleSetNetwork(client, doc);
      else if (cmd == "set_ota_password") handleSetOtaPassword(client, doc);
      else if (cmd == "restart")          handleRestart(client, doc);
      else if (cmd == "factory_reset")    handleFactoryReset(client, doc);
      else {
        log_w("WS unknown type: %s", cmd.c_str());
        sendAck(client, "unknown", false, "unknown type");
      }
      break;
    }

    case WS_EVT_ERROR:
      log_w("WS error on client #%u", client->id());
      break;

    default:
      break;
  }
}

} // namespace

namespace ws {

void attach(AsyncWebServer& server) {
  s_ws.onEvent(onEvent);
  server.addHandler(&s_ws);
  log_i("WebSocket handler mounted at /ws");
}

void broadcastLog(const char* line) {
  if (!line) return;
  JsonDocument doc;
  doc["type"] = "log";
  doc["line"] = line;
  String out;
  serializeJson(doc, out);
  s_ws.textAll(out);
}

void task(void* /*arg*/) {
  watchdog::subscribeCurrentTask();
  TickType_t lastWake = xTaskGetTickCount();
  uint32_t   cleanupAccumMs = 0;

  for (;;) {
    watchdog::reset();

    // Only publish when at least one client is connected — avoids the
    // JSON cost when nobody is looking.
    if (s_ws.count() > 0) {
      const String json = buildStatusJson();
      s_ws.textAll(json);
    }

    // AsyncWebSocket housekeeping: drop stale clients roughly once per second.
    cleanupAccumMs += WS_PUSH_INTERVAL_MS;
    if (cleanupAccumMs >= 1000) {
      s_ws.cleanupClients();
      cleanupAccumMs = 0;
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(WS_PUSH_INTERVAL_MS));
  }
}

} // namespace ws
