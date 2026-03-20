# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

CogniDisplay — an AI-powered physical display A/B testing system. ESP32-S3 measures person dwell time (HC-SR04 ultrasonic) and ambient noise (MAX4466 microphone), sends data to a local Flask server, which queries Google Gemini to decide the next LED animation strategy (WS2812B NeoPixel ring). Closed-loop: observe → analyze → act.

The user is an IoT student new to embedded systems. Explain concepts before writing code, get approval before implementing.

## Build & Flash (ESP-IDF 5.5.3)

**IMPORTANT: Never run build/flash/monitor commands. The user always runs these manually.**

```bash
idf.py build                    # compile
idf.py flash -p COM6            # flash to ESP32-S3
idf.py monitor                  # serial output
idf.py flash monitor -p COM6    # flash + monitor combined
idf.py fullclean                # wipe build cache (use when old code persists after flash)
```

## Python Server

```bash
cd server
pip install -r requirements.txt
python app.py               # starts Flask on 0.0.0.0:5000
```

Dashboard: `http://localhost:5000/` — real-time web UI with ESP32 state, trial history, charts, AI reasoning, serial logs.

API endpoints:
- `POST /api/trial` — ESP32 sends sensor data, receives new strategy
- `GET /api/status` — trial history, stats, density, current strategy
- `GET /api/serial` — filtered ESP32 serial logs + live state
- `GET /api/serial/raw` — all serial lines (including distance spam)
- `GET /api/logs` — structured server event log

**Serial port exclusivity:** Flask reads COM6 via background thread (`serial_reader.py`). `idf.py monitor` and Flask cannot use COM6 simultaneously. When running the server, do NOT run `idf.py monitor`. Raw serial output is available via the dashboard "Show Raw" toggle or `server/esp_serial.log`.

## File Structure

```
main/
├── CMakeLists.txt          # Must list all .c files in SRCS
├── idf_component.yml       # espressif/led_strip dependency
├── header/                 # All .h files
│   ├── config.h            # Central config: pins, Wi-Fi, thresholds
│   └── *.h                 # Module headers
└── src/                    # All .c files
    ├── main.c              # State machine + calibration + density
    └── *.c                 # Module implementations
server/
├── app.py                  # Flask endpoints + serial reader startup
├── gemini_client.py        # Gemini API integration
├── serial_reader.py        # Background serial reader (COM6, ESP-IDF log parsing)
├── templates/
│   └── dashboard.html      # Real-time web dashboard (Chart.js, 3s polling)
├── trials.json             # Trial history (auto-generated)
├── esp_serial.log          # Raw serial output (auto-generated, reset each session)
└── .env                    # GEMINI_API_KEY
```

## Architecture

**ESP32 Firmware (C, ESP-IDF):** 6 modules in `main/`, all configured via `header/config.h`:
- `wifi_manager` — Wi-Fi STA, auto-reconnect, EventGroup signaling
- `ultrasonic` — HC-SR04 driver (GPIO4 trig, GPIO5 echo via level converter)
- `mic_adc` — MAX4466 ADC1_CH5 on GPIO6, RMS noise calculation (0-100 scale)
- `led_animation` — WS2812B via `espressif/led_strip` component (RMT backend, GPIO7), 6 animation types, dedicated FreeRTOS task at 30 FPS, mutex-protected strategy
- `http_client` — POST trial data as JSON (cJSON), parse response for new strategy + AI reason
- `main.c` — 3-state machine (IDLE → PERSON_DETECTED → PERSON_LEFT), 100ms scan cycle, adaptive baseline calibration, density tracking (2min window), idle mode (solid red after 2min inactivity)

**Python Server:** Three components:
- `app.py` — Flask routes, structured event logging (in-memory ring buffer), Gemini fallback (random strategy if API fails)
- `gemini_client.py` — Gemini API (`gemini-3.1-flash-lite-preview`), system prompt defines AI as "Marketing Display Expert"
- `serial_reader.py` — Background thread reads COM6, parses ESP-IDF log format (`I (12345) TAG: message`), maintains live ESP32 state dict, dual buffers (filtered events for dashboard + raw for debugging), writes `esp_serial.log`. Flask debug mode guard: serial reader only starts in `WERKZEUG_RUN_MAIN` child process to avoid double port opens.

Trial history in `server/trials.json`. API key in `server/.env`.

## Data Flow

ESP32 → Server (POST /api/trial):
```json
{"dwell_time_ms": 4500, "noise_level": 42.3, "current_strategy": {"animation": "breathing", "r": 0, "g": 0, "b": 255, "speed": 50}, "density_score": 0.45, "density_category": "medium", "person_count_2min": 3}
```

Server → ESP32 (response):
```json
{"animation": "wave", "r": 255, "g": 100, "b": 0, "speed": 75, "reason": "High density with moderate noise..."}
```

## Key Design Decisions

- **GPIO4-7** chosen to avoid strapping/USB/JTAG/flash pins. GPIO6 specifically for ADC1 (ADC2 conflicts with Wi-Fi).
- **Level converter required** for HC-SR04 (5V echo → 3.3V) and NeoPixel (3.3V data → 5V). 4-channel bi-directional.
- **Adaptive baseline:** Startup calibration (4s), dynamic thresholds (baseline×0.70 detect, baseline×0.85 depart), auto-updates after 60s stable reading.
- **Density tracking:** 2-minute sliding window, circular buffer of 20 events, composite score (person_count×0.4 + dwell×0.3 + noise×0.3). Sensitivity tuned for demos (5 people = max, 15s dwell = max).
- **Idle mode:** 2 minutes no person → solid red LED (no server call). Resumes normal flow on next detection.
- **AI reasoning:** Gemini returns "reason" field explaining strategy choice. Logged on ESP32, stored in trials.json, visible at /api/status.
- **Graceful degradation:** Wi-Fi down or server unreachable → keep current animation, log error, never crash.
- **Serial log filtering:** ESP32 main loop prints distance every 100ms (10/sec). serial_reader filters these out of dashboard logs, only showing meaningful events (person detected, dwell, density, idle mode, calibration, errors). Raw logs available via separate buffer/endpoint.

## Critical: CMakeLists.txt SRCS

`main/CMakeLists.txt` must list every `.c` file in SRCS with `src/` prefix. When writing single-component test programs (e.g., only testing ultrasonic), remove unused modules from SRCS to avoid link errors. Always restore all 6 modules when reverting to the full program:

```cmake
idf_component_register(SRCS "src/main.c"
                            "src/ultrasonic.c"
                            "src/mic_adc.c"
                            "src/led_animation.c"
                            "src/wifi_manager.c"
                            "src/http_client.c"
                    INCLUDE_DIRS "header")
```

## Network Configuration

When changing Wi-Fi networks, update **two things** in `header/config.h`:
1. `WIFI_SSID` and `WIFI_PASS`
2. `SERVER_URL` — must point to the Flask server's IP on the same network

## Component Dependency

`main/idf_component.yml` declares `espressif/led_strip: "^3.0.0"` — auto-downloaded by IDF component manager on first build.

## Hardware Notes

- ESP32 pins don't make reliable contact in cheap breadboards. Use dişi-erkek jumper cables with ESP32 outside the breadboard if pins read incorrectly.
- MAX4466 trim pot adjusts gain (25×-125×). Turn clockwise for higher sensitivity. Target noise level 20-40 during normal speech.
- HC-SR04 GND must go to GND, not 5V (common wiring mistake).
