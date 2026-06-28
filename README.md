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
# Edit secrets.yaml — fill in WiFi creds, the OTA password, and the
# MQTT broker host/user/password (mqtt_host is a bare IP — no port)

# 3. Plug the LOLIN D32 into USB, then flash the first time over USB:
esphome run fancontrol.yaml

# 4. All subsequent flashes happen over WiFi (OTA, password-gated):
esphome run fancontrol.yaml
```

If WiFi credentials are wrong or missing, the device falls back to an open AP
named **`FanControl-Setup`** at `192.168.4.1` — connect, configure, save.

## Updating the device

After the first USB flash the device is on WiFi, so every later update goes
**over the air** — no cable needed:

```bash
# Build and push the new firmware over WiFi (password-gated OTA)
esphome compile fancontrol.yaml
esphome run fancontrol.yaml --device fancontrol.local

# Confirm it rebooted and reconnected
esphome logs fancontrol.yaml --device fancontrol.local
```

The device authenticates the upload with `ota_password`, reboots into the new
build, and bumps its restart counter. You can also upload a locally-built
`.ota.bin` through the web dashboard at `http://fancontrol.local`.

**If OTA fails** (or the device can't join WiFi / is in safe mode), reflash over
USB:

```bash
esphome run fancontrol.yaml --device /dev/cu.wchusbserial10   # adjust the serial port
```

> ⚠️ **Don't flash the binaries attached to a GitHub Release.** They are built
> in CI with placeholder secrets and will not connect to your WiFi. Always
> build locally against your own `secrets.yaml`. GitHub Release artifacts exist
> for reference and reproducibility only.

## Home Assistant integration

The device publishes to an MQTT broker (configured via `mqtt_host` / `mqtt_user`
/ `mqtt_password` in `secrets.yaml`). With MQTT auto-discovery enabled, Home
Assistant picks up the entities automatically once the device connects to the
broker — no manual entity setup needed. Exposed entities:

| Entity | Type | Notes |
|---|---|---|
| Temperature | sensor | °C, from DHT22 |
| Humidity | sensor | %, from DHT22 |
| Fan Speed | sensor | % duty actually applied (curve in Auto, manual setting in Manual) |
| Fan PWM Frequency | sensor | Hz |
| Restart Counter | sensor | monotonic, persisted in NVS |
| Uptime | sensor | seconds |
| WiFi Signal | sensor | dBm |
| Temperature Alarm | binary_sensor | `safety` class, fires at the configured threshold |
| Sensor Alarm | binary_sensor | `problem` class, fires after 60 s of no successful reads |
| Operating Mode | select | `Auto` (curve) or `Manual`, persisted |
| Manual Fan | switch | on/off — Manual mode only |
| Manual Fan Speed | number | 0–100 % — Manual mode only |
| Alarm Temperature | number | tunable threshold |
| Fan Min % | number | minimum duty under normal operation (Auto) |
| Fan PWM Frequency Setting | number | 1000–5000 Hz, applied live via `ledc.set_frequency` |
| Restart / Factory Reset | button | standard ESPHome buttons |

## Operating modes

- **Auto** (default) — fan speed follows the temperature curve, clamped to `Fan Min %`.
- **Manual** — `Manual Fan` toggles the fan and `Manual Fan Speed` sets the duty
  directly (off forces 0 %).

Safety failsafes apply in **both** modes: a NaN reading or a >60 s sensor stall
forces the fan to 100 %, even in Manual.

> The built-in ESPHome web page can't hide entities by mode, so the manual
> controls are always visible (they're simply inert in Auto). For per-mode UI,
> use a Home Assistant dashboard with conditional cards keyed on `Operating Mode`.

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
