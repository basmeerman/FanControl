# FanControl — Project Plan v2.0

> **Status:** Active, ESPHome architecture (v0.2.0+)
> **Hardware:** LOLIN D32 ESP32 · DHT22 · Ruck EM 125L EC 02 (PWM)
> **Framework:** [ESPHome](https://esphome.io)

---

## 1. Overview

FanControl is a small, safety-conscious ventilation controller for a home
battery room (Victron Multiplus / JK BMS / 6× 48 V LFP). It reads a DHT22,
drives a Ruck EM 125L EC 02 EC fan via PWM against a configurable
temperature curve, and integrates with Home Assistant over the ESPHome
native API.

Everything runtime-visible lives in a single file: [`fancontrol.yaml`](fancontrol.yaml).

## 2. Features

### F1 — Ventilation core
- F1.1 PWM output on GPIO 25. Default 1 kHz (Ruck EM 125L EC 02 datasheet:
  PWM 5–10 V, 1–5 kHz), runtime-tunable via HA / web UI `number` component
  (clamped 1000–5000 Hz, 100 Hz step).
- F1.2 Temperature-driven fan speed using a 5-point piecewise-linear curve
  (°C → % duty), implemented as a lambda inside the `Fan Speed`
  `sensor.template`.
- F1.3 Humidity monitoring via the DHT22.
- F1.4 Thresholds, curve defaults, alarm temp, and min-fan-% are all
  `number.template` components with `restore_value: true` — persisted
  across reboots in NVS.
- F1.5 Minimum basis ventilation (default 10 %) enforced by the fan-curve
  lambda; never applied during failsafe.
- F1.6 **Failsafe: fan forced to 100 %** on sensor stall (no successful
  read in 60 s) or NaN readings.

### F2 — Safety & watchdog
- F2.1 ESP32 hardware TWDT — managed by ESPHome framework.
- F2.2 Sensor-stall detection: `Sensor Alarm` binary sensor flips to `on`
  after 60 s of no successful reads. The fan-curve lambda reads this and
  biases to 100 %.
- F2.3 Boot-counter persistence: `restart_counter` global with
  `restore_value: true`, incremented in `on_boot`. Exposed as a sensor.
- F2.4 Alarm temperature threshold (default 35 °C).
- F2.5 Alarms surfaced in HA as `binary_sensor` entities with
  `device_class: safety` / `problem`.

### F3 — User interface
- F3.1 ESPHome's built-in `web_server` on port 80 — live dashboard with all
  entities and controls. `fancontrol.local` via mDNS.
- F3.2 Home Assistant integration: native `api:` with encryption key. Zero
  manual configuration on the HA side beyond accepting the auto-discovered
  device.
- F3.3 Captive portal (`captive_portal:`) on first boot / on WiFi-config
  loss — SSID `FanControl-Setup`, open AP, 192.168.4.1.

### F4 — MQTT (optional)
An `mqtt:` block is commented out in `fancontrol.yaml`. Users who want the
v0.1.x topic layout can uncomment it, fill in `secrets.yaml`, and ESPHome
publishes state with HA auto-discovery. Most deployments will prefer the
native API and leave MQTT off.

### F5 — OTA updates
- `ota:` platform esphome, password-gated (`secrets.yaml` → `ota_password`).
- SHA-256 handshake; no RSA signing (see *History* below and `SECURITY.md`).

### F6 — Persistence
`globals` with `restore_value: true` for the boot counter. All `number`
components with `restore_value: true` for user-tunable settings. No manual
NVS wrapping needed.

## 3. Architecture

One file, one responsibility:

- [`fancontrol.yaml`](fancontrol.yaml) — firmware definition
- [`secrets.yaml.example`](secrets.yaml.example) — template for local
  credentials (real `secrets.yaml` is gitignored)

Custom C++ is reduced to one lambda (the fan-curve interpolation) plus a
handful of one-liners. Everything else is declarative.

## 4. Build & release

- CI (`.github/workflows/ci.yml`) on every push/PR runs
  `esphome config` + full compile (via `esphome/build-action`) + 1.5 MB
  size gate + artifact upload.
- Release (`.github/workflows/release.yml`) triggered by `v*.*.*` tag
  pushes. Compiles a release build, publishes `firmware.bin`,
  `firmware.ota.bin`, `firmware.factory.bin`, and their SHA-256 checksums.
  Release body pulled from the matching `CHANGELOG.md` section via `awk`.

## 5. Quality gates

| Gate | Check |
|---|---|
| Q1 — Config valid | `esphome config fancontrol.yaml` passes |
| Q2 — Compiles | `esphome compile fancontrol.yaml` succeeds |
| Q3 — Size | firmware.bin ≤ 1.5 MB (CI hard-gate) |
| Q4 — Safe defaults | fan failsafe = 100 %, min-% = 10, alarm = 35 °C |
| Q5 — Hardware test matrix | T01–T12 below (manual, on rig) |

## 6. Test matrix (hardware)

| # | Scenario | Expected |
|---|---|---|
| T01 | DHT22 unplugged during operation | `Sensor Alarm` ON within 60 s, fan → 100 % |
| T02 | WiFi AP unavailable | Captive portal SSID comes up on reconnect window |
| T03 | HA unavailable | Fan keeps running on the local curve |
| T04 | Long-duration (72 h soak) | No watchdog resets, restart counter unchanged |
| T05 | OTA via ESPHome | New firmware active after reboot, restart counter +1 |
| T06 | Factory reset button | NVS wiped, captive portal on next boot |
| T07 | Temperature above alarm threshold | `Temperature Alarm` ON, HA notification fires |
| T08 | PWM frequency change via HA number entity | `ledc.set_frequency` applies live, no reboot; scope trace matches |
| T09 | Min-% floor | Setting min-% = 30 while temp is below curve[0] → fan runs at 30 % |
| T10 | Power cycle | Restart counter increments; tunable settings preserved |

These are manual; no Unity harness exists under ESPHome. Document results
in a lightweight `docs/test_log.md` when running the rig.

## 7. History

### v0.2.0 — ESPHome migration
The project originally shipped (v0.1.0, v0.1.1) as a custom PlatformIO +
Arduino-ESP32 firmware — ~2,000 lines of C++ across 12 source files, with
hand-written modules for DHT22 reading, LEDC PWM, hardware + software
watchdog, NVS wrapper, MQTT + HA auto-discovery, AsyncWebServer + WebSocket,
captive portal, and ElegantOTA with RSA signature verification.

That firmware worked, built green, and shipped a signed v0.1.1 release. The
user then asked for an architectural review comparing PlatformIO/Arduino
vs. ESPHome for a one-off, personal-use, LAN-only battery-room controller.

The conclusion: for this specific deployment and maintainer profile,
ESPHome wins on:

- **Long-term maintenance**: ecosystem carries the ESP32 Arduino-core churn
  (already bitten by `me-no-dev/ESPAsyncWebServer` being dropped from the
  PIO registry mid-build).
- **LOC footprint**: ~2,000 lines of C++ → ~200 lines of YAML + 1 non-trivial
  lambda.
- **HA integration quality**: native API beats MQTT for responsiveness and
  configuration friction.
- **Hand-off**: anyone can pick up a YAML file in an afternoon; C++ +
  FreeRTOS + Arduino + ESP-IDF is a much steeper ramp.

The tradeoff was RSA-signed OTA, a polished 4-section custom web UI, and a
handful of Unity tests. For a LAN-only device you own, none of those are
load-bearing. See `SECURITY.md` for the current auth model and how the
v0.1.x signed releases remain verifiable via the preserved public key.

### v0.1.1 — First RSA-signed release (2026-04-19, retired)
`SECRET_RSA_KEY` repo secret configured; release workflow attached
`firmware.signed.bin`. Verified end-to-end locally.

### v0.1.0 — First end-to-end custom-C++ release (2026-04-19, retired)
Six-agent rollout (architect / firmware / mqtt / webserver / qa / devops).
~1,800 lines of C++, 7 Unity tests on pure fan-curve logic. Full plan
archived under the `v0.1.0` tag.

## 8. Agent roles (for future Claude Code sessions)

With the code collapsed to a single YAML file, the earlier six-role split
(firmware / mqtt / webserver / qa / architect / devops) is overkill.
Two effective roles remain:

- **esphome-engineer** — owns `fancontrol.yaml`. Adds components, tunes
  lambdas, keeps the YAML idiomatic. Must run `esphome compile` before
  merging.
- **devops-engineer** — owns `.github/`, the release flow, dependabot,
  CHANGELOG discipline.

Security-sensitive changes (OTA, API encryption, secrets handling) should
be flagged explicitly in the PR body and cross-checked against
`SECURITY.md`.

## 9. Out of scope (for now)

- Factory-reset via physical boot-button long-press (GPIO 0 held at
  runtime). Currently only the `button.factory_reset` HA entity.
- MQTT TLS with CA pinning. Native API is preferred; if someone really
  wants TLS-to-broker they can wire it up.
- Brute-force protection on ESPHome OTA. Handle network-side.
- Web dashboard authentication. Trust the LAN or put it behind a reverse
  proxy.

## 10. Dependencies

- ESPHome 2026.4.0+ (pinned in CI via `version: latest`; bump review on
  each new ESPHome release via CHANGELOG)
- ESP-IDF / Arduino-ESP32 — pulled in transitively by ESPHome

No direct C++ library dependencies; ESPHome manages them.

---

*This document is the project's single source of truth for intent and
scope. `fancontrol.yaml` is the single source of truth for behaviour.*
