# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository status

This directory currently contains **only the project plan** — `PROJECT_PLAN.md`. No source code, `platformio.ini`, or git history exists yet. The first implementation sessions will scaffold a PlatformIO project from scratch following that plan.

`PROJECT_PLAN.md` is the **single source of truth** (v1.2). Read it before making structural decisions — especially sections 3 (project structure), 5 (WebSocket protocol), 6 (MQTT topics), and 7.3 (code quality rules).

## Project at a glance

ESP32 firmware for a home battery room ventilation controller:
- **Hardware:** LOLIN D32 ESP32 + DHT22 + Ruck EM 125L EC 02 fan (PWM via level shifter + MT3608 → 9V)
- **Platform:** PlatformIO, Arduino framework, board `lolin_d32`, partitions `min_spiffs.csv`
- **Integrates with:** Home Assistant via MQTT auto-discovery; embedded single-page web UI with WebSocket live push
- **Reference design:** SmartEVSE-3.5 (basmeerman/dingo35) — webserver sections, release flow, and CI structure mirror that project

## Commands (once the PlatformIO project exists)

```bash
pio run                         # build firmware for lolin_d32
pio run -t upload               # OTA upload (espota → accuruimte.local, auth in platformio.ini)
pio run -t uploadfs             # if SPIFFS/LittleFS is added later
pio test -e native              # run Unity unit tests on host
pio test -e native -f test_sensor   # run a single test folder
pio device monitor -b 115200    # serial monitor
```

CI runs `pio run` + `pio test -e native` on every push/PR to `main`/`develop`. A tag push `v*.*.*` to `main` triggers `release.yml`, which injects the tag into `src/version.h` via `sed` and attaches `firmware.bin` + SHA256 to a GitHub Release.

## Architecture constraints (non-obvious, enforce these)

These come from plan §7.3 and the agent briefs — violating them breaks the Q1–Q5 quality gates:

- **No `delay()` anywhere.** Use `millis()` or `vTaskDelay()`. No blocking calls >10ms inside `loop()`.
- **WebSocket and MQTT run in separate FreeRTOS tasks**, not on the main loop. Do not merge them.
- **`config.h` is the only place for defaults.** No other globals outside it.
- **Every NVS write must check its return value and log the result.** NVS is accessed via a `Preferences.h` wrapper in `storage.cpp` — don't call `Preferences` directly from other modules.
- **Fail-safe: on sensor failure or watchdog timeout the fan goes to 100%**, not 0%. Minimum fan speed under normal operation is configurable (default 10%) — never 0.
- **Watchdog:** ESP32 hardware TWDT via `esp_task_wdt` + software watchdog that detects sensor stalls within 60s. Auto-restart is rate-limited to 1× per 5 minutes via an NVS cooldown timestamp; the restart counter is persistent.
- **Webserver is a single `index.html` embedded as a PROGMEM string** in `index_html.h`. No external CDN deps, vanilla JS/CSS only, dark theme, accordion sections. The four sections (Status / Ventilation / Network+MQTT / System) are fixed by the plan.
- **WebSocket push cadence is 2s** (ESP32 → browser, JSON shape in plan §5). Browser → ESP32 uses `{"type": "set_config", ...}` — do not invent new message types without updating the plan.
- **Fan PWM frequency is runtime-tunable** (NVS-backed, 1000–5000 Hz, default 1 kHz per Ruck datasheet). Use `ledcSetup()` / `ledcChangeFrequency()` to apply changes without reboot. Always clamp to spec range in firmware as a second line of defense.
- **Release signing matches SmartEVSE-3.5 exactly:** `openssl dgst -sign -keyform PEM -sha256` against firmware.bin, signature *prepended* to bin → `firmware.signed.bin`. Repo secret `SECRET_RSA_KEY` holds the PEM private key. Step skips silently when the secret is missing so forks build cleanly.
- **MQTT:** birth (`online`) + last will (`offline`, `retain=true`) on `{prefix}/status`. HA discovery under `homeassistant/` prefix. Reconnect uses exponential backoff and must never block the main loop. All topics live under a user-configurable prefix stored in NVS.
- **No circular dependencies between modules.** Every module must be testable in isolation (this is why `test/` has per-module subfolders).

## Planned module layout (plan §3)

```
src/
  main.cpp          # setup + FreeRTOS task orchestration only
  config.h          # all defaults, single source of truth
  version.h         # VERSION macro — CI rewrites this from git tag
  sensor.{h,cpp}    # DHT22 read + calibration filter
  fan.{h,cpp}       # LEDC PWM, speed calc from temperature curve
  watchdog.{h,cpp}  # HW TWDT + SW sensor-stall watchdog
  mqtt.{h,cpp}      # PubSubClient, HA discovery, birth/will
  webserver.{h,cpp} # AsyncWebServer setup
  websocket.{h,cpp} # AsyncWebSocket JSON push
  storage.{h,cpp}   # Preferences wrapper (NVS)
  wifi_manager.{h,cpp}  # WiFi + captive portal + mDNS accuruimte.local
  index_html.h      # embedded HTML/CSS/JS (PROGMEM)
```

## Agent roles referenced by the plan

Plan §8 defines six specialist personas (`firmware-engineer`, `webserver-engineer`, `mqtt-engineer`, `qa-engineer`, `architect`, `devops-engineer`) with scoped ownership and prompt hints. When a user says "act as the X agent", use §8 as the brief — each role has explicit file ownership and quality gates.

## Language note

The plan is in Dutch. **All code, log output, web UI labels, MQTT labels, HA entity names, commit messages, and CHANGELOG entries are English** (v1.2 decision). The plan stays Dutch as a reference document; everything user-facing or runtime is English.

## Test matrix

Plan §7.2 defines T01–T12. When implementing a feature, check which T-tests cover it and note them in the PR. T12 (72-hour soak) gates the Q3 quality level.
