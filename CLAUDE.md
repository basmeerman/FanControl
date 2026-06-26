# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository shape

FanControl is an **ESPHome** project. The firmware is defined by a single YAML
file, [`fancontrol.yaml`](fancontrol.yaml), which ESPHome compiles to an ESP32
binary. There is no custom C++ source tree: the old PlatformIO + Arduino
architecture was retired in v0.2.0 (see [`PROJECT_PLAN.md`](PROJECT_PLAN.md)
"History" for why, and the [v0.1.x tags](https://github.com/basmeerman/FanControl/tags)
if you need to reference the old code).

Single source of truth: **`fancontrol.yaml`**. Everything runtime-visible
(entities, thresholds, fan curve, PWM frequency, alarms, captive portal, OTA,
HA integration) is declared there.

## Hardware

- **Board:** LOLIN D32 (ESP32-WROOM-32)
- **Sensor:** DHT22 with onboard 10 kΩ pull-up
- **Fan:** Ruck EM 125L EC 02 — PWM input 5–10 V, 1–5 kHz (datasheet)
- **PWM path:** ESP32 GPIO 16 → TXS0108E / 74HCT125 level shifter → MT3608 boost → 9 V PWM line

Pin map, locked in `fancontrol.yaml`:

| GPIO | Role | Notes |
|---|---|---|
| 4 | DHT22 data | module has its own pull-up; firmware treats pin as plain input |
| 16 | Fan PWM (LEDC ch0) | default 1 kHz, 10-bit auto |
| 5 | Status LED | onboard LOLIN D32 LED, active LOW |
| 0 | Boot button | reserved for factory reset long-press (not yet wired in YAML) |

## Commands

```bash
pipx install esphome                         # one time
cp secrets.yaml.example secrets.yaml         # fill in real values
esphome config fancontrol.yaml               # lint only
esphome compile fancontrol.yaml              # full build (generates firmware.{bin,ota.bin,factory.bin} under .esphome/build/)
esphome run fancontrol.yaml                  # compile + flash via USB (first boot) or OTA (afterwards)
esphome logs fancontrol.yaml                 # stream runtime logs from the device
```

CI runs `esphome config` + full compile on every push/PR to `main` / `develop`
and uploads the built binaries as artifacts. A tag push `v*.*.*` to `main`
fires `release.yml`, which compiles a release build and publishes a GitHub
Release with `firmware.bin`, `firmware.ota.bin`, `firmware.factory.bin`, and
their SHA-256 checksums.

**Release signing:** v0.2.0 onward has **no RSA signing** — ESPHome OTA uses
password + checksum. Pre-v0.2.0 releases (v0.1.0, v0.1.1) are still RSA-signed
and verifiable against `docs/signing_public_key.pem`; see [`SECURITY.md`](SECURITY.md).

## Architecture constraints (enforce these)

Because the project lives in one YAML file, "architecture" is mostly about
**keeping the YAML idiomatic ESPHome** rather than growing a parallel
custom-code layer. Concretely:

- **Prefer built-in components over lambdas.** `sensor.template` with filters,
  `binary_sensor.template`, `number.template`, `output.ledc`, `status_led`,
  `button.factory_reset` etc. exist for a reason. Reach for a lambda only when
  no component fits (e.g. the piecewise-linear fan curve).
- **Lambdas stay short.** The fan-curve interpolation is the only non-trivial
  piece of C++ in the repo. If a lambda grows past ~30 lines, split it into a
  reusable `lambda`-scoped helper via [external components](https://esphome.io/components/external_components.html)
  rather than inflating a sensor block.
- **Every runtime-tunable setting is a `number.template` with `restore_value: true`.**
  This is the NVS-persisted-defaults pattern. Don't invent alternatives.
- **Safety behaviour must be encoded in the YAML, not assumed.** The
  sensor-stall failsafe (fan to 100 % after 60 s of no reads) is implemented
  inside the `Fan Speed` template sensor lambda. If you refactor, preserve it.
- **No MQTT-specific code unless the user explicitly wants MQTT.** The default
  HA integration is ESPHome's `api:` (native, encrypted). An `mqtt:` block is
  commented out in the YAML and may be enabled, but don't add MQTT as a
  parallel publishing path.
- **Secrets stay in `secrets.yaml` (gitignored).** Never hard-code WiFi, API
  encryption keys, or OTA passwords into `fancontrol.yaml`.
- **All UI labels in English.** (Inherited from the v0.1.x language decision.)
- **Frequency clamp for the fan PWM:** 1000–5000 Hz, matching the Ruck
  datasheet. The `number` component enforces this.
- **Boot count must always increment on every boot.** That's the signal HA
  uses to detect unexpected restarts; if you remove or rearrange the
  `on_boot: globals.set` block, add back an equivalent.

## Language note

The old PROJECT_PLAN.md was in Dutch; v0.2.0 rewrote it in English to match
the YAML comments and entity labels. All new documentation, commit messages,
and CHANGELOG entries should be in English.

## When touching `fancontrol.yaml`

1. `esphome config fancontrol.yaml` must pass.
2. `esphome compile fancontrol.yaml` must succeed. Lambdas only fail at compile
   time (not config time), so compile is non-optional before committing.
3. Update `CHANGELOG.md` under `## [Unreleased]`.
4. Open a PR against `main` (branch-protected; direct pushes blocked).
