# Changelog

All notable changes to FanControl will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Release notes for each tagged version are extracted from this file by the
`release.yml` workflow — keep the `## [x.y.z] - YYYY-MM-DD` heading format exact.

## [Unreleased]

### Added
- **Operating modes (Auto / Manual).** New `Operating Mode` select switches
  between the existing temperature-driven curve (**Auto**) and a new **Manual**
  mode with a `Manual Fan` on/off switch and a `Manual Fan Speed` (0–100 %)
  number. A single resolved-speed template sensor drives the PWM in both modes,
  so `Fan Speed` always reflects the actual duty. Safety failsafes (NaN read,
  >60 s sensor stall → 100 %) still apply in **all** modes, including Manual.
  Mode + manual controls are grouped on the web UI via `web_server`
  sorting groups (the stock UI can't hide entities by state, so they are shown
  but inert in Auto).

### Changed
- Home Assistant transport switched from the native ESPHome **`api:` to
  `mqtt:`** in `fancontrol.yaml`. The encrypted native-API block is now
  commented out and the MQTT block (broker auto-discovery, `fancontrol`
  topic prefix, online/offline birth & will messages) is enabled. Set
  `mqtt_host` (bare IP, **no port** — the port is the separate `port:` key),
  `mqtt_user`, and `mqtt_password` in `secrets.yaml`. Revert by swapping the
  two blocks back. Verified on hardware: device connects to the broker and
  publishes temperature, humidity, fan speed, and diagnostics.
- Fan PWM output moved from **GPIO 25 → GPIO 16** in `fancontrol.yaml` to
  match the assembled hardware. GPIO 16 is free on the plain LOLIN D32
  (WROOM-32, no PSRAM); only the D32 *Pro* (WROVER) reserves 16/17 for PSRAM.
  Pin map updated in `CLAUDE.md`, `PROJECT_PLAN.md`, and
  `docs/wiring_diagram.md`.

### Fixed
- `.github/dependabot.yml`: dropped the `pip` ecosystem. v0.2.0 removed
  `platformio.ini` and there are no Python manifests left, so Dependabot's
  pip updater was erroring with "No files found in /" on every run.
  `github-actions` updates continue as before.

## [0.2.0] - 2026-04-19

**Architecture switch: custom PlatformIO + Arduino C++ → ESPHome.**

Firmware behaviour is unchanged in spirit (same hardware, same fan curve,
same captive portal, same safety failsafe), but the entire ~2,000-line C++
codebase is replaced by a single ~200-line YAML file. See
`PROJECT_PLAN.md` "History" for the decision rationale.

### Added

- `fancontrol.yaml` — single-source firmware config driving ESPHome:
  - DHT22 on GPIO 4, 5 s polling, last-good caching via ESPHome filters.
  - `output.ledc` PWM on GPIO 25, default 1 kHz, runtime-tunable
    1000–5000 Hz via `number.template` + `ledc.set_frequency` action.
    10-bit resolution auto-computed by ESPHome.
  - `Fan Speed` template sensor with a 5-point piecewise-linear
    temperature-to-PWM lambda matching the v0.1.x curve exactly.
    NaN → 100 %; sensor-stall flag → 100 %.
  - `Sensor Alarm` (60 s stall detection) and `Temperature Alarm`
    (configurable threshold, default 35 °C) as `binary_sensor.template`
    entities with `safety` / `problem` device classes.
  - `Restart Counter` persisted via `globals` + `restore_value: true`,
    incremented in `on_boot`.
  - WiFi STA + open captive-portal AP `FanControl-Setup` via `captive_portal:`.
  - mDNS `fancontrol.local`.
  - ESPHome native API (`api:` with encryption key) as the default HA
    transport. MQTT block left commented out for users who want the
    v0.1.x topic shape.
  - ESPHome `web_server` on port 80 for the built-in dashboard.
  - OTA via `ota:` platform `esphome`, password-gated.
  - `status_led` on GPIO 5 (LOLIN D32 onboard LED).
  - `button.restart` and `button.factory_reset` entities.
- `secrets.yaml.example` — template for WiFi credentials, API encryption
  key, and OTA password. Real `secrets.yaml` gitignored.
- `docs/home_assistant.md` — integration walkthrough, entity table, sample
  automations, optional MQTT-with-TLS recipe.
- `.github/workflows/ci.yml` now uses `esphome/build-action@v10` — full
  compile gate (not just config lint), size gate unchanged (≤ 1.5 MB).
- `.github/workflows/release.yml` publishes `firmware.bin`,
  `firmware.ota.bin`, `firmware.factory.bin`, and their SHA-256 checksums.

### Changed

- **Home Assistant integration transport:** MQTT (with HA auto-discovery)
  → ESPHome native API (encrypted, faster, zero-config on the HA side).
  Users who want MQTT can uncomment the `mqtt:` block in `fancontrol.yaml`.
- **Web UI:** custom 4-section dark-themed single-page PROGMEM UI →
  ESPHome's built-in `web_server` dashboard. The control surface for most
  users becomes Home Assistant itself.
- **Documentation:**
  - `README.md` rewritten for the ESPHome workflow.
  - `CLAUDE.md` rewritten — architecture rules now about keeping the YAML
    idiomatic rather than FreeRTOS task discipline.
  - `CONTRIBUTING.md` rewritten — build loop is `esphome config/compile/run`;
    release cut is merge `develop` → `main` + tag.
  - `PROJECT_PLAN.md` rewritten as v2.0 with an ESPHome-shaped feature list
    and a "History" section archiving the v0.1.x custom-C++ approach.
  - `SECURITY.md` updated — new OTA + API auth model documented; v0.1.x RSA
    signature verification recipe retained for historical artifacts.
  - `docs/wiring_diagram.md` kept (pin map unchanged); references to
    `src/config.h` updated to `fancontrol.yaml`.
  - `docs/mqtt_topics.md` and `docs/api.md` deleted — MQTT is now optional
    and documented inline in the YAML; no custom HTTP / WebSocket API
    exists under ESPHome.

### Removed

- **Entire custom C++ tree** under `src/` (config, storage, sensor, fan,
  fan_curve, watchdog, wifi_manager, mqtt, webserver, websocket,
  index_html, version, main). ~2,000 LOC deleted.
- **Unity test scaffold** under `test/` — ESPHome has no comparable unit
  test concept; the fan-curve lambda was ported verbatim from the Unity-
  tested `fan_curve::computeFromTemperature`, so the logic is unchanged.
- `platformio.ini` — ESPHome handles the PlatformIO build transparently.
- **RSA-signed OTA pipeline.** v0.2.0+ releases ship `firmware.bin` +
  SHA-256 only. The `SECRET_RSA_KEY` repo secret remains configured but
  unused; `docs/signing_public_key.pem` is kept so v0.1.x signed
  binaries remain verifiable. See `SECURITY.md`.

### Migration notes

- **Reflash required.** Pre-v0.2.0 devices store their NVS keys under the
  custom `fancontrol` namespace; ESPHome uses its own global-variables
  namespace layout. Flashing v0.2.0 over v0.1.x will not carry user
  settings forward — reconfigure via the captive portal on first boot.
- **Home Assistant:** remove the MQTT-based device (if you want). Install
  the ESPHome integration if you haven't; paste the API key from
  `secrets.yaml` when prompted.
- **OTA password** in `secrets.yaml` is the one you chose in v0.2.0;
  there is no longer a "changeme" default + first-boot forced-rotation
  banner (ESPHome bakes the password at compile time).

## [0.1.1] - 2026-04-19

First signed release. `SECRET_RSA_KEY` is now configured, so the release
workflow attaches `firmware.signed.bin` alongside the unsigned binary
and SHA-256 checksum. No firmware changes vs. v0.1.0.

### Added
- GitHub repository secret `SECRET_RSA_KEY` (2048-bit RSA private key)
  — enables the previously skipped signing step in `release.yml`.

### Fixed
- (Docs-only in v0.1.0 polish) `release.yml` now correctly strips the
  `v` prefix from the tag before searching the CHANGELOG, so the
  release body is populated from the matching section.

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

[Unreleased]: https://github.com/basmeerman/FanControl/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/basmeerman/FanControl/releases/tag/v0.2.0
[0.1.1]: https://github.com/basmeerman/FanControl/releases/tag/v0.1.1
[0.1.0]: https://github.com/basmeerman/FanControl/releases/tag/v0.1.0
