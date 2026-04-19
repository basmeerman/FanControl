#pragma once

// MQTT client + Home Assistant auto-discovery.
//
// Plan §F4 / §6. Publishes state periodically (MQTT_PUBLISH_INTERVAL_MS)
// and pushes alarms edge-triggered via publishAlarm(). HA discovery is
// republished on every (re)connect so HA always recovers cleanly after a
// broker restart.
//
// Connection type is chosen at begin() time based on NVS MqttConfig.port:
//   port == 8883 → WiFiClientSecure with setInsecure() (no CA check).
//                  Matches SmartEVSE-3.5 approach. Switching TLS on/off
//                  is a port change in the web UI; no reboot required on
//                  the next boot.
//                  TODO(security): pin a CA / cert fingerprint.
//   port != 8883 → plain WiFiClient.
//
// All NVS access goes through storage::* — no direct Preferences here.
// Never blocks more than ~10 ms per iteration; sleeps between attempts.

#include <Arduino.h>
#include "config.h"

namespace mqtt {

  // Load NVS config, pick the transport (TLS or plain), prepare topics.
  // Call once from setup() before spawning the task.
  void begin();

  // FreeRTOS task entry. Main owns the reconnect + publish loop. Caller
  // creates the task with xTaskCreatePinnedToCore(mqtt::task, ...).
  void task(void* arg);

  // True when the underlying PubSubClient is connected.
  bool isConnected();

  // Edge-triggered alarm push. Safe to call from other tasks; the
  // publish is attempted on the calling task when connected, otherwise
  // a flag is set and the value is picked up at the next routine cycle.
  // `name` must be one of: "alarm_temp", "alarm_sensor", "watchdog".
  void publishAlarm(const char* name, bool active);

} // namespace mqtt
