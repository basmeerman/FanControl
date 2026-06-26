# FanControl

Generic Failsafe PWM Ventilation controller based on temperature. 

It drives any PWM steered ventilator, for me specifically it drives a Ruck EM 125L EC 02 fan via PWM based LOLIN D32 (ESP32) on DHT22 temperature, integrates with Home
Assistant over MQTT and exposes a built-in web dashboard on `fancontrol.local`.

- **Hardware:** LOLIN D32 (ESP32) + DHT22 + Ruck EM 125L EC 02 (PWM via level-shifter)
- **PWM:** default 1 kHz, runtime-tunable 1–5 kHz (per Ruck datasheet)
- **Framework:** [ESPHome](https://esphome.io) — YAML-driven firmware, compiled on demand
- **Firmware size:** ~945 KB (under the 1.5 MB CI gate)

## Quick start

Prerequisites: Python 3.10+, [`pipx`](https://pipx.pypa.io), and a micro-USB cable.

```bash
# 1. Install ESPHome
pipx install esphome

# 2. Configure secrets
cp secrets.yaml.example secrets.yaml
# Edit secrets.yaml — fill in WiFi creds and pick strong passwords
# (generate an API encryption key with: openssl rand -base64 32)

# 3. Plug the LOLIN D32 into USB, then flash the first time over USB:
esphome run fancontrol.yaml

# 4. All subsequent flashes happen over WiFi (OTA, password-gated):
esphome run fancontrol.yaml
```

If WiFi credentials are wrong or missing, the device falls back to an open AP
named **`FanControl-Setup`** at `192.168.4.1` — connect, configure, save.

## Home Assistant integration

Once on the network, HA auto-discovers the device via the ESPHome integration.
No manual MQTT configuration needed. Exposed entities:

| Entity | Type | Notes |
|---|---|---|
| Temperature | sensor | °C, from DHT22 |
| Humidity | sensor | %, from DHT22 |
| Fan Speed | sensor | % duty, derived from the curve |
| Fan PWM Frequency | sensor | Hz |
| Restart Counter | sensor | monotonic, persisted in NVS |
| Uptime | sensor | seconds |
| WiFi Signal | sensor | dBm |
| Temperature Alarm | binary_sensor | `safety` class, fires at the configured threshold |
| Sensor Alarm | binary_sensor | `problem` class, fires after 60 s of no successful reads |
| Alarm Temperature | number | tunable threshold |
| Fan Min % | number | minimum duty under normal operation |
| Fan PWM Frequency Setting | number | 1000–5000 Hz, applied live via `ledc.set_frequency` |
| Restart / Factory Reset | button | standard ESPHome buttons |

## Safety behaviour

- **Sensor stall** (no successful DHT22 read for 60 s) → fan forced to 100 %.
- **NaN temperature reading** → fan biased to 100 % (curve treats unknown as hot).
- **Fan-curve interpolation**: linear between the 5 configured (°C, %) points; below the first point → use its PWM with the min-% floor; above the last point → 100 %.
- **Hardware watchdog**: managed by ESPHome core; restart counter persisted across reboots.

## Web dashboard

ESPHome's built-in dashboard is served on port 80 at `http://fancontrol.local` — live
sensor values, controls for the number entities, restart/factory-reset buttons.

## Documentation

- [`fancontrol.yaml`](fancontrol.yaml) — single source of truth for firmware behaviour
- [`PROJECT_PLAN.md`](PROJECT_PLAN.md) — project goals, history, and the rationale for the
  ESPHome switch (pre-v0.2.0 custom-C++ architecture documented under "History")
- [`docs/wiring_diagram.md`](docs/wiring_diagram.md) — BOM, pin map, PWM verification
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — dev workflow, branching, release cut
- [`SECURITY.md`](SECURITY.md) — OTA/API security model, pre-v0.2.0 legacy RSA-signed
  releases still verifiable

## License

MIT
