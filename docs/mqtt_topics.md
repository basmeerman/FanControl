# FanControl — MQTT Topics

All topics live under a configurable prefix (default `fancontrol`, set in the web UI).
Home Assistant discovery topics are published under the fixed `homeassistant/` prefix.

## Connection

| Setting | Default | Notes |
|---|---|---|
| Broker host | *(empty)* | Set via the web UI → Network & MQTT |
| Broker port | `1883` | `8883` auto-enables TLS (`WiFiClientSecure` + `setInsecure()`; matches SmartEVSE-3.5) |
| Username / password | *(empty)* | Optional |
| Prefix | `fancontrol` | Any ASCII slug; no leading slash |
| Keep-alive | 60 s | Hardcoded in `mqtt.cpp` |
| QoS | 0 | Sufficient for telemetry; retained where noted |

## State topics (device → broker)

| Topic | Payload | Retain | Cadence |
|---|---|---|---|
| `{prefix}/status` | `online` / `offline` | ✓ | Birth on (re)connect; LWT publishes `offline` on disconnect |
| `{prefix}/sensor/temperature/state` | Float string, e.g. `24.3` | | Every `MQTT_PUBLISH_INTERVAL_MS` (15 s) |
| `{prefix}/sensor/humidity/state` | Float string, e.g. `58.2` | | 15 s |
| `{prefix}/sensor/fan_speed/state` | Integer 0–100 | | 15 s |
| `{prefix}/sensor/fan_pwm_freq/state` | Integer Hz, 1000–5000 | | 15 s |
| `{prefix}/sensor/restarts/state` | Integer (monotonic) | | 15 s |
| `{prefix}/binary_sensor/alarm_temp/state` | `ON` / `OFF` | | Edge-triggered + every 15 s |
| `{prefix}/binary_sensor/alarm_sensor/state` | `ON` / `OFF` | | Edge-triggered + every 15 s |
| `{prefix}/binary_sensor/watchdog/state` | `ON` / `OFF` | | Edge-triggered + every 15 s |

**Alarm semantics:**
- `alarm_temp` — `temperature >= alarm_temp_threshold` (set in web UI, default 35 °C)
- `alarm_sensor` — `sensor::isStale()` (no successful DHT22 read in `SENSOR_STALL_TIMEOUT_MS` = 60 s)
- `watchdog` — sensor-stall failsafe engaged (fan forced to 100 %)

Alarms are pushed edge-triggered (immediately when the flag flips) **and** republished at every routine cycle so HA recovers cleanly after a broker restart.

## HA auto-discovery (device → broker)

Published once on every (re)connect, retained, on topic `homeassistant/{component}/{node_id}_{entity}/config`.

- `node_id = "fancontrol_<last-6-hex-of-MAC>"` (underscores; HA spec forbids dashes in the topic slug)
- All child entities share one `device` block so HA groups them under **one device**:

```jsonc
{
  "device": {
    "identifiers": ["fancontrol-<last-6-hex-of-MAC>"],
    "name": "<storage::loadDeviceName()>",
    "manufacturer": "FanControl",
    "model": "ESP32 LOLIN D32",
    "sw_version": "<VERSION>"
  },
  "availability": [{ "topic": "{prefix}/status" }],
  "unique_id": "<node_id>_<entity>",
  "name": "<English label>",
  "state_topic": "{prefix}/sensor/temperature/state",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "state_class": "measurement"
}
```

**Entities published:**

| Entity | HA component | Device class | Unit | State class |
|---|---|---|---|---|
| Temperature | `sensor` | `temperature` | `°C` | `measurement` |
| Humidity | `sensor` | `humidity` | `%` | `measurement` |
| Fan speed | `sensor` | — | `%` | `measurement` |
| Fan PWM frequency | `sensor` | — | `Hz` | `measurement` |
| Restarts | `sensor` | — | — | `total_increasing` |
| Temperature alarm | `binary_sensor` | `safety` | — | — |
| Sensor alarm | `binary_sensor` | `safety` | — | — |
| Watchdog | `binary_sensor` | `problem` | — | — |

## Reconnect behaviour

- Exponential backoff: `MQTT_RECONNECT_MIN_MS` (1 s) → double → `MQTT_RECONNECT_MAX_MS` (60 s). Reset on success.
- Never blocks the main task longer than ~10 ms at a time — `vTaskDelay` between attempts.
- WiFi required — `mqtt::task` skips connect attempts while `WiFi.status() != WL_CONNECTED`.

## TLS (port 8883)

- Chosen at `mqtt::begin()` time based on NVS `MqttConfig.port`. Switching TLS on/off is a port change in the web UI.
- Uses `WiFiClientSecure::setInsecure()` — **no CA certificate verification** yet. Matches SmartEVSE-3.5; adequate for a trusted LAN broker but not for internet-exposed brokers.
- TODO (security): pin a CA certificate or server fingerprint. Tracked by the `TODO(security)` comments in `src/mqtt.cpp`.

## Thread safety

PubSubClient is not thread-safe. All `client.publish / loop / connect` calls are guarded by a FreeRTOS mutex inside `src/mqtt.cpp`. Edge-triggered `mqtt::publishAlarm()` is safe to call from any task; if the mutex can't be grabbed within 10 ms the alarm is deferred to the next routine cycle (pending-flag pattern).
