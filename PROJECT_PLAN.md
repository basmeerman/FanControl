# FanControl — Project Plan v1.2

> **Status:** Klaar voor uitvoering in Claude Code / CLI  
> **Gebaseerd op:** SmartEVSE-3.5 architectuur (basmeerman/dingo35)  
> **Hardware:** LOLIN D32 ESP32 · DHT22 · Ruck EM 125L EC 02 (PWM)  
> **Platform:** PlatformIO · Arduino Framework  
> **Repository:** GitHub — nieuw aan te maken, volledig CI/CD ingericht  

---

## 1. Projectoverzicht

Een zelfstandige ventilatie controller voor een thuisaccu ruimte (Victron Multiplus / JK BMS / 6x 48V LFP), gebouwd op een ESP32 LOLIN D32. De controller regelt een Ruck EM 125L EC 02 buisventilator via PWM op basis van temperatuur, communiceert via MQTT met Home Assistant (auto-discovery), en biedt een embedded webserver met live data via WebSockets — ontwerp geïnspireerd op SmartEVSE-3.5.

---

## 2. Featurelijst (definitief)

### F1 — Kern ventilatiefunctie
- F1.1 PWM ventilator sturing (default 1 kHz, runtime instelbaar 1–5 kHz via web UI, 0–100%, via level shifter + MT3608 naar 9V). Datasheet Ruck EM 125L EC 02: PWM 5–10 V, 1–5 kHz.
- F1.2 Temperatuurgestuurde regeling via DHT22 (lineaire interpolatie)
- F1.3 Luchtvochtigheidsmonitoring
- F1.4 Instelbare temperatuurdrempels (via webinterface, opgeslagen in NVS)
- F1.5 Minimale basisventilatie (nooit 0% onder normaal bedrijf, standaard 10%)
- F1.6 Fail-safe: ventilator naar 100% bij sensorstoring of watchdog timeout

### F2 — Veiligheid & Watchdog
- F2.1 ESP32 hardware TWDT (Task Watchdog Timer) via `esp_task_wdt`
- F2.2 Software watchdog: sensorstilstand detectie binnen 60 seconden
- F2.3 Automatische herstart max 1x per 5 minuten (cooldown in NVS)
- F2.4 Herstartteller persistent in NVS
- F2.5 Instelbare alarmdrempel temperatuur (standaard 35°C)
- F2.6 MQTT last will message bij onverwacht verbindingsverlies
- F2.7 Alle alarmen zichtbaar op webinterface én via MQTT

### F3 — Webserver (single page, secties — SmartEVSE stijl)
**Architectuur:** Eén `index.html` embedded als PROGMEM string, AsyncWebServer + AsyncWebSocket

**Sectie 1 — Status (live via WebSocket)**
- F3.1 Temperatuur (°C) realtime
- F3.2 Luchtvochtigheid (%) realtime
- F3.3 Ventilatorsnelheid (%) realtime + visuele indicator/balk
- F3.4 MQTT verbindingsstatus met broker IP
- F3.5 WiFi SSID, RSSI signaalsterkte, IP adres
- F3.6 Uptime (dd:hh:mm:ss)
- F3.7 Alarmstatus (temperatuur / sensor / watchdog) — kleurgecodeerd
- F3.8 **WebSocket live push** vanuit ESP32 elke 2 seconden (geen polling)
- F3.9 Watchdog herstartteller

**Sectie 2 — Ventilatie instellingen (opvouwbaar, SmartEVSE stijl)**
- F3.10 Temperatuurdrempels instellen (T1..T4 met bijbehorend PWM%) + PWM frequentie (1000–5000 Hz, stap 100 Hz)
- F3.11 Alarmtemperatuur instellen
- F3.12 Minimale ventilatorsnelheid instellen
- F3.13 Sensoruitleesinterval instellen
- F3.14 Opslaan knop → opgeslagen in NVS

**Sectie 3 — Netwerk & MQTT instellingen (opvouwbaar)**
- F3.15 WiFi SSID + wachtwoord
- F3.16 MQTT broker IP + poort
- F3.17 MQTT gebruikersnaam + wachtwoord
- F3.18 MQTT topic prefix
- F3.19 Apparaatnaam (voor MQTT discovery label)
- F3.20 Opslaan knop → opgeslagen in NVS, herverbinden

**Sectie 4 — Systeem (opvouwbaar)**
- F3.21 Firmware versie + build datum
- F3.22 Vrij heap geheugen
- F3.23 OTA firmware update (ElegantOTA, wachtwoord beveiligd)
- F3.24 Herstart knop (met bevestigingsdialoog)
- F3.25 Factory reset knop (met bevestigingsdialoog)
- F3.26 Debug log (laatste 20 regels, realtime via WebSocket)

**Ontwerp:**
- Responsief, mobiel-vriendelijk (min 380px)
- Donker thema, kleurindicatoren voor alarmstatus
- Secties opvouwbaar (accordion stijl, zoals SmartEVSE)
- Geen externe CDN afhankelijkheden (alles embedded)
- Vanilla JS + CSS (geen React/Vue, klein footprint)

### F4 — MQTT
- F4.1 Home Assistant auto-discovery (`homeassistant/sensor/.../config`)
- F4.2 Temperatuur publicatie elke 15s (instelbaar)
- F4.3 Luchtvochtigheid publicatie
- F4.4 Ventilatorsnelheid publicatie bij wijziging
- F4.5 Alarmstatus publicatie bij event
- F4.6 Watchdog status + herstartteller
- F4.7 Online/offline (birth/will, retain=true)
- F4.8 Alle topics onder instelbare prefix

### F5 — WiFi & Netwerk
- F5.1 Verbinding via opgeslagen credentials (NVS)
- F5.2 Captive portal AP mode als geen credentials beschikbaar
- F5.3 Automatisch herverbinden bij WiFi verlies
- F5.4 mDNS hostname (bijv. `fancontrol.local`)
- F5.5 Statisch IP optioneel instelbaar

### F6 — OTA Updates
- F6.1 OTA via ElegantOTA webinterface (wachtwoord beveiligd)
- F6.2 OTA via PlatformIO `espota` voor CLI gebruik
- F6.3 Automatische herstart na succesvolle update
- F6.4 Fallback naar oude firmware bij mislukte flash

### F7 — Opslag (NVS)
- F7.1 WiFi credentials
- F7.2 MQTT configuratie
- F7.3 Temperatuurdrempels en ventilatie-instellingen
- F7.4 Herstartteller en cooldown timestamp
- F7.5 Apparaatnaam
- F7.6 Factory reset via webinterface of fysieke GPIO pin (boot button)

---

## 3. Projectstructuur (PlatformIO)

```
FanControl/
├── platformio.ini
├── README.md
├── CHANGELOG.md
├── docs/
│   ├── wiring_diagram.md
│   ├── mqtt_topics.md
│   └── api.md
├── test/
│   ├── test_sensor/
│   ├── test_mqtt/
│   └── test_watchdog/
└── src/
    ├── main.cpp              # Setup, loop, task orchestratie
    ├── version.h             # Versienummer + build datum
    ├── config.h              # Constanten, default waarden
    ├── sensor.h / .cpp       # DHT22 uitlezing, kalibratiefilter
    ├── fan.h / .cpp          # PWM sturing, snelheidsberekening
    ├── watchdog.h / .cpp     # HW + SW watchdog logica
    ├── mqtt.h / .cpp         # MQTT client, discovery, publicatie
    ├── webserver.h / .cpp    # AsyncWebServer + WebSocket handler
    ├── websocket.h / .cpp    # WebSocket JSON push logica
    ├── storage.h / .cpp      # NVS lees/schrijf wrapper
    ├── wifi_manager.h / .cpp # WiFi + captive portal + mDNS
    └── index_html.h          # Embedded HTML/CSS/JS als PROGMEM
```

---

## 4. platformio.ini

```ini
[env:lolin_d32]
platform = espressif32
board = lolin_d32
framework = arduino
monitor_speed = 115200
board_build.partitions = min_spiffs.csv

; OTA via CLI
upload_protocol = espota
upload_port = fancontrol.local
upload_flags = --auth=changeme

lib_deps =
  knolleary/PubSubClient@^2.8
  adafruit/DHT sensor library@^1.4.6
  adafruit/Adafruit Unified Sensor@^1.1.14
  esp32async/ESPAsyncWebServer@^3.7.0     ; v1.2: maintained fork (me-no-dev/* removed from registry)
  esp32async/AsyncTCP@^3.4.0
  ayushsharma82/ElegantOTA@^3.1.0
  bblanchon/ArduinoJson@^7.0.0

build_flags =
  -DCORE_DEBUG_LEVEL=0
  -DVERSION='"1.0.0"'
```

---

## 5. WebSocket Protocol (ESP32 → Browser)

JSON push elke 2 seconden:

```json
{
  "type": "status",
  "temperature": 24.3,
  "humidity": 58.2,
  "fan_speed": 45,
  "fan_mode": "auto",
  "alarm": {
    "temperature": false,
    "sensor": false,
    "watchdog": false
  },
  "mqtt": {
    "connected": true,
    "broker": "192.168.1.10"
  },
  "wifi": {
    "ssid": "MyNetwork",
    "rssi": -62,
    "ip": "192.168.1.50"
  },
  "system": {
    "uptime": 86400,
    "heap_free": 142320,
    "restarts": 2,
    "version": "1.0.0"
  }
}
```

Browser → ESP32 (instellingen opslaan):
```json
{
  "type": "set_config",
  "thresholds": [15, 20, 25, 30, 35],
  "pwm_levels": [10, 25, 50, 75, 100],
  "alarm_temp": 35,
  "min_fan": 10
}
```

---

## 6. MQTT Topics

| Topic | Richting | Inhoud |
|---|---|---|
| `{prefix}/sensor/temperature/state` | → HA | `24.3` |
| `{prefix}/sensor/humidity/state` | → HA | `58.2` |
| `{prefix}/sensor/fan_speed/state` | → HA | `45` |
| `{prefix}/binary_sensor/alarm_temp/state` | → HA | `ON`/`OFF` |
| `{prefix}/binary_sensor/alarm_sensor/state` | → HA | `ON`/`OFF` |
| `{prefix}/binary_sensor/watchdog/state` | → HA | `ON`/`OFF` |
| `{prefix}/sensor/restarts/state` | → HA | `2` |
| `{prefix}/status` | → HA | `online`/`offline` |
| `homeassistant/sensor/{id}/config` | → HA | discovery JSON |

---

## 7. Kwaliteitsstructuur

### 7.1 Kwaliteitsniveaus

| Niveau | Omschrijving | Toets |
|---|---|---|
| **Q1 — Compileerbaar** | Code compileert zonder warnings | `pio run` |
| **Q2 — Functioneel** | Alle features werken op hardware | Handmatige test checklist |
| **Q3 — Stabiel** | 72 uur onafgebroken runtime zonder herstart | Soak test |
| **Q4 — Veilig** | Fail-safe werkt bij elke faalscenario | Injectietestruns |
| **Q5 — Gedocumenteerd** | Alle publieke functies gedocumenteerd | Code review |

### 7.2 Testmatrix

| Test | Beschrijving | Verwacht resultaat |
|---|---|---|
| T01 | DHT22 losgekoppeld tijdens bedrijf | Ventilator → 100%, alarm MQTT |
| T02 | WiFi AP uitgeschakeld | Auto-herverbinding binnen 30s |
| T03 | MQTT broker offline | Ventilatie blijft werken, alarm UI |
| T04 | Watchdog trigger (60s geen data) | Software reboot, MQTT offline |
| T05 | 5 reboots binnen 5 minuten | Cooldown actief, HA notificatie |
| T06 | OTA update via webinterface | Nieuwe firmware actief na herstart |
| T07 | OTA update via CLI (`pio run -t upload`) | Identiek aan T06 |
| T08 | WebSocket verbinding browser | Live updates elke 2s zichtbaar |
| T09 | Instellingen opslaan + herstart | Instellingen behouden in NVS |
| T10 | Factory reset | Alle NVS gewist, captive portal |
| T11 | Temperatuur boven 35°C | Ventilator 100%, alarm UI + MQTT |
| T12 | 72 uur soak test | Geen crashes, memory stabiel |

### 7.3 Code kwaliteitsregels

- Geen globale variabelen buiten `config.h` defaults
- Elke module heeft eigen header met doxygen commentaar
- Geen blocking calls in loop() langer dan 10ms
- WebSocket en MQTT in aparte FreeRTOS tasks
- Alle NVS writes gelogd met result check
- Geen `delay()` — gebruik `millis()` of `vTaskDelay()`

---

## 8. Claude Agent Team Definitie

Geïnspireerd op de SmartEVSE-3.5 multi-contributor aanpak waarbij verschillende experts elk een domein bezitten. Voor dit project worden de volgende Claude agent rollen gedefinieerd voor gebruik in Claude Code CLI:

---

### Agent 1 — `firmware-engineer`
**Domein:** C++ firmware, PlatformIO, ESP32 hardware  
**Skills:** ESP32 TWDT, LEDC PWM, FreeRTOS tasks, NVS, ArduinoJson  
**Verantwoordelijk voor:** `main.cpp`, `fan.cpp`, `watchdog.cpp`, `sensor.cpp`, `storage.cpp`  
**Kwaliteitseis:** Q1 + Q2 + Q4  
**Prompt hint voor CLI:**
```
You are the firmware-engineer agent. Focus only on C++ firmware for ESP32/PlatformIO.
Ensure: no blocking calls >10ms in loop(), hardware watchdog via esp_task_wdt,
PWM via LEDC API, DHT22 via Adafruit library, NVS via Preferences.h.
Always check return values of NVS operations.
```

---

### Agent 2 — `webserver-engineer`
**Domein:** AsyncWebServer, WebSockets, embedded HTML  
**Skills:** ESPAsyncWebServer, AsyncWebSocket, PROGMEM strings, JSON  
**Verantwoordelijk voor:** `webserver.cpp`, `websocket.cpp`, `index_html.h`  
**Kwaliteitseis:** Q1 + Q2 + Q5  
**Prompt hint voor CLI:**
```
You are the webserver-engineer agent. Build a single-page embedded webserver
inspired by SmartEVSE-3.5 (basmeerman/dingo35). Single index.html in PROGMEM.
Sections: Status (live WebSocket), Ventilation Settings, Network/MQTT Settings,
System/OTA. Dark theme, accordion sections, vanilla JS only, no CDN dependencies.
WebSocket pushes JSON status every 2 seconds from ESP32 to browser.
```

---

### Agent 3 — `mqtt-engineer`
**Domein:** MQTT protocol, Home Assistant integratie  
**Skills:** PubSubClient, HA MQTT discovery, retain, QoS, birth/will  
**Verantwoordelijk voor:** `mqtt.cpp`, `docs/mqtt_topics.md`  
**Kwaliteitseis:** Q1 + Q2 + Q5  
**Prompt hint voor CLI:**
```
You are the mqtt-engineer agent. Implement MQTT using PubSubClient for ESP32.
Use Home Assistant MQTT auto-discovery format (homeassistant/ prefix).
Implement birth message (online), last will (offline, retain=true).
All topics configurable via NVS prefix. Reconnect logic with exponential backoff.
Never block loop() during reconnect.
```

---

### Agent 4 — `qa-engineer`
**Domein:** Testing, kwaliteitsbewaking, documentatie  
**Skills:** Unity test framework (PlatformIO), checklist verificatie, soak testing  
**Verantwoordelijk voor:** `test/`, `docs/`, `CHANGELOG.md`, testmatrix T01-T12  
**Kwaliteitseis:** Q1 t/m Q5 verificatie  
**Prompt hint voor CLI:**
```
You are the qa-engineer agent. Your job is to verify all features against the
feature list F1-F7 and run the test matrix T01-T12. Write Unity unit tests for
sensor.cpp, watchdog.cpp, storage.cpp. Create test injection scripts for
failure scenarios (sensor disconnect, WiFi loss, MQTT offline, watchdog trigger).
Document all results. Flag any feature deviating from spec.
```

---

### Agent 5 — `architect` (coördinator)
**Domein:** Projectcoördinatie, integratie, architectuurbewaking  
**Skills:** Alle domeinen op hoog niveau, code review, merge beslissingen  
**Verantwoordelijk voor:** `platformio.ini`, `config.h`, `version.h`, algehele architectuur  
**Kwaliteitseis:** Q1 t/m Q5 — eindverantwoordelijk  
**Prompt hint voor CLI:**
```
You are the architect agent. You coordinate all other agents and own the overall
architecture. Enforce: no circular dependencies between modules, all modules
testable in isolation, WebSocket and MQTT in separate FreeRTOS tasks,
config.h as single source of truth for all defaults. Review all PRs/changes
from other agents before integration into main.cpp.
```

---

## 9. Uitvoeringsvolgorde (Fasen)

### Fase 1 — Kern firmware (Agent: firmware-engineer + architect)
1. PlatformIO project initialiseren
2. `config.h` met alle defaults
3. `version.h`
4. `storage.cpp` — NVS wrapper
5. `sensor.cpp` — DHT22 uitlezing + filter
6. `fan.cpp` — PWM LEDC sturing
7. `watchdog.cpp` — HW + SW watchdog
8. `main.cpp` — integratie + FreeRTOS tasks
9. **QA:** Tests T01, T04, T05, T11

### Fase 2 — MQTT (Agent: mqtt-engineer + qa-engineer)
1. `mqtt.cpp` — verbinding + reconnect
2. HA auto-discovery topics
3. Alle publicatie functies
4. Birth/will implementatie
5. **QA:** Tests T02, T03

### Fase 3 — Webserver + WebSocket (Agent: webserver-engineer + architect)
1. `webserver.cpp` — AsyncWebServer setup
2. `websocket.cpp` — JSON push logica
3. `index_html.h` — embedded HTML, SmartEVSE stijl, 4 secties
4. OTA integratie (ElegantOTA)
5. **QA:** Tests T06, T07, T08, T09, T10

### Fase 4 — Integratie & soak test (Alle agents)
1. Volledige integratie test
2. 72 uur soak test
3. **QA:** Tests T12 + volledige T01-T11 rerun
4. Documentatie finaliseren
5. `CHANGELOG.md` bijwerken

---

## 10. GitHub Repository Setup

### 10.1 Nieuwe repository aanmaken

```bash
# Via GitHub CLI (gh) — uitvoeren in Claude Code CLI
gh repo create FanControl \
  --public \
  --description "ESP32 ventilatie controller voor thuisaccu ruimte — Victron/JK BMS" \
  --license MIT \
  --clone

cd FanControl
```

**Repository instellingen:**
- Branch protection op `main`: PR verplicht + CI groen vóór merge
- Branch protection op `develop`: CI groen verplicht
- Default branch: `main`
- Squash merge als standaard strategie
- Discussions en Issues ingeschakeld

### 10.2 Branch strategie (SmartEVSE stijl)

```
main          ← stabiele releases, altijd tag + GitHub Release
  └─ develop  ← integratie branch, CI verplicht
       ├─ feature/webserver-websocket
       ├─ feature/mqtt-autodiscovery
       ├─ feature/watchdog
       └─ fix/sensor-failsafe
```

Regels:
- `main` krijgt alleen merges van `develop` via PR
- `develop` krijgt merges van `feature/*` en `fix/*`
- Elke tag push naar `main` triggert automatisch een GitHub Release
- Versienummer in `src/version.h` wordt door CI geïnjecteerd vanuit de git tag

### 10.3 GitHub Labels

```bash
gh label create "bug"         --color "d73a4a" --description "Iets werkt niet"
gh label create "enhancement" --color "a2eeef" --description "Nieuwe feature"
gh label create "firmware"    --color "0075ca" --description "ESP32 C++ firmware"
gh label create "webserver"   --color "e4e669" --description "Webserver / WebSocket"
gh label create "mqtt"        --color "cfd3d7" --description "MQTT integratie"
gh label create "ci-cd"       --color "6f42c1" --description "CI/CD pipeline"
gh label create "docs"        --color "0052cc" --description "Documentatie"
gh label create "safety"      --color "ee0701" --description "Veiligheid / watchdog"
gh label create "question"    --color "d876e3" --description "Vraag of discussie"
```

### 10.4 Repository structuur (root niveau)

```
FanControl/
├── .github/
│   ├── workflows/
│   │   ├── ci.yml                   # Build + test op elke push/PR
│   │   └── release.yml              # Release op tag push naar main
│   ├── ISSUE_TEMPLATE/
│   │   ├── bug_report.md
│   │   └── feature_request.md
│   ├── PULL_REQUEST_TEMPLATE.md
│   └── dependabot.yml
├── src/                             # Firmware broncode
├── test/                            # PlatformIO unit tests
├── docs/                            # Documentatie
├── scripts/                         # Hulpscripts
├── platformio.ini
├── README.md
├── CHANGELOG.md                     # Keep-a-changelog formaat
├── CONTRIBUTING.md
└── LICENSE                          # MIT
```

---

## 11. CI/CD Pipeline (GitHub Actions — SmartEVSE stijl)

### 11.1 Workflow 1 — CI: Build & Test (`ci.yml`)

**Trigger:** elke push en PR naar `main` en `develop`

```yaml
name: PlatformIO CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - uses: actions/cache@v4
        with:
          path: |
            ~/.cache/pip
            ~/.platformio/.cache
          key: ${{ runner.os }}-pio-${{ hashFiles('platformio.ini') }}

      - uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install PlatformIO
        run: pip install --upgrade platformio

      - name: Build firmware
        run: pio run

      - name: Run unit tests (native)
        run: pio test -e native

      - name: Check binary size
        run: |
          SIZE=$(stat -c%s .pio/build/lolin_d32/firmware.bin)
          echo "Firmware size: $SIZE bytes"
          if [ $SIZE -gt 1500000 ]; then
            echo "ERROR: firmware te groot (>1.5MB)"
            exit 1
          fi

      - name: Upload firmware artifact
        uses: actions/upload-artifact@v4
        with:
          name: firmware-${{ github.sha }}
          path: .pio/build/lolin_d32/firmware.bin
          retention-days: 30
```

### 11.2 Workflow 2 — Release (`release.yml`)

**Trigger:** push van tag `v*.*.*` naar `main` — exact zoals SmartEVSE `Create Release`

```yaml
name: Create Release

on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  build-and-release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - uses: actions/cache@v4
        with:
          path: |
            ~/.cache/pip
            ~/.platformio/.cache
          key: ${{ runner.os }}-pio-${{ hashFiles('platformio.ini') }}

      - uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install PlatformIO
        run: pip install --upgrade platformio

      - name: Extract version from tag
        id: version
        run: echo "VERSION=${GITHUB_REF#refs/tags/}" >> $GITHUB_OUTPUT

      - name: Inject version into firmware
        run: |
          sed -i 's/#define VERSION ".*"/#define VERSION "${{ steps.version.outputs.VERSION }}"/' \
            src/version.h

      - name: Build release firmware
        run: pio run

      - name: Rename binary
        run: |
          cp .pio/build/lolin_d32/firmware.bin \
             FanControl-${{ steps.version.outputs.VERSION }}.bin

      - name: SHA256 checksum
        run: |
          sha256sum FanControl-${{ steps.version.outputs.VERSION }}.bin \
            > FanControl-${{ steps.version.outputs.VERSION }}.bin.sha256

      # v1.2: signing matches SmartEVSE-3.5/pio-build.yaml exactly.
      # Repo secret SECRET_RSA_KEY holds the PEM-encoded RSA private key.
      # If the secret is missing the step skips silently (so forks build cleanly).
      - name: Sign firmware
        env:
          super_secret: ${{ secrets.SECRET_RSA_KEY }}
        run: |
          if [ -z "$super_secret" ]; then
            echo "Signing key not set, skipping firmware signing"
            exit 0
          fi
          secret_file=$(mktemp)
          echo "$super_secret" > "$secret_file"
          openssl dgst -sign "$secret_file" -keyform PEM -sha256 \
            -out firmware.sign -binary \
            FanControl-${{ steps.version.outputs.VERSION }}.bin
          cat firmware.sign FanControl-${{ steps.version.outputs.VERSION }}.bin \
            > FanControl-${{ steps.version.outputs.VERSION }}.signed.bin
          rm -f "$secret_file" firmware.sign

      - name: Extract changelog voor deze release
        run: |
          VERSION=${{ steps.version.outputs.VERSION }}
          awk "/^## \[$VERSION\]/{flag=1; next} /^## \[/{flag=0} flag" \
            CHANGELOG.md > release_notes.md

      - name: Create GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          name: "FanControl ${{ steps.version.outputs.VERSION }}"
          body_path: release_notes.md
          draft: false
          prerelease: ${{ contains(steps.version.outputs.VERSION, '-') }}
          files: |
            FanControl-${{ steps.version.outputs.VERSION }}.bin
            FanControl-${{ steps.version.outputs.VERSION }}.bin.sha256
            FanControl-${{ steps.version.outputs.VERSION }}.signed.bin
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### 11.3 Versioning strategie (SemVer)

| Increment | Wanneer | Voorbeeld |
|---|---|---|
| **MAJOR** (x.0.0) | Breaking change MQTT topics of API | v2.0.0 |
| **MINOR** (x.y.0) | Nieuwe feature, backwards compatible | v1.1.0 |
| **PATCH** (x.y.z) | Bugfix, performance | v1.0.1 |
| **Pre-release** | Beta / testversie | v1.1.0-beta.1 |

**Release flow CLI:**
```bash
# 1. Develop naar main mergen
git checkout main
git merge develop --no-ff -m "Release v1.1.0: websocket live updates"

# 2. Tag aanmaken — triggert release.yml automatisch
git tag v1.1.0
git push origin main --tags

# GitHub Actions produceert automatisch:
# → firmware.bin met versie geïnjecteerd vanuit tag
# → SHA256 checksum bestand
# → GitHub Release met CHANGELOG sectie als release notes
```

### 11.4 CHANGELOG.md formaat (Keep-a-Changelog)

```markdown
# Changelog

## [Unreleased]

## [1.1.0] - 2026-05-01
### Added
- WebSocket live updates elke 2 seconden
- Accordion secties in webinterface

### Fixed
- Watchdog cooldown niet correct opgeslagen in NVS

## [1.0.0] - 2026-04-01
### Added
- Eerste stabiele release
- PWM ventilator sturing via LEDC
- MQTT auto-discovery voor Home Assistant
- DHT22 temperatuurgestuurde regeling
- Hardware + software watchdog
```

### 11.5 Dependabot (`dependabot.yml`)

```yaml
version: 2
updates:
  - package-ecosystem: "pip"
    directory: "/"
    schedule:
      interval: "weekly"
    labels:
      - "ci-cd"
```

### 11.6 Pull Request Template

```markdown
## Beschrijving
<!-- Wat doet deze PR? -->

## Gerelateerde issues
Closes #

## Type wijziging
- [ ] Bugfix (F/T nummer: ___)
- [ ] Nieuwe feature (F nummer: ___)
- [ ] Refactor / CI

## Checklist
- [ ] `pio run` compileert zonder warnings
- [ ] `pio test -e native` slaagt
- [ ] Getest op hardware
- [ ] CHANGELOG.md bijgewerkt onder [Unreleased]
- [ ] Geen blocking calls >10ms in loop()
- [ ] NVS writes gecheckt op return value
```

### 11.7 CI/CD Agent (toevoeging agentteam)

**Agent 6 — `devops-engineer`**
**Domein:** GitHub Actions, CI/CD, repository configuratie, release management
**Skills:** GitHub Actions YAML, SemVer, branch protection, artifact management, gh CLI
**Verantwoordelijk voor:** `.github/workflows/`, branch strategie, labels, release flow, CHANGELOG
**Kwaliteitseis:** Elke push compileert groen. Elke release heeft binary + checksum + changelog.
**Prompt hint voor CLI:**
```
You are the devops-engineer agent. Set up and maintain the GitHub repository
and CI/CD pipeline modelled after SmartEVSE-3.5 (dingo35/basmeerman).
Two workflows: ci.yml (build + pio test on every push/PR to main/develop)
and release.yml (triggered on v*.*.* tag push to main, identical to SmartEVSE
'PlatformIO CI' + 'Create Release' workflow split).
Version injected from git tag into src/version.h at build time via sed.
Release artifacts: firmware.bin + sha256 attached to GitHub Release.
CHANGELOG.md in keep-a-changelog format, release notes extracted by awk.
Branch protection: main and develop require CI green before merge.
PlatformIO cache key includes platformio.ini hash for efficient caching.
```

---

## 12. Startinstructies voor Claude Code CLI

Wanneer je dit plan oppakt in de terminal:

```bash
# 0. GitHub repo aanmaken (devops-engineer agent)
# Prompt: "Act as devops-engineer agent. Create the GitHub repo and
#          set up branch protection, labels, and all .github/ files."

# 1. Nieuw PlatformIO project aanmaken
pio init --board lolin_d32 --ide vscode

# 2. Geef dit plan mee als context
# Laad ACCURUIMTE_VENTILATIE_PLAN.md in je Claude Code sessie

# 3. Start met architect agent voor projectstructuur
# Prompt: "Act as architect agent. Initialize the project structure
#          as defined in section 3 of the project plan."

# 4. Ga naar fase 1
# Prompt: "Act as firmware-engineer agent. Implement Phase 1:
#          config.h, storage.cpp, sensor.cpp, fan.cpp, watchdog.cpp"

# 5. Valideer met qa-engineer na elke fase
# Prompt: "Act as qa-engineer agent. Verify Phase 1 against
#          feature list F1+F2 and run tests T01, T04, T05, T11"
```

---

## 13. Afhankelijkheden & Versies

| Library | Versie | Gebruik |
|---|---|---|
| PubSubClient | ^2.8 | MQTT client |
| DHT sensor library | ^1.4.6 | DHT22 sensor |
| Adafruit Unified Sensor | ^1.1.14 | DHT22 dependency |
| ESP Async WebServer | ^1.2.3 | Webserver + WebSocket |
| AsyncTCP | ^1.1.1 | TCP dependency |
| ElegantOTA | ^3.1.0 | OTA webinterface |
| ArduinoJson | ^7.0.0 | JSON serialisatie |
| Preferences (built-in) | ESP32 core | NVS opslag |

---

## 14. Versie & Changelog

**v1.2.0** — FanControl rename + uitvoeringsbesluiten vastgelegd
- Project hernoemd: `accuruimte-ventilatie` → `FanControl` (mDNS, MQTT prefix, hostname, repo allemaal `fancontrol`)
- F1.1 PWM frequentie runtime instelbaar in web UI (1000–5000 Hz, default 1 kHz; was hardcoded)
- F3.10 PWM frequentie toegevoegd aan ventilatie-instellingen UI
- §4 platformio.ini: AsyncWebServer fork → `esp32async/ESPAsyncWebServer` + `esp32async/AsyncTCP` (oude `me-no-dev/*` packages zijn uit de PIO registry)
- §11.2 release.yml: firmware signing toegevoegd identiek aan SmartEVSE-3.5/pio-build.yaml (RSA + openssl SHA256, secret `SECRET_RSA_KEY`, produceert `firmware.signed.bin`)
- Web UI taal: alle labels Engels (was: Nederlands met Engelse identifiers)
- Captive portal SSID: `FanControl-Setup`, geen wachtwoord
- OTA: `changeme` placeholder → forceer wijziging via web UI banner bij eerste boot
- MQTT: TLS support optioneel via web UI (zoals SmartEVSE), default plain TCP
- HA discovery: één HA "device" met `fancontrol-<MAC suffix>` als unique ID, kindentities voor temp/humidity/fan/alarms
- GPIO map vastgelegd: DHT22=4, FAN_PWM=25, STATUS_LED=5, FACTORY_RESET=0

**v1.1.0** — GitHub repo + CI/CD toegevoegd
- Secties 10 + 11: GitHub repo setup en CI/CD pipeline
- Agent 6 devops-engineer toegevoegd
- Branch strategie, labels, PR template, dependabot

**v1.0.0** — Initieel projectplan  
- Gebaseerd op SmartEVSE-3.5 (basmeerman/dingo35) architectuur  
- Single-page webserver met 4 secties, live WebSocket  
- 5 Claude agent rollen gedefinieerd  
- 12-punt testmatrix  
- 4 uitvoeringsfasen

---

*Dit document is de single source of truth voor het FanControl Controller project.*  
*Bewaar dit bestand in de root van de repository als `PROJECT_PLAN.md`.*
