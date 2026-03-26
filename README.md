# CogniDisplay

An AI-powered physical display A/B testing system. ESP32-S3 measures person dwell time (HC-SR04 ultrasonic) and ambient noise (MAX4466 microphone), sends data to a local Flask server, which queries Google Gemini to decide the next LED animation strategy (WS2812B NeoPixel ring).

**Closed-loop:** Observe → Analyze → Act

## System Architecture

```
┌─────────────┐         HTTP POST         ┌──────────────┐       API        ┌────────────┐
│   ESP32-S3  │ ──────────────────────►   │ Flask Server │ ──────────────► │  Gemini AI │
│             │ ◄──────────────────────   │  (Python)    │ ◄────────────── │            │
│  Sensors    │      New Strategy         │              │    Strategy     │  Decision  │
│  + LED Ring │                           │  Dashboard   │                 │  Engine    │
└─────────────┘                           └──────────────┘                 └────────────┘
```

## Hardware Requirements

| Component | Description | Connection |
|-----------|-------------|------------|
| **ESP32-S3 DevKit** | Main microcontroller | USB-C to computer |
| **HC-SR04** | Ultrasonic distance sensor (5V) | GPIO4 (Trig), GPIO5 (Echo) |
| **MAX4466** | Electret microphone module | GPIO6 (ADC1_CH5) |
| **WS2812B** | NeoPixel LED Ring (16 LEDs) | GPIO7 |
| **Level Converter** | 4-channel, bi-directional (3.3V ↔ 5V) | HC-SR04 Echo + NeoPixel DIN |

### Pin Assignment

```
GPIO4  →  Level Converter CH1  →  HC-SR04 Trig
GPIO5  →  Level Converter CH2  →  HC-SR04 Echo
GPIO6  →  MAX4466 OUT (direct, 3.3V compatible)
GPIO7  →  Level Converter CH3  →  330Ω  →  WS2812B DIN
```

> **Note:** GPIO4-7 were chosen to avoid strapping/USB/JTAG/flash pins. GPIO6 is specifically selected for ADC1 (ADC2 conflicts with Wi-Fi).

## Software Requirements

### ESP32 Firmware
- **ESP-IDF v5.5.3**
- `espressif/led_strip` component (v3.0+, auto-downloaded on first build)

### Python Server
- Python 3.10+
- Dependencies: Flask, google-generativeai, python-dotenv, pyserial

## Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/<username>/cogni-display.git
cd cogni-display
```

### 2. Configure ESP32 Firmware

Update Wi-Fi and server settings in `main/header/config.h`:

```c
#define WIFI_SSID    "Your_WiFi_Name"
#define WIFI_PASS    "Your_WiFi_Password"
#define SERVER_URL   "http://<YOUR_PC_IP>:5000/api/trial"
```

### 3. Build and Flash Firmware

```bash
idf.py build
idf.py flash -p COM6
```

### 4. Set Up Python Server

```bash
cd server
pip install -r requirements.txt
```

Create `server/.env`:

```
GEMINI_API_KEY=your_api_key_here
```

Start the server:

```bash
python app.py
```

Dashboard: `http://localhost:5000/`

> **Important:** The Flask server uses COM6 serial port in a background thread. Do not run `idf.py monitor` while the server is running — two programs cannot use the same port simultaneously. You can view serial output through the dashboard.

## How It Works

### State Machine (ESP32)

```
        Distance < Threshold           Dwell time recorded
  IDLE ──────────────────► PERSON_DETECTED ──────────────────► PERSON_LEFT
   ▲                                                               │
   │                          Data sent to server                  │
   └───────────────────────────────────────────────────────────────┘
```

1. **IDLE:** Measures distance every 100ms. After 2 minutes of no activity, a solid red LED turns on.
2. **PERSON_DETECTED:** Person detected — dwell time is being measured, noise level is being recorded.
3. **PERSON_LEFT:** Person departed. Sensor data (dwell time, noise, density) is POSTed to the server.
4. The server queries Gemini AI for a new strategy and returns it to ESP32.
5. ESP32 applies the new LED animation.

### Animation Types

| Animation | Description |
|-----------|-------------|
| `rainbow` | Eye-catching rainbow cycles |
| `breathing` | Smooth pulsing glow effect |
| `color_wipe` | Sequential color fill |
| `theater_chase` | Theater marquee chase lights |
| `wave` | Wave-like color transition |
| `sparkle` | Random sparkling effect |

### Density Tracking

Composite score calculated over a 2-minute sliding window:

- Person count: 40%
- Average dwell time: 30%
- Average noise level: 30%

Categories: `low` / `medium` / `high`

## API Reference

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/trial` | POST | ESP32 sends sensor data, receives new strategy |
| `/api/status` | GET | Trial history, statistics, current strategy |
| `/api/serial` | GET | Filtered ESP32 serial logs + live state |
| `/api/serial/raw` | GET | All serial lines (including distance measurements) |
| `/api/logs` | GET | Structured server event log |

### Example Data Flow

**ESP32 → Server:**
```json
{
  "dwell_time_ms": 4500,
  "noise_level": 42.3,
  "current_strategy": {"animation": "breathing", "r": 0, "g": 0, "b": 255, "speed": 50},
  "density_score": 0.45,
  "density_category": "medium",
  "person_count_2min": 3
}
```

**Server → ESP32:**
```json
{
  "animation": "wave",
  "r": 255,
  "g": 100,
  "b": 0,
  "speed": 75,
  "reason": "High density with moderate noise suggests an energetic warm-toned animation..."
}
```

## Project Structure

```
cogni-display/
├── main/
│   ├── CMakeLists.txt              # Build configuration
│   ├── idf_component.yml           # led_strip dependency
│   ├── header/
│   │   ├── config.h                # Central configuration
│   │   ├── wifi_manager.h
│   │   ├── ultrasonic.h
│   │   ├── mic_adc.h
│   │   ├── led_animation.h
│   │   └── http_client.h
│   └── src/
│       ├── main.c                  # State machine + calibration
│       ├── wifi_manager.c          # Wi-Fi connection management
│       ├── ultrasonic.c            # HC-SR04 driver
│       ├── mic_adc.c               # MAX4466 ADC reading
│       ├── led_animation.c         # WS2812B animations
│       └── http_client.c           # HTTP client
├── server/
│   ├── app.py                      # Flask server
│   ├── gemini_client.py            # Gemini AI integration
│   ├── serial_reader.py            # Serial port reader
│   ├── requirements.txt            # Python dependencies
│   ├── templates/
│   │   └── dashboard.html          # Web dashboard
│   └── .env                        # API key
├── docs/                           # Project documentation
├── CLAUDE.md
└── README.md
```

## Hardware Notes

- ESP32 pins may not make reliable contact in cheap breadboards. If pins read incorrectly, keep the ESP32 outside the breadboard and use female-male (F-M) jumper cables.
- The MAX4466 trim pot adjusts gain (25x–125x). Turn clockwise for higher sensitivity. Target noise level for normal speech: 20–40.
- HC-SR04 GND wire must go to GND, not 5V (common wiring mistake).
- The level converter is required for both HC-SR04 (5V echo → 3.3V) and NeoPixel (3.3V data → 5V).

## Design Decisions

- **Adaptive baseline:** 4-second startup calibration, dynamic thresholds (baseline × 0.70 detect, baseline × 0.85 depart), auto-updates after 60 seconds of stable readings.
- **Idle mode:** 2 minutes with no person detected → solid red LED (no server request). Resumes normal flow on next detection.
- **Graceful degradation:** If Wi-Fi or server is unreachable, the current animation continues, the error is logged, and the system never crashes.
- **Serial log filtering:** The dashboard shows only meaningful events (person detection, dwell, density, errors). Raw logs are available via a separate endpoint.

## Tech Stack

- **Firmware:** C, ESP-IDF 5.5.3, FreeRTOS
- **Server:** Python, Flask, Google Gemini AI
- **Frontend:** HTML/CSS/JS, Chart.js
- **Communication:** HTTP/JSON, UART (serial)

## License

This project was developed for educational purposes.
