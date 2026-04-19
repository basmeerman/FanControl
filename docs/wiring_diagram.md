# FanControl — Wiring Diagram

## Bill of materials

| Item | Notes |
|---|---|
| LOLIN D32 (ESP32-WROOM-32) | Any LOLIN D32 variant. Onboard LED on GPIO 5 (active LOW). Boot button on GPIO 0. |
| DHT22 module **with onboard pull-up** | 10 kΩ pull-up already populated; firmware relies on this. |
| Ruck ETAMASTER EM 125L EC 02 | 230 VAC EC fan; PWM input 5–10 V, 1–5 kHz per datasheet. |
| MT3608 boost module | Configures 3.3 V / 5 V rail up to ~9 V for the fan's PWM input line. |
| Level shifter (3.3 V ↔ 9 V) | TXS0108E 8-ch or 74HCT125 — any channel; pin choice is not dictated by the shifter. |
| Enclosure, 230 V screw terminal, 5 V USB PSU | 1 A on the 5 V rail is plenty. |

## Pin map (`fancontrol.yaml`)

| ESP32 GPIO | Role | Direction | Notes |
|---|---|---|---|
| **4** | `PIN_DHT22` | Input (pulled up on module) | Avoids ADC2 (conflicts with WiFi). |
| **25** | `PIN_FAN_PWM` | LEDC PWM output | Channel `FAN_LEDC_CHANNEL = 0`, default 1 kHz, 10-bit resolution. Drives the level-shifter LV side. |
| **5** | `PIN_STATUS_LED` | Output (active LOW) | Onboard LED. Reserved — not yet driven by firmware. |
| **0** | `PIN_FACTORY_RESET` | Input (boot button) | Hold at boot to trigger factory reset (to be wired into `storage::factoryReset()` as a future hook — currently only web UI calls it). |
| 3V3, GND, VIN (5 V) | Power rails | — | VIN accepts 5 V USB. 3V3 from the onboard regulator feeds the DHT22 module. |

## Rationale

- **GPIO 4 for DHT22**: input-capable, outside the ADC2 block, not a strapping pin. Widely used for DHT sensors in ESP32 examples.
- **GPIO 25 for fan PWM**: LEDC-capable, not a strapping pin, no ADC2 conflict. Next to the DAC pins but we're using LEDC (digital PWM) not DAC.
- **Avoid GPIO 12**: strapping pin (MTDI); pulled HIGH during boot can brick flash in some boards.
- **Avoid GPIO 2 / 15**: boot-time strapping; fine for outputs after boot but safer to leave alone.

## Wiring (text schematic)

```
┌──────────────────────────┐
│        LOLIN D32         │
│                          │
│  GPIO 4  ──── DHT22 DAT  (module has built-in 10 kΩ pull-up to VCC)
│  GPIO 25 ──── LevelShift A1 (LV) ── LevelShift B1 (HV) ── Fan PWM+
│  3V3     ──── DHT22 VCC, LevelShift LV
│  GND     ──── DHT22 GND, LevelShift GND (both sides), Fan GND,
│               MT3608 input GND, 5 V PSU GND
│  5V/VIN  ──── 5 V PSU +, MT3608 input +
└──────────────────────────┘

MT3608: adjust output trimmer to 9.0 V BEFORE connecting the fan.
        Output + → LevelShift HV (VCC_B)
        Output − → common GND (shared with ESP32)

Ruck EM 125L EC 02 control cable (3-wire):
   PWM+ (yellow/white)  ← LevelShift B1 (HV side output)
   GND  (black)         ← common GND
   +V   (red)           ← not used by the controller; fan is 230 V mains-powered separately.
   230 V L / N          → mains, via your own safety-compliant switching/isolation.
```

**Safety:**
- Keep the ESP32 side galvanically isolated from the 230 V side. The fan is mains-powered; the controller only supplies the low-voltage PWM signal.
- Use a screw-terminal block rated for 230 V for the mains side; house in a fire-rated enclosure.
- The MT3608 must be set to **exactly 9 V** before first connect — 10 V max per Ruck spec.

## PWM verification

After first power-on, check the fan PWM line with an oscilloscope or logic analyser:

- **Frequency**: 1000 Hz ± 5 % (default) or whatever is set in the web UI
- **Amplitude**: 9 V ± 0.3 V
- **Duty**: matches `fan_speed` pushed to the web UI / MQTT

If the fan hums audibly and doesn't ramp smoothly, try **2 kHz** in the web UI (still within the 1–5 kHz spec).

## Factory reset

Currently triggered via the ESPHome dashboard's `Factory Reset` button entity
(exposed in both the built-in web UI and Home Assistant). A future enhancement
would detect GPIO 0 (boot button) held for 5 s at runtime and trigger a
factory reset automation; not yet wired into the YAML.
