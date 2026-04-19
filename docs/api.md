# FanControl — HTTP + WebSocket API

All network-facing endpoints are served by the ESP32 on port 80 once WiFi is up
(`fancontrol.local` via mDNS, or the STA IP).

## HTTP endpoints

| Method | Path | Purpose | Response |
|---|---|---|---|
| `GET` | `/` | Single-page web UI (embedded PROGMEM) | `200 text/html` — 14.6 KB |
| `GET` | `/healthz` | Liveness probe | `200 ok` |
| `GET` | `/ws` | WebSocket upgrade endpoint | See below |
| `GET` | `/update` | ElegantOTA — HTTP basic-auth form | `200` once OTA password is non-default; `403` otherwise (see *OTA gate* below) |
| `POST` | `/update/...` | ElegantOTA upload targets | Same gate as above |
| `GET` | `/generate_204`, `/gen_204`, `/hotspot-detect.html`, `/ncsi.txt`, `/connecttest.txt`, `/library/test/success.html`, `/redirect` | Captive-portal detection probes | In AP mode: 302 redirect to `/`. In STA mode: 204 no-content (keeps phones happy). |

### OTA gate

ElegantOTA is only mounted when `storage::loadOtaPassword()` differs from the
hardcoded default (`"changeme"`). Until the user saves a new password via the
web UI, the three `/update*` routes return `403 Forbidden` with a body asking
the user to open the web UI and set a password.

`webserver::applyOtaPassword()` mounts ElegantOTA live on the first non-default
save (no reboot required). Once mounted, ElegantOTA v3's API doesn't expose a
clean rebind — subsequent password changes are applied but the cleanest state
comes from a reboot (the web UI prompts for one).

## WebSocket (`/ws`)

A single `AsyncWebSocket` endpoint. JSON frames both directions.

### Device → Browser

#### `status` — pushed every `WS_PUSH_INTERVAL_MS` (2 s)

```json
{
  "type": "status",
  "temperature": 24.3,
  "humidity": 58.2,
  "fan_speed": 45,
  "fan_pwm_freq": 1000,
  "alarm":  { "temperature": false, "sensor": false, "watchdog": false },
  "mqtt":   { "connected": true, "broker": "192.168.1.10" },
  "wifi":   { "ssid": "MyNetwork", "rssi": -62, "ip": "192.168.1.50", "portal": false },
  "system": {
    "uptime": 86400,
    "heap_free": 142320,
    "restarts": 2,
    "version": "0.1.0",
    "build_date": "Apr 19 2026 07:51:08",
    "first_boot_change_password": true
  }
}
```

- `temperature` / `humidity` may be `null` while the sensor hasn't produced a first reading.
- `wifi.portal = true` when the captive-portal AP is active.
- `first_boot_change_password = true` when the stored OTA password still equals the `"changeme"` default. The UI shows a sticky red banner in this state.

#### `saved` — ack for an inbound config frame

```json
{ "type": "saved", "section": "fan", "ok": true }
```

On validation failure the ack carries an error:

```json
{ "type": "saved", "section": "fan", "ok": false, "error": "thresholds must be strictly increasing" }
```

Sections: `fan`, `network`, `ota_password`, `restart`, `factory_reset`.

#### `log` — optional debug stream

```json
{ "type": "log", "line": "[FanControl] mqtt connected broker=..." }
```

Sent when `ws::broadcastLog()` is invoked. Not pushed routinely.

### Browser → Device

#### `set_fan`

Persist the fan curve + thresholds + PWM frequency; apply runtime.

```json
{
  "type": "set_fan",
  "thresholds":  [15, 20, 25, 30, 35],
  "pwm_levels":  [10, 25, 50, 75, 100],
  "alarm_temp":  35,
  "min_fan":     10,
  "sensor_interval_ms": 5000,
  "pwm_freq_hz": 1000
}
```

**Validation (enforced in `src/websocket.cpp`):**
- `thresholds` — 5 values, strictly monotonically increasing
- `pwm_levels` — 5 values, each in `[0, 100]`
- `alarm_temp` — `[0, 80]` °C
- `min_fan` — `[0, 100]`
- `sensor_interval_ms` — `[1000, 60000]` ms
- `pwm_freq_hz` — `[FAN_PWM_FREQ_MIN_HZ, FAN_PWM_FREQ_MAX_HZ]` = `[1000, 5000]`

On success: `fan::setMinPercent()` + `fan::setFrequency()` apply live, no reboot.

#### `set_network`

Persist WiFi + MQTT + device name. **Reboot required** for WiFi changes to take effect; the ack signals this.

```json
{
  "type": "set_network",
  "wifi": { "ssid": "MyNet", "password": "..." },
  "mqtt": {
    "host": "192.168.1.10", "port": 1883,
    "user": "ha", "password": "...",
    "prefix": "fancontrol"
  },
  "device_name": "fancontrol"
}
```

Ack: `{"type":"saved","section":"network","ok":true,"reboot_required":true}`.

**MQTT TLS:** set `port = 8883` to auto-enable TLS (`WiFiClientSecure` + `setInsecure()` — no CA verification). See [docs/mqtt_topics.md](mqtt_topics.md#tls-port-8883).

#### `set_ota_password`

```json
{ "type": "set_ota_password", "password": "<new>" }
```

Rejected if length < 8. On success, `storage::saveOtaPassword()` + `webserver::applyOtaPassword()` run live; `/update` becomes available.

#### `restart`

```json
{ "type": "restart" }
```

Calls `watchdog::requestRestart("user")`. Subject to the 5-minute cooldown if a restart already happened recently.

#### `factory_reset`

```json
{ "type": "factory_reset", "confirm": true }
```

`confirm: true` is required — frames without it are ignored. On match, `storage::factoryReset()` wipes the NVS namespace and schedules a restart. On next boot the device comes up in captive-portal mode.

## Error handling

- Malformed JSON → ignored; a `log` frame describes the parse error (when `broadcastLog` is enabled).
- Unknown `type` → ignored with a `log` frame.
- Any handler may emit a `saved` ack with `"ok": false` and an `"error"` string; the UI surfaces this as a toast.

## Rate limits

- Inbound handler: no explicit rate limit; `AsyncWebSocket` handles backpressure.
- Outbound push: one `status` every 2 s, one `cleanupClients()` every second (housekeeping).

## Stack / task model

- `webserver::begin()` creates the `AsyncWebServer` on core 0.
- `ws::task` runs on core 0 at priority 3 with a 6 KB stack — enough for ArduinoJson 7 serialisation of the status frame.
- `wifi_manager::task` runs on core 0 at priority 3 with a 4 KB stack — handles STA/AP transitions.
- All three network tasks are subscribed to the hardware TWDT via `watchdog::subscribeCurrentTask()`.
