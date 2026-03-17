# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

CogniDisplay — an AI-powered physical display A/B testing system. ESP32-S3 measures person dwell time (HC-SR04 ultrasonic) and ambient noise (MAX4466 microphone), sends data to a local Flask server, which queries Google Gemini to decide the next LED animation strategy (WS2812B NeoPixel ring). Closed-loop: observe → analyze → act.

The user is an IoT student new to embedded systems. Explain concepts before writing code, get approval before implementing.

## Build & Flash (ESP-IDF 5.5.3)

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

Test with: `curl -X POST http://localhost:5000/api/trial -H "Content-Type: application/json" -d "{...}"`

Debug view: `http://localhost:5000/api/status`

## Architecture

**ESP32 Firmware (C, ESP-IDF):** 6 modules in `main/`, all configured via `config.h`:
- `wifi_manager` — Wi-Fi STA, auto-reconnect, EventGroup signaling
- `ultrasonic` — HC-SR04 driver (GPIO4 trig, GPIO5 echo via level converter)
- `mic_adc` — MAX4466 ADC1_CH5 on GPIO6, RMS noise calculation (0-100 scale)
- `led_animation` — WS2812B via `espressif/led_strip` component (RMT backend, GPIO7), 6 animation types, dedicated FreeRTOS task at 30 FPS, mutex-protected strategy
- `http_client` — POST trial data as JSON (cJSON), parse response for new strategy
- `main.c` — 3-state machine (IDLE → PERSON_DETECTED → PERSON_LEFT), 100ms scan cycle, static object detection via variance analysis

**Python Server:** `server/app.py` (Flask) + `server/gemini_client.py` (Gemini API). Model: `gemini-3.1-flash-lite-preview`. Trial history in `server/trials.json`. API key in `server/.env`.

## Key Design Decisions

- **GPIO4-7** chosen to avoid strapping/USB/JTAG/flash pins. GPIO6 specifically for ADC1 (ADC2 conflicts with Wi-Fi).
- **Level converter required** for HC-SR04 (5V echo → 3.3V) and NeoPixel (3.3V data → 5V). 4-channel bi-directional.
- **Hysteresis** in person detection: 120cm detect, 150cm depart, 5 consecutive readings to confirm departure.
- **Static object detection:** If distance variance < 3cm over 60s, marks as static. Supports up to 10 static objects stored in RAM (cleared on reboot).
- **Graceful degradation:** Wi-Fi down or server unreachable → keep current animation, log error, never crash.

## Critical: CMakeLists.txt SRCS

`main/CMakeLists.txt` must list every `.c` file in SRCS. When writing single-component test programs (e.g., only testing ultrasonic), remove unused modules from SRCS to avoid link errors. Always restore all 6 modules when reverting to the full program:

```cmake
idf_component_register(SRCS "main.c"
                            "ultrasonic.c"
                            "mic_adc.c"
                            "led_animation.c"
                            "wifi_manager.c"
                            "http_client.c"
                    INCLUDE_DIRS ".")
```

## Component Dependency

`main/idf_component.yml` declares `espressif/led_strip: "^3.0.0"` — auto-downloaded by IDF component manager on first build.

## Hardware Notes

- Wi-Fi SSID/password are in `config.h` — update when changing networks.
- ESP32 pins don't make reliable contact in cheap breadboards. Use dişi-erkek jumper cables with ESP32 outside the breadboard if pins read incorrectly.
- MAX4466 trim pot adjusts gain (25×–125×). Turn clockwise for higher sensitivity. Target noise level 20-40 during normal speech.
