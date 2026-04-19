# FanControl tests

PlatformIO + Unity. Two tiers: host-side (fast, run in CI) and
hardware-in-the-loop (deferred until real hardware is on the bench).

## Layout

```
test/
  test_fan_curve/          host (pio test -e native) — pure-logic coverage
                           for src/fan_curve.h (temperature → PWM percent).
```

## Running

```bash
# All host tests — runs in CI, must pass.
pio test -e native

# Single folder during development.
pio test -e native -f test_fan_curve
```

## What runs where

| Suite | Env | Purpose |
|---|---|---|
| `test_fan_curve` | `native` | Pure interpolation in `src/fan_curve.h`. No Arduino deps. |

## What is NOT yet covered

These map to hardware / system boundaries that need either real silicon
or a dedicated mock layer. They are on the roadmap but out of scope for
the first-wave host tests.

- **Storage (NVS / `Preferences.h`).** Reading/writing via
  `src/storage.cpp` calls into the ESP32 Preferences API. Host coverage
  requires a `Preferences` mock, which is out of scope for this round.
- **Sensor (DHT22).** `src/sensor.cpp` pulls the Adafruit DHT library
  and reads a GPIO — hardware-in-the-loop only.
- **Fan LEDC PWM.** `src/fan.cpp` calls `ledcSetup/ledcWrite` — the
  pure temperature-curve logic is already extracted into
  `src/fan_curve.h` and covered here; the LEDC side needs hardware.
- **Watchdog.** `src/watchdog.cpp` uses `esp_task_wdt` — hardware only.
- **MQTT / WiFi / WebSocket.** All network modules are hardware-only.
- **Test matrix T01–T12** (see `PROJECT_PLAN.md` §7.2). These are
  integration / fault-injection tests that require wired hardware
  (sensor disconnect, WiFi drop, OTA, 72 h soak). Tracked separately.

## Rule

Every new **pure-logic** function (no Arduino or ESP32 headers required
to compile it) gets a Unity test under `test/` **before merge**.
Extract the logic into a header-only or platform-neutral `.cpp` module
if the existing one is too entangled with the hardware layer — the
`src/fan_curve.h` split is the reference pattern.
