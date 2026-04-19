# Changelog

All notable changes to FanControl will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Release notes for each tagged version are extracted from this file by the
`release.yml` workflow — keep the `## [x.y.z] - YYYY-MM-DD` heading format exact.

## [Unreleased]

## [0.1.0] - 2026-04-19

First end-to-end release. Boots, drives the fan curve from DHT22 readings,
serves the web UI on `fancontrol.local`, publishes to MQTT with Home
Assistant auto-discovery, runs OTA. All quality gates green: `pio run`
SUCCESS, `pio test -e native` 7/7 PASSED.

### Added

- **Hardware abstraction**
  - `storage.{h,cpp}`: NVS wrapper around `Preferences.h` with return-checked writes
    (only module that imports `Preferences`); typed accessors for WiFi, MQTT, fan
    curve, alarm temp, fan-min %, sensor interval, **PWM frequency**, OTA password,
    device name, restart counter, cooldown timestamp; `factoryReset()` clears the
    namespace.
  - `sensor.{h,cpp}`: Adafruit DHT22 driver with last-good cache and
    `isStale()` flag tied to `SENSOR_STALL_TIMEOUT_MS` (60 s).
  - `fan.{h,cpp}`: LEDC PWM driver. `setFrequency()` re-runs `ledcSetup`
    without reboot, clamped to 1000–5000 Hz (Ruck EM 125L EC 02 spec).
    `failsafe()` latches at 100 %.
  - `fan_curve.h`: header-only, Arduino-free pure interpolation
    (`fan_curve::computeFromTemperature`) — host-testable.
  - `watchdog.{h,cpp}`: hardware TWDT (modern `esp_task_wdt_init(&cfg)` with
    legacy fallback) + per-task subscribe/reset + SW sensor-stall detection
    + persistent restart counter with 5-min cooldown.

- **Network + MQTT**
  - `wifi_manager.{h,cpp}`: STA with NVS creds; falls back to open captive AP
    `FanControl-Setup` on `192.168.4.1` with DNSServer catch-all so phones
    detect the portal. mDNS hostname `fancontrol`. Reconnect task on TWDT.
  - `mqtt.{h,cpp}`: PubSubClient task with HA auto-discovery (one device
    `fancontrol-<MAC6>`, child entities for temperature, humidity,
    fan_speed, fan_pwm_freq, restarts + alarm_temp/alarm_sensor/watchdog
    binary_sensors); birth/will (`{prefix}/status` retain=true);
    exponential reconnect (1 s → 60 s); edge-triggered `publishAlarm()`
    with FreeRTOS mutex (PubSubClient is not thread-safe). Optional TLS via
    `WiFiClientSecure` + `setInsecure()` when `port == 8883` (matches
    SmartEVSE-3.5; TODO(security) for cert pinning).

- **Web UI + OTA**
  - `webserver.{h,cpp}`: AsyncWebServer on :80 (esp32async fork — the old
    `me-no-dev` registry packages are gone). ElegantOTA at `/update`,
    gated 403 until the OTA password is changed away from the default.
    Captive-portal magic URLs (Android, iOS/macOS, Windows variants).
  - `websocket.{h,cpp}`: AsyncWebSocket at `/ws`. Status JSON pushed every
    `WS_PUSH_INTERVAL_MS` (2 s) with ArduinoJson 7. Inbound dispatch:
    `set_fan` / `set_network` / `set_ota_password` / `restart` /
    `factory_reset` with monotonic-curve + range validators and `saved`
    ack frames. `cleanupClients()` once per second.
  - `index_html.h`: 14 676 B PROGMEM single-page UI. Vanilla JS, dark theme,
    four `<details>` accordion sections (Status / Ventilation / Network +
    MQTT / System). Sticky red banner forcing OTA-password change on first
    boot. Auto-reconnecting WebSocket. English labels throughout. No CDN
    dependencies.

- **FreeRTOS task layout** (`main.cpp`):
  - Compute on core 1: `sensorTask` (prio 5), `fanTask` (prio 4)
  - Watchdog on core 0 (PRO core), prio 6
  - Network on core 0 next to AsyncTCP/WiFi: `wifiTask` (prio 3),
    `mqttTask` (prio 3, 6 KB stack), `wsTask` (prio 3, 6 KB stack)
  - `loop()` parked at `portMAX_DELAY` per CLAUDE.md

- **Project infrastructure**
  - PlatformIO project for `lolin_d32` (Arduino framework, `min_spiffs.csv`)
  - GPIO map: DHT22 = 4, FAN_PWM = 25, STATUS_LED = 5, FACTORY_RESET = 0
  - CI workflow: `pio run` + 1.5 MB size gate + `pio test -e native` (hard
    gate) + artifact upload
  - Release workflow: tag-triggered, `sed`-injects VERSION into `version.h`,
    builds, SHA256, **signs with `openssl dgst -sign` + `SECRET_RSA_KEY`**
    (matches SmartEVSE-3.5/pio-build.yaml exactly; skip-on-empty so forks
    build cleanly), publishes `.bin` / `.bin.sha256` / `.signed.bin` via
    `softprops/action-gh-release`.
  - Issue + PR templates, dependabot (pip + GitHub Actions, weekly),
    `CONTRIBUTING.md` with signing-key generation and verification recipe.
  - Unity host-test scaffold (`pio test -e native`):
    `test/test_fan_curve/` covers the pure interpolation (edges, exact
    points, midway rounding, NaN failsafe, degenerate-segment guard).
    7 / 7 tests passing.

### Documentation

- `PROJECT_PLAN.md` (formerly `ACCURUIMTE_VENTILATIE_PLAN.md`) — v1.2
  records the execution decisions: PWM frequency runtime-tunable,
  esp32async fork, SmartEVSE-style signing, English UI, captive portal
  defaults, HA discovery grouping, GPIO map, force-OTA-password-change
  on first boot.
- `CLAUDE.md` — architecture constraints for future Claude Code sessions.

### Build verification

- `pio run -e lolin_d32` → SUCCESS, RAM 15.0 %, flash 54.5 %.
- `pio test -e native` → 7 / 7 PASSED.

[Unreleased]: https://github.com/basmeerman/FanControl/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/basmeerman/FanControl/releases/tag/v0.1.0
