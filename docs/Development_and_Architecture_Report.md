# CogniDisplay — Development and Architecture Report

> This document covers the complete technical architecture, design decisions, module structures, and implementation plan of the CogniDisplay project. It will be used as a reference guide during the code development process.

---

## Table of Contents

1. [Project Summary](#1-project-summary)
2. [System Architecture](#2-system-architecture)
3. [Hardware Connections](#3-hardware-connections)
4. [ESP32 Firmware Architecture](#4-esp32-firmware-architecture)
5. [Python Server Architecture](#5-python-server-architecture)
6. [Data Flow and JSON Schemas](#6-data-flow-and-json-schemas)
7. [LED Animation Types](#7-led-animation-types)
8. [Error Handling Strategy](#8-error-handling-strategy)
9. [Implementation Plan](#9-implementation-plan)
10. [Testing and Validation](#10-testing-and-validation)

---

## 1. Project Summary

**CogniDisplay** is an autonomous "Physical Interaction Agent" that brings the A/B testing logic from the software world into the physical world.

The system measures the **interaction time (dwell time)** of people passing in front of a storefront (or booth), detects ambient noise levels, and forwards this data to a Large Language Model (LLM — Google Gemini) to autonomously determine the **most attention-grabbing LED light color and animation**.

**Core Loop:**
1. A person approaches the storefront → sensors take measurements
2. The person leaves → data is sent to the server
3. Gemini AI determines a new strategy
4. The LED ring applies the new animation
5. The performance of the new strategy is measured with the next person

**Technology Stack:**
- **Microcontroller:** ESP32-S3 (ESP-IDF 5.5.3, C language)
- **Sensors:** HC-SR04 (distance), MAX4466 (sound)
- **Actuator:** WS2812B 8-Bit NeoPixel Ring (RGB LED)
- **Communication:** Wi-Fi, HTTP, JSON
- **Server:** Python Flask (local computer)
- **Artificial Intelligence:** Google Gemini API (gemini-3.1-flash-lite-preview)

---

## 2. System Architecture

The project is built on a modern 3-layer IoT architecture:

### 2.1 Device Layer

This is the layer that observes the physical world and takes action.

| Component | Role | Voltage |
|---|---|---|
| ESP32-S3 | Main controller, Wi-Fi, data processing | 3.3V logic |
| HC-SR04 | Person detection and dwell time measurement | 5V (level converter required) |
| MAX4466 | Ambient noise level measurement | 3.3V (direct connection) |
| WS2812B NeoPixel Ring | Physically applying the AI decision | 5V (level converter recommended) |
| Logic Level Converter | 3.3V ↔ 5V signal bridge | Bi-directional, 4 channels |
| 330Ω Resistor | WS2812B data line noise protection | — |
| 1000µF Capacitor | 5V power line filtering | 16V max |

### 2.2 Network Layer

Provided by the Wi-Fi module on the ESP32-S3. The device transmits sensor data to the upper layer via **HTTP POST** request in JSON format. The new strategy from the server is also received as JSON.

| Parameter | Value |
|---|---|
| Protocol | HTTP (plain, no TLS — local network) |
| Data Format | JSON |
| Direction | Bi-directional (POST send, response receive) |
| Wi-Fi Mode | STA (Station) |
| Wi-Fi Credentials | Hardcoded (in header file) |

### 2.3 Application Layer

Consists of a local Python Flask server running on the computer and the Google Gemini API.

| Component | Task |
|---|---|
| Flask Server | REST API, trial history management |
| Gemini API | AI that analyzes A/B test data and determines new strategy |
| trials.json | JSON file where all trials are stored |

---

## 3. Hardware Connections

### 3.1 GPIO Pin Assignments

| Function | GPIO | Direction | ADC Channel | Description |
|---|---|---|---|---|
| HC-SR04 Trig | GPIO4 | Digital Output | — | To 5V Trig pin via level converter CH1 |
| HC-SR04 Echo | GPIO5 | Digital Input | — | From 5V Echo pin via level converter CH2 |
| MAX4466 OUT | GPIO6 | Analog Input | ADC1_CH5 | Direct connection (3.3V compatible) |
| WS2812B DIN | GPIO7 | Digital Output (RMT) | — | Via level converter CH3 + 330Ω resistor |

**Pin Selection Rationale:**
- **ADC1 requirement:** On ESP32-S3, ADC2 cannot be used when Wi-Fi is active. One of the ADC1 channels (GPIO1-10) must be selected for the microphone. GPIO6 = ADC1_CH5.
- **Avoided pins:** GPIO0, 3, 45, 46 (strapping), GPIO19-20 (USB), GPIO26-32 (SPI flash/PSRAM), GPIO39-42 (JTAG).
- **Adjacent pin group:** Using GPIO4-7 provides tidy wiring on the breadboard.

### 3.2 Logic Level Converter Connections

| LLC Channel | LV Side (3.3V) | HV Side (5V) | Signal Direction | Purpose |
|---|---|---|---|---|
| Reference | ESP32 3.3V → LV pin | 5V supply → HV pin | — | Reference voltages |
| GND | Common GND | Common GND | — | Ground line |
| CH1 (LV1↔HV1) | GPIO4 | HC-SR04 Trig | 3.3V → 5V (Step-up) | Sending trigger signal |
| CH2 (LV2↔HV2) | GPIO5 | HC-SR04 Echo | 5V → 3.3V (Step-down) | Receiving echo signal |
| CH3 (LV3↔HV3) | GPIO7 | 330Ω → WS2812B DIN | 3.3V → 5V (Step-up) | LED data signal |
| CH4 | Empty | Empty | — | Spare channel |

### 3.3 Power Connections

| Component | Power Source | Description |
|---|---|---|
| ESP32-S3 | USB (5V) | Powered via USB, internal regulator produces 3.3V |
| HC-SR04 | ESP32 5V pin (VBUS) | Draws ~15mA |
| MAX4466 | ESP32 3.3V pin | Powered directly with 3.3V |
| WS2812B Ring | ESP32 5V pin (VBUS) | Max ~160mA (full white), typical 40-80mA |
| Level Converter | LV=3.3V, HV=5V | Both references connected |
| 1000µF Capacitor | Between 5V and GND | Close to NeoPixel power line, power ripple filter |

**Total Power Consumption:** ~250-400mA. A USB power supply (500mA+) is sufficient.

---

## 4. ESP32 Firmware Architecture

### 4.1 File Structure

```
main/
├── CMakeLists.txt          # Source file list (updated)
├── idf_component.yml       # led_strip dependency
├── config.h                # All pin, Wi-Fi, constant definitions
├── main.c                  # app_main(), state machine, main loop
├── wifi_manager.h          # Wi-Fi module interface
├── wifi_manager.c          # Wi-Fi STA connection, automatic reconnect
├── ultrasonic.h            # Ultrasonic sensor interface
├── ultrasonic.c            # HC-SR04 driver (trig pulse + echo measurement)
├── mic_adc.h               # Microphone ADC interface
├── mic_adc.c               # MAX4466 reading, RMS noise calculation
├── led_animation.h         # LED animation interface
├── led_animation.c         # 6 animation types + FreeRTOS task
├── http_client.h           # HTTP client interface
└── http_client.c           # Send JSON POST, parse response
```

**idf_component.yml** contents:
```yaml
dependencies:
  espressif/led_strip:
    version: "^3.0.0"
```

**CMakeLists.txt** (main/) updated version:
```cmake
idf_component_register(
    SRCS "main.c"
         "wifi_manager.c"
         "ultrasonic.c"
         "mic_adc.c"
         "led_animation.c"
         "http_client.c"
    INCLUDE_DIRS "."
)
```

### 4.2 FreeRTOS Task Structure

| Task | Priority | Stack | Core | Function |
|---|---|---|---|---|
| Main (app_main) | 5 | 3584 (default) | Any | State machine, sensor reading, HTTP requests |
| LED Animation | 4 | 4096 | Any | Continuous LED animation rendering (~30 FPS) |
| Wi-Fi (ESP-IDF system) | 23 | System default | Core 0 | Wi-Fi stack (pinned to Core 0 via sdkconfig) |

**Synchronization Mechanisms:**
- `led_strategy_t` structure: Protected by **Mutex** (SemaphoreHandle_t). Main task writes (rarely), LED task reads (30 times per second).
- Wi-Fi connection status: CONNECTED_BIT flag via **EventGroup**.

### 4.3 State Machine

The main logic of the system is managed by a 3-state state machine:

```
IDLE ──→ PERSON_DETECTED ──→ PERSON_LEFT ──→ (HTTP send) ──→ IDLE
  ↑                                                              │
  └──────────────────────────────────────────────────────────────┘
```

**State Transitions:**

| Current State | Condition | Next State | Action |
|---|---|---|---|
| IDLE | distance < 120cm | PERSON_DETECTED | Start timer, reset noise accumulator |
| PERSON_DETECTED | distance > 150cm (5 consecutive readings) | PERSON_LEFT | Calculate dwell time and average noise |
| PERSON_DETECTED | distance < 150cm | PERSON_DETECTED | Accumulate noise measurement, continue scanning |
| PERSON_LEFT | dwell_time < 1000ms | IDLE | Too short, ignore |
| PERSON_LEFT | dwell_time >= 1000ms | IDLE | Send HTTP POST, apply new strategy |

**Design Decisions:**
- **Hysteresis:** Detection threshold 120cm, departure threshold 150cm. This 30cm difference prevents continuous detect/lost cycles when a person stands at the boundary.
- **Confirmation Counter:** Person departure is confirmed by 5 consecutive readings (100ms × 5 = 500ms). Prevents false "departed" detections caused by momentary sensor noise.
- **Minimum Dwell Time:** Detections under 1000ms are ignored. Quickly passing people or sensor glitches are filtered out.
- **Graceful Degradation:** If Wi-Fi is down or the server is unreachable, the current animation continues to run.

### 4.4 Module Details

#### 4.4.1 config.h — Central Configuration

All constants are defined in this file:

| Category | Constant | Value | Description |
|---|---|---|---|
| Wi-Fi | `WIFI_SSID` | "ssid_here" | Network name |
| Wi-Fi | `WIFI_PASS` | "password_here" | Network password |
| Wi-Fi | `WIFI_MAX_RETRIES` | 10 | Maximum connection retry count |
| Server | `SERVER_URL` | "http://192.168.x.x:5000/api/trial" | Flask server address |
| Server | `HTTP_TIMEOUT_MS` | 10000 | HTTP request timeout |
| Pin | `PIN_ULTRASONIC_TRIG` | GPIO_NUM_4 | HC-SR04 trigger pin |
| Pin | `PIN_ULTRASONIC_ECHO` | GPIO_NUM_5 | HC-SR04 echo pin |
| Pin | `PIN_MIC_ADC` | GPIO_NUM_6 | Microphone analog input |
| Pin | `PIN_LED_DATA` | GPIO_NUM_7 | NeoPixel data pin |
| ADC | `MIC_ADC_UNIT` | ADC_UNIT_1 | ADC unit |
| ADC | `MIC_ADC_CHANNEL` | ADC_CHANNEL_5 | ADC1 channel for GPIO6 |
| ADC | `MIC_ADC_ATTEN` | ADC_ATTEN_DB_12 | Full range ~0-3.1V |
| ADC | `MIC_SAMPLE_COUNT` | 256 | Number of samples per measurement |
| Ultrasonic | `ULTRASONIC_SCAN_INTERVAL_MS` | 100 | Scan interval |
| Ultrasonic | `PERSON_DETECT_THRESHOLD_CM` | 120 | Detection threshold |
| Ultrasonic | `PERSON_LEFT_THRESHOLD_CM` | 150 | Departure threshold |
| Ultrasonic | `MIN_DWELL_TIME_MS` | 1000 | Minimum valid duration |
| Ultrasonic | `PERSON_LEFT_CONFIRM_COUNT` | 5 | Departure confirmation count |
| LED | `NUM_LEDS` | 8 | NeoPixel LED count |
| LED | `LED_ANIMATION_FPS` | 30 | Animation frame rate |
| LED | `LED_TASK_STACK_SIZE` | 4096 | LED task stack size |

#### 4.4.2 wifi_manager — Wi-Fi Connection Manager

**Interface:**

| Function | Return | Description |
|---|---|---|
| `wifi_manager_init()` | `esp_err_t` | Initialize NVS, netif, event loop, configure Wi-Fi STA and start connection |
| `wifi_manager_wait_connected(timeout)` | `esp_err_t` | Block until connected or timeout |
| `wifi_manager_is_connected()` | `bool` | Instant connection status |

**Operating Principle:**
- ESP-IDF Wi-Fi STA (Station) model is used.
- Automatic `esp_wifi_connect()` call on `WIFI_EVENT_STA_DISCONNECTED` event (with retry counter).
- On `IP_EVENT_STA_GOT_IP` event, `CONNECTED_BIT` is set in EventGroup and the IP address is logged.
- If maximum retries are exceeded, the device restarts with `esp_restart()`.

#### 4.4.3 ultrasonic — HC-SR04 Driver

**Interface:**

| Function | Return | Description |
|---|---|---|
| `ultrasonic_init()` | `esp_err_t` | Trig (output) and Echo (input) GPIO configuration |
| `ultrasonic_measure_cm(float *distance)` | `esp_err_t` | Take a single measurement, write result in cm |

**Operating Principle:**
1. A 10µs HIGH pulse is sent to the Trig pin (`esp_rom_delay_us(10)`)
2. Wait for Echo pin to go HIGH (timeout: 25ms)
3. Record HIGH start time (`esp_timer_get_time()`)
4. Wait for Echo pin to go LOW (timeout: 25ms)
5. Record HIGH end time
6. `distance_cm = duration_µs / 58.0`
7. Returns error code on timeout or uses 999.0cm sentinel value

**Measurement accuracy:** ±3mm (HC-SR04 specification). Software timing resolution: 1µs (`esp_timer_get_time`).

#### 4.4.4 mic_adc — MAX4466 Microphone ADC

**Interface:**

| Function | Return | Description |
|---|---|---|
| `mic_adc_init()` | `esp_err_t` | ADC1 Oneshot driver configuration, calibration |
| `mic_adc_read_noise_level(float *level)` | `esp_err_t` | Read noise level on 0-100 scale |

**Operating Principle:**
1. ADC1 is configured with CH5, 12-bit resolution, 12dB attenuation
2. ADC calibration scheme is created (`adc_cali_create_scheme_curve_fitting`)
3. 256 rapid ADC readings are taken
4. DC offset (mean) is calculated — MAX4466 output is centered around VCC/2 (~1.65V)
5. AC component is obtained by subtracting DC offset from each sample
6. RMS (Root Mean Square) is calculated: `sqrt(sum((sample - mean)²) / N)`
7. RMS value is mapped to 0-100 scale (0 = quiet, 100 = very noisy)

**Calibration note:** The scaling factor (`max_rms` value) should be adjusted in practice according to the environment. Additionally, the gain can be physically adjusted via the trim pot on the MAX4466.

#### 4.4.5 led_animation — LED Animation Engine

**Data Structures:**

```c
typedef enum {
    ANIM_SOLID,          // Solid color
    ANIM_BREATHING,      // Breathing (fade in/out)
    ANIM_RAINBOW_CYCLE,  // Rainbow cycle
    ANIM_BLINK,          // Blinking
    ANIM_WAVE,           // Wave effect
    ANIM_COLOR_WIPE,     // Sequential fill
    ANIM_COUNT           // Total animation count
} animation_type_t;

typedef struct {
    animation_type_t animation;
    uint8_t r, g, b;     // RGB color values
    uint8_t speed;       // 1-100 (animation speed)
} led_strategy_t;
```

**Interface:**

| Function | Description |
|---|---|
| `led_animation_init()` | led_strip init + start FreeRTOS animation task |
| `led_animation_set_strategy(strategy)` | Assign new strategy (thread-safe with mutex) |
| `led_animation_get_strategy()` | Read current strategy (thread-safe with mutex) |
| `led_animation_type_to_str(type)` | Enum → string conversion ("breathing" etc.) |
| `led_animation_str_to_type(str)` | String → enum conversion |

**LED Strip Configuration:**
- Component: `espressif/led_strip` v3.0.0+ (IDF Component Registry)
- Backend: RMT (Remote Control Transceiver) peripheral
- LED model: WS2812 (GRB color order)
- RMT resolution: 10 MHz
- DMA: Disabled (unnecessary for 8 LEDs)

**Animation Task:**
- Runs in an infinite loop at ~30 FPS (33ms/frame)
- Reads the current `led_strategy_t` each frame (with mutex)
- Calls the corresponding animation function
- Internal counters are reset when the strategy changes (seamless transition)

**Helper Function:** `hsv_to_rgb()` — HSV color space to RGB conversion (required for rainbow animation).

#### 4.4.6 http_client — HTTP Client

**Data Structure:**

```c
typedef struct {
    uint32_t dwell_time_ms;      // Person's dwell time (ms)
    float noise_level;           // Average noise level (0-100)
    led_strategy_t current_strategy;  // Active strategy during measurement
} trial_data_t;
```

**Interface:**

| Function | Description |
|---|---|
| `http_client_send_trial(trial, new_strategy)` | POST trial data, receive new strategy |

**Operating Principle:**
- ESP-IDF `esp_http_client` API is used
- JSON creation/parsing: `cJSON` library built into ESP-IDF (no additional dependency required)
- POST body: trial data in JSON format
- Response: new strategy in JSON format
- On error (timeout, HTTP 4xx/5xx, invalid JSON) returns error code, caller preserves current strategy

### 4.5 Startup Sequence (app_main)

Startup steps in the `app_main()` function:

| Order | Operation | Description |
|---|---|---|
| 1 | `nvs_flash_init()` | Initialize Non-Volatile Storage (required for Wi-Fi) |
| 2 | `wifi_manager_init()` | Start Wi-Fi STA |
| 3 | `wifi_manager_wait_connected(30s)` | Wait for connection |
| 4 | `ultrasonic_init()` | Configure HC-SR04 GPIO |
| 5 | `mic_adc_init()` | Configure and calibrate ADC1 |
| 6 | `led_animation_init()` | Start LED strip + animation task |
| 7 | Assign default strategy | `ANIM_RAINBOW_CYCLE, r=0, g=0, b=255, speed=50` |
| 8 | Enter main loop | State machine, at 100ms intervals |

---

## 5. Python Server Architecture

### 5.1 File Structure

```
server/
├── app.py              # Flask application, REST endpoints
├── gemini_client.py    # Gemini API integration
├── requirements.txt    # Python dependencies
├── .env                # API key (not added to git)
└── trials.json         # Created at runtime
```

**requirements.txt:**
```
flask>=3.0
google-generativeai>=0.8
python-dotenv>=1.0
```

### 5.2 Flask Endpoints

| Method | URL | Description |
|---|---|---|
| POST | `/api/trial` | Receive new trial data, query Gemini, return new strategy |
| GET | `/api/status` | Last 10 trials + current strategy (for debugging) |

**POST /api/trial flow:**
1. Extract `dwell_time_ms`, `noise_level`, `current_strategy` from JSON body
2. Add as a new trial to trials.json (trial_number auto-incrementing)
3. Send trial history to Gemini API
4. Validate the new strategy JSON from Gemini (valid animation name, RGB 0-255, speed 1-100)
5. Return the validated strategy to ESP32

**Error case:** Gemini API error → a random strategy is generated and returned (fallback).

### 5.3 Gemini Integration

- **Model:** `gemini-3.1-flash-lite-preview`
- **Role:** "Marketing and Visual Attention Expert" — analyzes A/B test results and determines a new LED strategy to maximize dwell time
- **Context:** Last 20 trial history (for token savings), best and worst trials are highlighted
- **Strategy logic:**
  - < 5 trials: Exploration mode (try various animations)
  - >= 5 trials: Exploitation mode (repeat well-performing strategies with small variations)
- **Output:** JSON-only response requirement, server-side validation and clamping applied

### 5.4 Trial History Management

- **Storage:** `trials.json` file (simple, readable)
- **Format:** JSON array, each element is a trial record
- **Loading:** Server reads from file on each request
- **Saving:** Writes to file after each new trial (indent=2, readable format)

---

## 6. Data Flow and JSON Schemas

### 6.1 ESP32 → Server (POST /api/trial body)

```json
{
  "dwell_time_ms": 4500,
  "noise_level": 42.3,
  "current_strategy": {
    "animation": "breathing",
    "r": 0,
    "g": 0,
    "b": 255,
    "speed": 50
  }
}
```

| Field | Type | Description |
|---|---|---|
| `dwell_time_ms` | integer | Duration the person stood in front of the display (milliseconds) |
| `noise_level` | float | Average ambient noise level (0.0 - 100.0) |
| `current_strategy.animation` | string | Animation type active during measurement |
| `current_strategy.r/g/b` | integer | Active RGB color values (0-255) |
| `current_strategy.speed` | integer | Active animation speed (1-100) |

### 6.2 Server → ESP32 (Response body)

```json
{
  "animation": "rainbow_cycle",
  "r": 255,
  "g": 100,
  "b": 0,
  "speed": 75
}
```

| Field | Type | Description |
|---|---|---|
| `animation` | string | New animation type to apply |
| `r/g/b` | integer | New RGB color values (0-255) |
| `speed` | integer | New animation speed (1-100) |

### 6.3 trials.json (Server Disk Structure)

```json
[
  {
    "trial_number": 1,
    "dwell_time_ms": 4500,
    "noise_level": 42.3,
    "strategy": {
      "animation": "breathing",
      "r": 0,
      "g": 0,
      "b": 255,
      "speed": 50
    }
  }
]
```

---

## 7. LED Animation Types

| # | Animation | Description | RGB Usage | Speed Effect |
|---|---|---|---|---|
| 1 | `solid` | All 8 LEDs light up in the same color steadily | Direct color | No effect (static) |
| 2 | `breathing` | All LEDs sinusoidal brightness variation (fade in/out) | Base color | Breath cycle duration (fast=short cycle) |
| 3 | `rainbow_cycle` | Each LED offset by 45° in HSV color space, hue rotates | Ignored | Rotation speed |
| 4 | `blink` | All LEDs toggle on/off | Color when on | Blinking frequency |
| 5 | `wave` | Color progresses through the ring with a "traveling wave" effect | Wave color | Wave progression speed |
| 6 | `color_wipe` | LEDs turn on one by one sequentially, then turn off sequentially | Fill color | Fill speed |

**Speed parameter mapping:**
- speed=1 → Slowest (e.g.: breathing cycle ~5 seconds)
- speed=100 → Fastest (e.g.: breathing cycle ~0.5 seconds)
- Intermediate values are calculated via linear interpolation

---

## 8. Error Handling Strategy

### 8.1 ESP32 Firmware Errors

| Error Condition | Detection | Solution |
|---|---|---|
| Wi-Fi connection error | Event handler retry counter | 10 attempts, then `esp_restart()` |
| Wi-Fi disconnection during operation | `WIFI_EVENT_STA_DISCONNECTED` | Automatic reconnect in background |
| Server unreachable / timeout | `esp_http_client` error code | Log error, keep current animation |
| Server returns invalid JSON | cJSON parse error | Log error, keep current animation |
| Server HTTP 4xx/5xx | Status code check | Log error, keep current animation |
| Ultrasonic timeout (no echo) | Measurement function returns error/sentinel | Treat as 999cm (no person) |
| Ultrasonic invalid distance | < 2cm or > 400cm | Discard reading, use previous valid reading |
| ADC read error | `adc_oneshot_read` error code | Skip sample, do not increment counter |
| LED strip init error | `led_strip_new_rmt_device` error | Fatal — halt with `ESP_ERROR_CHECK` (hardware error) |

### 8.2 Python Server Errors

| Error Condition | Solution |
|---|---|
| Gemini API error / timeout | Generate random strategy (fallback) |
| Gemini returns invalid JSON | Retry once, fallback if unsuccessful |
| trials.json unreadable | Start with empty list |
| Invalid POST body | Return HTTP 400 + error message |

### 8.3 General Principle

**"Never stop, always run."** Even if the server is unreachable, ESP32 continues to display its current animation. No error condition causes the system to completely halt (except LED strip init — this is a hardware connection error).

---

## 9. Implementation Plan

Development will follow an incremental approach that produces testable outputs at each phase.

### Phase 1: Basic Hardware (Testing with Serial Monitor)

| Step | Module | Test Method |
|---|---|---|
| 1 | Create `config.h` | Successful compilation |
| 2 | `led_animation` module + `idf_component.yml` | Flash → NeoPixel ring animation display |
| 3 | `ultrasonic` module | Flash → Distance readings in serial monitor |
| 4 | `mic_adc` module | Flash → Noise levels in serial monitor |

### Phase 2: State Machine (ESP32 Standalone)

| Step | Module | Test Method |
|---|---|---|
| 5 | `main.c` state machine (serial log instead of HTTP) | Approach-retreat with hand movement, verify correct state transitions and dwell_time in serial output |

### Phase 3: Connectivity

| Step | Module | Test Method |
|---|---|---|
| 6 | `wifi_manager` | Flash → IP address in serial log |
| 7 | `http_client` | Trigger trial → POST reaches server |

### Phase 4: Python Server

| Step | Module | Test Method |
|---|---|---|
| 8 | Flask server (without Gemini, random fallback) | POST test with `curl`, trials.json creation |
| 9 | Gemini integration | POST with `curl` → Gemini strategy returned |

### Phase 5: Integration and Calibration

| Step | Task | Test Method |
|---|---|---|
| 10 | End-to-end test | 10-20 trials, LED changes + trials.json verification |
| 11 | Calibration | Threshold values, microphone gain, Gemini prompt tuning |

---

## 10. Testing and Validation

### 10.1 Module Tests

| Module | Validation Criteria |
|---|---|
| LED Animation | All 6 animations are visually verified; strategy changes reflect instantly |
| Ultrasonic | ±5cm accuracy at known distances (30cm, 100cm) |
| Microphone | Quiet environment < 15, speech ~30-50, clapping/shouting > 70 |
| Wi-Fi | IP address is obtained, reconnects when router is turned off and on |
| HTTP Client | POST data visible in server logs, response is parsed |

### 10.2 Integration Test

| Scenario | Expected Behavior |
|---|---|
| Person approaches, stays 5s, walks away | Dwell time ~5000ms is logged, sent to server, new animation is applied |
| Person passes quickly (< 1s) | Ignored, not sent to server |
| Server is shut down | ESP32 continues running with current animation, error is logged |
| Wi-Fi is disconnected | Automatic reconnect is attempted, trial is not sent while disconnected |
| 10 consecutive trials | 10 records in trials.json, Gemini strategies become increasingly informed |

### 10.3 Monitoring with `/api/status`

During development, by opening `http://localhost:5000/api/status` in a browser:
- Total trial count
- Last 10 trial details
- Current active strategy

can be viewed.

---

*Document date: March 2025 | Project: CogniDisplay | Platform: ESP32-S3, ESP-IDF 5.5.3*
