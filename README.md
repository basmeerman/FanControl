# FanControl

ESP32 ventilation controller for a home battery room (Victron Multiplus / JK BMS / 6× 48 V LFP).
Drives a Ruck EM 125L EC 02 fan via PWM based on DHT22 temperature, exposes live status over an
embedded web UI (WebSocket push) and integrates with Home Assistant through MQTT auto-discovery.

- **Hardware:** LOLIN D32 ESP32 + DHT22 + Ruck EM 125L EC 02 (PWM via level shifter + MT3608 → 9 V)
- **Platform:** PlatformIO, Arduino framework, board `lolin_d32`
- **Design reference:** SmartEVSE-3.5 (basmeerman/dingo35)

## Status

Phase 1 scaffold — `platformio.ini`, `config.h`, `version.h`, `storage.cpp` (NVS wrapper) in place.
Fan/sensor/watchdog/MQTT/webserver modules follow in phases 2–4.

See [`ACCURUIMTE_VENTILATIE_PLAN.md`](ACCURUIMTE_VENTILATIE_PLAN.md) for the full project plan (v1.1 —
feature list, test matrix, agent roles, CI/CD layout) and [`CLAUDE.md`](CLAUDE.md) for architecture
constraints future Claude Code sessions must respect.

## Build

```bash
pio run                     # build firmware for lolin_d32
pio run -t upload           # serial upload (OTA once device is on network)
pio device monitor -b 115200
pio test -e native          # host-side Unity tests
```

## License

MIT
