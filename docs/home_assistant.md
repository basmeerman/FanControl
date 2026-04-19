# FanControl — Home Assistant integration

FanControl exposes its state to Home Assistant over the **ESPHome native API**
(encrypted, authenticated, fast). HA discovers the device automatically once
the ESPHome integration is installed and the device is on the same LAN.

## Setup

1. Install the [ESPHome integration](https://www.home-assistant.io/integrations/esphome/)
   in Home Assistant (usually it self-prompts when a new `*.local` ESPHome
   device appears).
2. When prompted, paste the value of `api_encryption_key` from your local
   `secrets.yaml` (same key the device was compiled with).
3. The device now appears as `fancontrol` with all entities auto-added.

## Entities

| Entity name | Type | Unit | Notes |
|---|---|---|---|
| Temperature | sensor | °C | from DHT22 |
| Humidity | sensor | % | from DHT22 |
| Fan Speed | sensor | % | derived live from the curve |
| Fan PWM Frequency | sensor | Hz | reflects the current setting |
| Restart Counter | sensor | — | `state_class: total_increasing`; persisted across reboots |
| Uptime | sensor | s | ESPHome built-in |
| WiFi Signal | sensor | dBm | updates every 60 s |
| Temperature Alarm | binary_sensor | — | `device_class: safety`, fires at configured threshold |
| Sensor Alarm | binary_sensor | — | `device_class: problem`, fires after 60 s of no reads |
| Alarm Temperature | number | °C | tunable threshold, 0–80, default 35 |
| Fan Min % | number | % | min duty under normal operation, 0–100, default 10 |
| Fan PWM Frequency Setting | number | Hz | 1000–5000, 100 step, applied live |
| Restart | button | — | graceful reboot |
| Factory Reset | button | — | wipes NVS, next boot → captive portal |

## Automation ideas

```yaml
# Push a notification when either alarm fires
alias: FanControl alarm notification
trigger:
  - platform: state
    entity_id:
      - binary_sensor.fancontrol_temperature_alarm
      - binary_sensor.fancontrol_sensor_alarm
    to: "on"
action:
  - service: notify.mobile_app
    data:
      title: "FanControl alarm"
      message: "{{ trigger.to_state.name }} is ON"
```

```yaml
# Bump the fan floor to 25 % when the battery is charging hard
alias: Battery charging → fan floor 25 %
trigger:
  - platform: numeric_state
    entity_id: sensor.victron_battery_current
    above: 30
action:
  - service: number.set_value
    target:
      entity_id: number.fancontrol_fan_min_percent
    data:
      value: 25
```

## Using MQTT instead of (or alongside) the native API

Uncomment the `mqtt:` block in `fancontrol.yaml`, fill the broker fields in
`secrets.yaml`, and rebuild. ESPHome will publish HA discovery topics under
`homeassistant/...` and state under `fancontrol/...`. Topic shape is
identical to the v0.1.x project plan's F4/§6 table.

For TLS, set `port: 8883` and add:

```yaml
mqtt:
  broker: your.broker
  port: 8883
  ssl_fingerprints:
    - "<SHA1 fingerprint of the broker cert>"
```

This is stricter than the v0.1.x `setInsecure()` approach — the fingerprint
pin is enforced by the library.
