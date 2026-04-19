# FanControl

ESP32 ventilation controller for a home battery room (Victron Multiplus / JK BMS / 6× 48 V LFP).
Drives a Ruck EM 125L EC 02 fan via PWM based on DHT22 temperature, exposes live status over an
embedded web UI (WebSocket push every 2 s) and integrates with Home Assistant through MQTT
auto-discovery (one HA device, child entities for temperature, humidity, fan speed, PWM
frequency, restarts and three alarm binary sensors).

- **Hardware:** LOLIN D32 ESP32 + DHT22 + Ruck EM 125L EC 02 (PWM via level shifter + MT3608 → 9 V)
- **PWM:** default 1 kHz, runtime-tunable 1–5 kHz via web UI (per Ruck datasheet)
- **Platform:** PlatformIO, Arduino framework, board `lolin_d32`
- **Design reference:** SmartEVSE-3.5 (basmeerman/dingo35)

## Status

v0.1.0 — first end-to-end release. All modules wired, all quality gates green.
See [`CHANGELOG.md`](CHANGELOG.md) for the full feature list.

| | |
|---|---|
| Build | `pio run` SUCCESS, RAM 15 %, flash 54.5 % (under the 1.5 MB CI gate) |
| Tests | `pio test -e native` 7 / 7 PASSED |
| CI | GitHub Actions builds every push and PR; tag `v*.*.*` triggers signed release |

## Build & flash

```bash
pio run                          # build firmware for lolin_d32
pio run -t upload                # serial upload over USB
pio run -t upload --upload-port=fancontrol.local  # OTA after first boot
pio device monitor -b 115200     # serial monitor
pio test -e native               # host-side Unity tests
pio test -e native -f test_fan_curve   # run a single test folder
```

## First boot

1. Flash via USB the first time.
2. The device starts in captive-portal mode — connect to the open WiFi SSID
   **`FanControl-Setup`** and you'll be redirected to the configuration page.
3. Enter your home WiFi credentials and (optionally) MQTT broker details, save.
4. The device reboots into station mode and is reachable at
   [`http://fancontrol.local`](http://fancontrol.local).
5. The web UI shows a sticky red banner until you set a new OTA password —
   OTA uploads return 403 until then.

## Endpoints

| Path | Purpose |
|---|---|
| `/` | Single-page web UI (status, settings, system) |
| `/ws` | WebSocket — JSON status push every 2 s; inbound config frames |
| `/update` | ElegantOTA (HTTP basic-auth, gated until password is changed) |
| `/healthz` | `200 ok` for upstream monitoring |

## MQTT topics

Published under the configurable prefix (default `fancontrol`). Full table in
[`PROJECT_PLAN.md`](PROJECT_PLAN.md) §6.

```
fancontrol/sensor/temperature/state         24.3
fancontrol/sensor/humidity/state            58.2
fancontrol/sensor/fan_speed/state           45
fancontrol/sensor/fan_pwm_freq/state        1000
fancontrol/sensor/restarts/state            2
fancontrol/binary_sensor/alarm_temp/state   ON|OFF
fancontrol/binary_sensor/alarm_sensor/state ON|OFF
fancontrol/binary_sensor/watchdog/state     ON|OFF
fancontrol/status                           online|offline
```

HA discovery is republished on every (re)connect under `homeassistant/{component}/{node_id}_*`.
TLS is enabled automatically when the broker port is `8883` (insecure mode, matches SmartEVSE).

## Verifying releases

Every tagged release ships `firmware.bin` + SHA-256 + `firmware.signed.bin`.
Verify signature with the public key committed at
[`docs/signing_public_key.pem`](docs/signing_public_key.pem):

```bash
dd if=FanControl-vX.Y.Z.signed.bin of=firmware.sign bs=1 count=256 status=none
dd if=FanControl-vX.Y.Z.signed.bin of=firmware.bin  bs=1 skip=256  status=none
openssl dgst -verify docs/signing_public_key.pem -keyform PEM -sha256 \
  -signature firmware.sign firmware.bin
# → Verified OK
```

Full recipe + key rotation policy in [`SECURITY.md`](SECURITY.md).

## Documentation

- [`PROJECT_PLAN.md`](PROJECT_PLAN.md) — v1.2, the single source of truth (features, test
  matrix, agent roles, CI/CD layout)
- [`CLAUDE.md`](CLAUDE.md) — architecture constraints for future Claude Code sessions
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — branching model + RSA signing-key generation /
  verification recipe

## License

MIT
