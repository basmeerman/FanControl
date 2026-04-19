#include "mqtt.h"

#include "config.h"
#include "version.h"
#include "storage.h"
#include "sensor.h"
#include "fan.h"
#include "watchdog.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT module. See mqtt.h for the public contract.
//
// Threading: all reads of other modules (sensor::, fan::, watchdog::)
// happen on the mqtt task. publishAlarm() may be called from any task;
// it only flips a volatile flag when the client isn't connected, and
// otherwise does a single publish — PubSubClient is not thread-safe so
// we must guard publish calls with a mutex.

namespace {

// ---------- Sizing / constants ----------

constexpr uint16_t KEEPALIVE_SEC    = 30;
constexpr uint16_t SOCKET_TIMEOUT_S = 5;       // seconds
constexpr uint16_t BUFFER_SIZE      = 1024;    // discovery payloads fit in ~600 B
constexpr uint32_t TASK_TICK_MS     = 50;      // base loop cadence
constexpr uint32_t TLS_PORT         = 8883;

// ---------- State ----------

WiFiClient*          s_plainClient = nullptr;
WiFiClientSecure*    s_tlsClient   = nullptr;
PubSubClient*        s_client      = nullptr;
SemaphoreHandle_t    s_mutex       = nullptr;

MqttConfig           s_cfg;
String               s_nodeId;       // e.g. "fancontrol_a1b2c3"
String               s_deviceId;     // e.g. "fancontrol-a1b2c3" (used in device identifiers)
String               s_deviceName;   // user-visible name
String               s_prefix;       // copy of s_cfg.prefix for quick access
String               s_statusTopic;  // "{prefix}/status"

volatile bool        s_alarmTemp    = false;
volatile bool        s_alarmSensor  = false;
volatile bool        s_alarmWdt     = false;

// Edge flags set by publishAlarm() when the client isn't connected.
volatile bool        s_pendingTemp   = false;
volatile bool        s_pendingSensor = false;
volatile bool        s_pendingWdt    = false;

// ---------- Helpers ----------

String macSuffix6() {
  // Lowercase last 6 hex of base MAC, no separators.
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  char buf[7];
  snprintf(buf, sizeof(buf), "%02x%02x%02x", mac[3], mac[4], mac[5]);
  return String(buf);
}

String stateTopic(const char* kind, const char* entity) {
  // kind = "sensor" or "binary_sensor"
  String t = s_prefix;
  t += '/';
  t += kind;
  t += '/';
  t += entity;
  t += "/state";
  return t;
}

String discoveryTopic(const char* kind, const char* entityShort) {
  // homeassistant/{kind}/{node_id}_{entity_short}/config
  String t = "homeassistant/";
  t += kind;
  t += '/';
  t += s_nodeId;
  t += '_';
  t += entityShort;
  t += "/config";
  return t;
}

// Serialise a JsonDocument to a buffer and publish with retain=true.
// Caller must hold s_mutex.
bool publishRetainedJson_locked(const String& topic, const JsonDocument& doc) {
  char buf[BUFFER_SIZE];
  const size_t n = serializeJson(doc, buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf)) {
    log_e("Discovery payload serialize failed (topic=%s size=%u)",
          topic.c_str(), (unsigned)n);
    return false;
  }
  const bool ok = s_client->publish(topic.c_str(),
                                    reinterpret_cast<const uint8_t*>(buf),
                                    n,
                                    true /* retained */);
  if (!ok) {
    log_w("MQTT publish failed: %s (%u bytes)", topic.c_str(), (unsigned)n);
  }
  return ok;
}

// Shared `device` + `availability` block + unique_id. Adds English name.
void fillCommonDiscovery(JsonDocument& doc,
                         const char* entityShort,
                         const char* label) {
  JsonObject dev = doc["device"].to<JsonObject>();
  JsonArray  ids = dev["identifiers"].to<JsonArray>();
  ids.add(s_deviceId);
  dev["name"]         = s_deviceName;
  dev["manufacturer"] = "FanControl";
  dev["model"]        = "ESP32 LOLIN D32";
  dev["sw_version"]   = VERSION;

  JsonArray av = doc["availability"].to<JsonArray>();
  JsonObject av0 = av.add<JsonObject>();
  av0["topic"] = s_statusTopic;

  // unique_id = node_id + "_" + entityShort, e.g. fancontrol_a1b2c3_temperature
  String uid = s_nodeId;
  uid += '_';
  uid += entityShort;
  doc["unique_id"] = uid;
  doc["name"]      = label;
}

// Publish all HA discovery entities. Caller must hold s_mutex.
void publishDiscovery_locked() {
  // ----- Temperature -----
  {
    JsonDocument d;
    fillCommonDiscovery(d, "temperature", "Temperature");
    d["state_topic"]         = stateTopic("sensor", "temperature");
    d["unit_of_measurement"] = "\xC2\xB0" "C";  // °C as UTF-8
    d["device_class"]        = "temperature";
    d["state_class"]         = "measurement";
    publishRetainedJson_locked(discoveryTopic("sensor", "temperature"), d);
  }

  // ----- Humidity -----
  {
    JsonDocument d;
    fillCommonDiscovery(d, "humidity", "Humidity");
    d["state_topic"]         = stateTopic("sensor", "humidity");
    d["unit_of_measurement"] = "%";
    d["device_class"]        = "humidity";
    d["state_class"]         = "measurement";
    publishRetainedJson_locked(discoveryTopic("sensor", "humidity"), d);
  }

  // ----- Fan speed -----
  {
    JsonDocument d;
    fillCommonDiscovery(d, "fan_speed", "Fan Speed");
    d["state_topic"]         = stateTopic("sensor", "fan_speed");
    d["unit_of_measurement"] = "%";
    d["state_class"]         = "measurement";
    // no device_class — HA has no "fan speed" class
    publishRetainedJson_locked(discoveryTopic("sensor", "fan_speed"), d);
  }

  // ----- Fan PWM frequency (v1.2) -----
  {
    JsonDocument d;
    fillCommonDiscovery(d, "fan_pwm_freq", "Fan PWM Frequency");
    d["state_topic"]         = stateTopic("sensor", "fan_pwm_freq");
    d["unit_of_measurement"] = "Hz";
    d["state_class"]         = "measurement";
    publishRetainedJson_locked(discoveryTopic("sensor", "fan_pwm_freq"), d);
  }

  // ----- Restarts -----
  {
    JsonDocument d;
    fillCommonDiscovery(d, "restarts", "Restarts");
    d["state_topic"]  = stateTopic("sensor", "restarts");
    d["state_class"]  = "total_increasing";
    publishRetainedJson_locked(discoveryTopic("sensor", "restarts"), d);
  }

  // ----- Binary: alarm_temp -----
  {
    JsonDocument d;
    fillCommonDiscovery(d, "alarm_temp", "Temperature Alarm");
    d["state_topic"]  = stateTopic("binary_sensor", "alarm_temp");
    d["payload_on"]   = "ON";
    d["payload_off"]  = "OFF";
    d["device_class"] = "safety";
    publishRetainedJson_locked(discoveryTopic("binary_sensor", "alarm_temp"), d);
  }

  // ----- Binary: alarm_sensor -----
  {
    JsonDocument d;
    fillCommonDiscovery(d, "alarm_sensor", "Sensor Alarm");
    d["state_topic"]  = stateTopic("binary_sensor", "alarm_sensor");
    d["payload_on"]   = "ON";
    d["payload_off"]  = "OFF";
    d["device_class"] = "safety";
    publishRetainedJson_locked(discoveryTopic("binary_sensor", "alarm_sensor"), d);
  }

  // ----- Binary: watchdog -----
  {
    JsonDocument d;
    fillCommonDiscovery(d, "watchdog", "Watchdog");
    d["state_topic"]  = stateTopic("binary_sensor", "watchdog");
    d["payload_on"]   = "ON";
    d["payload_off"]  = "OFF";
    d["device_class"] = "problem";
    publishRetainedJson_locked(discoveryTopic("binary_sensor", "watchdog"), d);
  }

  log_i("MQTT discovery published for node_id=%s", s_nodeId.c_str());
}

// Publish birth message "online" retained. Caller must hold s_mutex.
void publishBirth_locked() {
  s_client->publish(s_statusTopic.c_str(), "online", true /* retained */);
}

// Snapshot current state + publish all routine topics. Caller must hold s_mutex.
void publishState_locked() {
  char buf[16];

  // Temperature / humidity — may be NAN pre-first-read; skip those.
  const float t = sensor::getTemperatureC();
  if (!isnan(t)) {
    snprintf(buf, sizeof(buf), "%.1f", t);
    s_client->publish(stateTopic("sensor", "temperature").c_str(), buf, false);
  }
  const float h = sensor::getHumidityPct();
  if (!isnan(h)) {
    snprintf(buf, sizeof(buf), "%.1f", h);
    s_client->publish(stateTopic("sensor", "humidity").c_str(), buf, false);
  }

  snprintf(buf, sizeof(buf), "%u", (unsigned)fan::currentPercent());
  s_client->publish(stateTopic("sensor", "fan_speed").c_str(), buf, false);

  snprintf(buf, sizeof(buf), "%u", (unsigned)fan::currentFrequency());
  s_client->publish(stateTopic("sensor", "fan_pwm_freq").c_str(), buf, false);

  snprintf(buf, sizeof(buf), "%u", (unsigned)watchdog::restartCount());
  s_client->publish(stateTopic("sensor", "restarts").c_str(), buf, false);

  // Alarms — republish routinely so HA recovers cleanly from broker restart.
  s_client->publish(stateTopic("binary_sensor", "alarm_temp").c_str(),
                    s_alarmTemp ? "ON" : "OFF", false);
  s_client->publish(stateTopic("binary_sensor", "alarm_sensor").c_str(),
                    s_alarmSensor ? "ON" : "OFF", false);
  s_client->publish(stateTopic("binary_sensor", "watchdog").c_str(),
                    s_alarmWdt ? "ON" : "OFF", false);
}

// Pull current alarm state from dependent modules into our cache. Must
// be called from the mqtt task (reads sensor::isStale() and friends).
void refreshAlarms() {
  const float alarmThresh = storage::loadAlarmTemp();
  const float t           = sensor::getTemperatureC();
  s_alarmTemp   = !isnan(t) && (t >= alarmThresh);
  s_alarmSensor = sensor::isStale();
  s_alarmWdt    = watchdog::alarmActive();
}

// Flush any edge-triggered alarm that came in while we were disconnected.
// Caller must hold s_mutex.
void flushPendingAlarms_locked() {
  if (s_pendingTemp) {
    s_client->publish(stateTopic("binary_sensor", "alarm_temp").c_str(),
                      s_alarmTemp ? "ON" : "OFF", false);
    s_pendingTemp = false;
  }
  if (s_pendingSensor) {
    s_client->publish(stateTopic("binary_sensor", "alarm_sensor").c_str(),
                      s_alarmSensor ? "ON" : "OFF", false);
    s_pendingSensor = false;
  }
  if (s_pendingWdt) {
    s_client->publish(stateTopic("binary_sensor", "watchdog").c_str(),
                      s_alarmWdt ? "ON" : "OFF", false);
    s_pendingWdt = false;
  }
}

// Attempt a connection with LWT preset. Returns true on success.
// Caller must hold s_mutex.
bool tryConnect_locked() {
  if (s_cfg.host.isEmpty()) {
    return false;
  }

  // Client ID includes node suffix to avoid broker collisions.
  String clientId = "fancontrol-";
  clientId += macSuffix6();

  // setServer is called here because the NVS config could be updated by
  // the web UI later (Round 4). Safe to call every attempt.
  s_client->setServer(s_cfg.host.c_str(), s_cfg.port);

  const bool hasAuth = !s_cfg.user.isEmpty();
  bool ok = false;

  // LWT: topic, qos(int), retain(bool), payload — set on .connect() call.
  // PubSubClient signature: connect(id, user, pass, willTopic, willQos,
  //                                  willRetain, willMessage)
  if (hasAuth) {
    ok = s_client->connect(clientId.c_str(),
                           s_cfg.user.c_str(),
                           s_cfg.password.c_str(),
                           s_statusTopic.c_str(),
                           1 /* willQos */,
                           true /* willRetain */,
                           "offline");
  } else {
    ok = s_client->connect(clientId.c_str(),
                           nullptr, nullptr,
                           s_statusTopic.c_str(),
                           1, true, "offline");
  }
  return ok;
}

} // namespace

namespace mqtt {

void begin() {
  s_mutex = xSemaphoreCreateMutex();

  s_cfg        = storage::loadMqtt();
  s_deviceName = storage::loadDeviceName();
  if (s_deviceName.isEmpty()) {
    s_deviceName = DEFAULT_DEVICE_NAME;
  }

  s_prefix = s_cfg.prefix.isEmpty() ? String(DEFAULT_MQTT_PREFIX) : s_cfg.prefix;

  const String mac6 = macSuffix6();
  s_nodeId   = String(DEFAULT_DEVICE_NAME) + "_" + mac6;   // underscore only
  s_deviceId = String(DEFAULT_DEVICE_NAME) + "-" + mac6;   // dash in device identifier is fine

  s_statusTopic = s_prefix + "/status";

  // Pick transport based on port. TLS decision is fixed for this boot;
  // flipping the port via the web UI takes effect on the next reboot.
  if (s_cfg.port == TLS_PORT) {
    s_tlsClient = new WiFiClientSecure();
    s_tlsClient->setInsecure();   // matches SmartEVSE-3.5 — no CA check.
    s_client = new PubSubClient(*s_tlsClient);
    log_i("MQTT transport: TLS (port %u, setInsecure)", (unsigned)s_cfg.port);
  } else {
    s_plainClient = new WiFiClient();
    s_client = new PubSubClient(*s_plainClient);
    log_i("MQTT transport: plain TCP (port %u)", (unsigned)s_cfg.port);
  }

  s_client->setBufferSize(BUFFER_SIZE);
  s_client->setKeepAlive(KEEPALIVE_SEC);
  s_client->setSocketTimeout(SOCKET_TIMEOUT_S);

  log_i("MQTT begin: host=%s prefix=%s node_id=%s",
        s_cfg.host.c_str(), s_prefix.c_str(), s_nodeId.c_str());
}

bool isConnected() {
  return s_client && s_client->connected();
}

void publishAlarm(const char* name, bool active) {
  if (!name) return;

  // Update cached state first so flushPendingAlarms_locked() sees the
  // correct value once we reconnect.
  if      (strcmp(name, "alarm_temp")   == 0) s_alarmTemp   = active;
  else if (strcmp(name, "alarm_sensor") == 0) s_alarmSensor = active;
  else if (strcmp(name, "watchdog")     == 0) s_alarmWdt    = active;
  else {
    log_w("publishAlarm: unknown name '%s'", name);
    return;
  }

  if (!s_client || !s_mutex) {
    // begin() hasn't run — defer entirely.
    return;
  }

  // Resolve the topic as a stable String in this stack frame.
  String topic;
  if      (strcmp(name, "alarm_temp")   == 0) topic = stateTopic("binary_sensor", "alarm_temp");
  else if (strcmp(name, "alarm_sensor") == 0) topic = stateTopic("binary_sensor", "alarm_sensor");
  else                                        topic = stateTopic("binary_sensor", "watchdog");

  // Try to push immediately, but never block long.
  if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (s_client->connected()) {
      s_client->publish(topic.c_str(), active ? "ON" : "OFF", false);
    } else {
      if      (strcmp(name, "alarm_temp")   == 0) s_pendingTemp   = true;
      else if (strcmp(name, "alarm_sensor") == 0) s_pendingSensor = true;
      else                                        s_pendingWdt    = true;
    }
    xSemaphoreGive(s_mutex);
  } else {
    // Couldn't grab the mutex quickly; defer until routine cycle.
    if      (strcmp(name, "alarm_temp")   == 0) s_pendingTemp   = true;
    else if (strcmp(name, "alarm_sensor") == 0) s_pendingSensor = true;
    else                                        s_pendingWdt    = true;
  }
}

void task(void* /*arg*/) {
  watchdog::subscribeCurrentTask();

  uint32_t backoffMs     = MQTT_RECONNECT_MIN_MS;
  uint32_t nextRetryMs   = 0;
  uint32_t nextPublishMs = 0;
  bool     wifiLoggedOnce = false;

  for (;;) {
    watchdog::reset();
    const uint32_t now = millis();

    // 1. Guard: WiFi must be up.
    if (WiFi.status() != WL_CONNECTED) {
      if (!wifiLoggedOnce) {
        log_w("MQTT paused: WiFi not connected");
        wifiLoggedOnce = true;
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (wifiLoggedOnce) {
      log_i("WiFi is up — MQTT will attempt to connect");
      wifiLoggedOnce = false;
    }

    // 2. Reconnect loop with exponential backoff.
    if (!s_client->connected()) {
      if (now < nextRetryMs) {
        vTaskDelay(pdMS_TO_TICKS(TASK_TICK_MS));
        continue;
      }

      bool connected = false;
      if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        connected = tryConnect_locked();
        if (connected) {
          publishBirth_locked();
          publishDiscovery_locked();
          refreshAlarms();
          flushPendingAlarms_locked();
          publishState_locked();
          nextPublishMs = now + MQTT_PUBLISH_INTERVAL_MS;
        }
        xSemaphoreGive(s_mutex);
      }

      if (connected) {
        log_i("MQTT connected to %s:%u", s_cfg.host.c_str(), (unsigned)s_cfg.port);
        backoffMs = MQTT_RECONNECT_MIN_MS;
      } else {
        const int rc = s_client ? s_client->state() : -99;
        log_w("MQTT connect failed (rc=%d) — retrying in %u ms", rc, (unsigned)backoffMs);
        nextRetryMs = millis() + backoffMs;
        // Exponential backoff, capped.
        uint32_t next = backoffMs * 2;
        if (next > MQTT_RECONNECT_MAX_MS) next = MQTT_RECONNECT_MAX_MS;
        backoffMs = next;
      }

      vTaskDelay(pdMS_TO_TICKS(TASK_TICK_MS));
      continue;
    }

    // 3. Connected: service the client and publish on cadence.
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      s_client->loop();
      // Pick up any deferred alarm edges.
      if (s_pendingTemp || s_pendingSensor || s_pendingWdt) {
        flushPendingAlarms_locked();
      }
      if ((int32_t)(now - nextPublishMs) >= 0) {
        refreshAlarms();
        publishState_locked();
        nextPublishMs = now + MQTT_PUBLISH_INTERVAL_MS;
      }
      xSemaphoreGive(s_mutex);
    }

    vTaskDelay(pdMS_TO_TICKS(TASK_TICK_MS));
  }
}

} // namespace mqtt
