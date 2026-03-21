# ESP32-S3 Project — Hardware Technical Specifications Report

> This report covers the full technical specifications of all hardware components purchased for the ESP32-S3 based project.

---

## Table of Contents

1. [HC-SR04 Ultrasonic Distance Sensor](#1-hc-sr04-ultrasonic-distance-sensor)
2. [Logic Voltage Level Converter (3.3V – 5V)](#2-logic-voltage-level-converter-33v--5v)
3. [MAX4466 Electret Microphone Module](#3-max4466-electret-microphone-module)
4. [8-Bit NeoPixel Ring WS2812B 5050 RGB LED Module](#4-8-bit-neopixel-ring-ws2812b-5050-rgb-led-module)
5. [Breadboard (830 Holes)](#5-breadboard-830-holes)
6. [20 cm 40 Pin M-M Jumper Wire](#6-20-cm-40-pin-m-m-jumper-wire)
7. [30 cm 40 Pin M-F Jumper Wire](#7-30-cm-40-pin-m-f-jumper-wire)
8. [1/4W 330Ω Resistor Pack (10 Pieces)](#8-14w-330ω-resistor-pack-10-pieces)
9. [16V 1000µF Electrolytic Capacitor Pack (5 Pieces)](#9-16v-1000µf-electrolytic-capacitor-pack-5-pieces)

---

## 1. HC-SR04 Ultrasonic Distance Sensor

**Source:** [robotistan.com](https://www.robotistan.com/hc-sr04-ultrasonik-mesafe-sensoru)

### General Description
The HC-SR04 is a low-cost and widely used sensor that can measure distances between 2 cm and 400 cm using 40 kHz ultrasonic waves. It is commonly used in range finding, radar, and robotics applications.

> ⚠️ **ESP32-S3 Compatibility:** The HC-SR04 operates at 5V logic level. The ESP32-S3's I/O pins are 3.3V tolerant. **A Logic Level Converter must be used for the Echo pin.**

### Technical Specifications

| Parameter | Value |
|---|---|
| Operating Voltage | DC 5V |
| Current Draw | 15 mA |
| Operating Frequency | 40 kHz |
| Measurement Parameter | Distance |
| Minimum Detection Range | 2 cm |
| Maximum Detection Range | 4 m (400 cm) |
| Measurement Accuracy | 3 mm |
| Detection Angle | 15° |
| Trigger Input Signal | 10 µs TTL Pulse |
| Echo Output Signal | Input TTL signal proportional to distance |
| Dimensions | 45 × 20 × 15 mm |

### Pin Configuration

| Pin | Description |
|---|---|
| VCC | 5V Power Input |
| Trig | Trigger Signal (Digital Input) |
| Echo | Echo Signal (Digital Output) |
| GND | Ground |

### Operating Principle
1. A TTL high signal of at least 10 µs is applied to the `Trig` pin.
2. The sensor emits 8 ultrasonic pulses at 40 kHz.
3. When the sound wave bounces off an obstacle and returns, the `Echo` pin goes HIGH.
4. The distance is calculated by measuring the HIGH duration: `Distance (cm) = Duration (µs) / 58`

---

## 2. Logic Voltage Level Converter (3.3V – 5V)

**Source:** [hepsiburada.com](https://www.hepsiburada.com/robo-dunya-lojik-gerilim-seviye-donusturucu-3-3-v-5-v-logic-level-converter-pm-HBC00003Z3XND)
**Brand:** Robo Dünya

### General Description
A bi-directional logic level converter module. It converts 5V logic signals to 3.3V and 3.3V signals to 5V. It resolves signal level mismatches between the ESP32-S3 (3.3V) and 5V peripherals (such as the HC-SR04).

### Technical Specifications

| Parameter | Value |
|---|---|
| High Side Voltage (HV) | 5V |
| Low Side Voltage (LV) | 3.3V |
| Conversion Direction | Bi-Directional |
| Number of Channels | 4 Channels (typically BSS138 MOSFET based) |
| Operating Principle | MOSFET-based voltage divider |
| Warranty Period | 1 Month |

### Pin Configuration

| Pin | Description |
|---|---|
| HV | High voltage input (5V) |
| LV | Low voltage input (3.3V) |
| GND | Ground (common to both sides) |
| HV1–HV4 | 5V side signal pins |
| LV1–LV4 | 3.3V side signal pins |

### Usage Notes
- Connect 5V to the `HV` pin (HC-SR04 power side)
- Connect 3.3V to the `LV` pin (ESP32-S3 side)
- `GND` must be connected as a common ground between both circuits
- Signal wires are connected to the corresponding HVx/LVx pin pairs

---

## 3. MAX4466 Electret Microphone Module

**Source:** [hepsiburada.com](https://www.hepsiburada.com/741-elektronik-max4466-elektret-mikrofon-modulu-pm-HBC00002OH10K)
**Brand:** 741 Elektronik

### General Description
An electret microphone module equipped with the MAX4466 op-amp IC from Maxim Integrated. Thanks to its adjustable gain feature, it amplifies weak audio signals and provides an analog output. It is used in sound detection, sound level measurement, and audio recording applications.

### Technical Specifications (MAX4466 IC Based)

| Parameter | Value |
|---|---|
| Operating Voltage | 2.4V – 5.5V |
| Output Type | Analog (audio signal) |
| Gain Range | 25× – 125× (adjustable via trim pot) |
| Frequency Band | 20 Hz – 20 kHz |
| Mid-Point Voltage | VCC/2 (DC bias) |
| Supply Mode | Single Supply |
| Noise Characteristics | Low-noise microphone amplifier |
| Microphone Type | Electret condenser |
| Output Impedance | Low (~600Ω) |

### Pin Configuration

| Pin | Description |
|---|---|
| VCC | Power Input (2.4V – 5.5V; can be connected directly to ESP32-S3 with 3.3V) |
| GND | Ground |
| OUT | Analog audio output (connected to ESP32-S3 ADC pin) |

### Usage Notes
- Can be powered directly from the ESP32-S3's 3.3V pin
- The OUT pin is connected to an ADC pin on the ESP32-S3
- Gain can be adjusted using the trim pot on the board
- High gain → sensitive but noise-prone; low gain → cleaner signal

---

## 4. 8-Bit NeoPixel Ring WS2812B 5050 RGB LED Module

**Source:** [hepsiburada.com](https://www.hepsiburada.com/roboyol-store-8-bit-neopixel-halka-ring-ws2812b-5050-rgb-led-modul-5050-adreslenebilir-lamba-kart-full-color-ekran-pm-HBC00006WAURV)
**Brand:** Roboyol Store

### General Description
A ring module consisting of 8 individually addressable 5050 RGB LEDs with embedded WS2812B driver ICs. All LEDs can be individually controlled over a single data line. Multiple modules can be daisy-chained together.

> ⚠️ **ESP32-S3 Compatibility:** The WS2812B data line expects 5V logic level. While the ESP32-S3 (3.3V) may work under some conditions, using a Logic Level Converter is recommended for reliable communication. Adding a 100-470µF capacitor across the power line (between 5V and GND) is also recommended.

### Technical Specifications

| Parameter | Value |
|---|---|
| LED Count | 8 WS2812B 5050 RGB LEDs |
| Operating Voltage | DC 5V (4V – 7V range) |
| Current Per LED | 20 mA (full white, maximum brightness) |
| Total Maximum Current | ~160 mA (8 LEDs × 20 mA) |
| Color Depth | 24 bit (R:8 + G:8 + B:8) |
| Number of Colors | 16.7 Million (256³) |
| Brightness Levels | 256 steps (per channel) |
| Refresh Rate | 30 FPS (maximum chain of 1024 LEDs) |
| Maximum Daisy-Chain | 1024 LEDs (up to 5 meters between modules) |
| Outer Diameter | 32 mm |
| Inner Diameter | 18 mm |
| Communication Protocol | Single-wire serial (NeoPixel/WS2812 protocol) |
| Warranty Period | 6 Months |

### Pin Configuration

| Pin | Description |
|---|---|
| 5V | Power Input (5V) |
| GND | Ground |
| DIN | Data Input Pin (signal from ESP32-S3) |
| DOUT | Data Output Pin (connects to the next module) |

### Compatible Libraries
- **Arduino/ESP-IDF:** `FastLED`, `Adafruit NeoPixel`
- **MicroPython:** `neopixel` module

---

## 5. Breadboard (830 Holes)

**Source:** [robotistan.com](https://www.robotistan.com/breadboard)
**Brand:** Robotistan

### General Description
A standard-size breadboard used for solderless prototyping. Ideal for quickly testing connections between the ESP32-S3 and other modules.

### Technical Specifications

| Parameter | Value |
|---|---|
| Total Number of Holes | 830 |
| Power Rail | 2 power bus strips |
| Number of Rows | 60 rows |
| Number of Columns | 10 columns |
| Pin Spacing (Pitch) | 2.54 mm (standard) |
| Compatible Wire Gauge | 29 – 20 AWG |
| Color | White |
| Back Surface | Adhesive backing (can be mounted to a surface) |
| Dimensions | 165.5 × 54.5 × 8.5 mm |
| DIP IC Support | Yes (5-row split structure) |

---

## 6. 20 cm 40 Pin M-M Jumper Wire

**Source:** [robotistan.com](https://www.robotistan.com/20cm-40-pin-m-m-jumper-wires)
**Brand:** Robotistan

### General Description
A pre-made jumper wire set with male connectors on both ends, designed for breadboard and development board connections.

### Technical Specifications

| Parameter | Value |
|---|---|
| Length | 20 cm |
| Total Wire Count | 40 |
| Color Variety | 10 different colors |
| Wires Per Color | 4 |
| Connector Type | Male – Male (M-M) |
| Pin Spacing Compatibility | 2.54 mm (standard) |
| Conductor Gauge | 26 AWG |
| Usage Area | Breadboard, Arduino, Raspberry Pi, ESP32, etc. |

---

## 7. 30 cm 40 Pin M-F Jumper Wire

**Source:** [robotistan.com](https://www.robotistan.com/30cm-40-pin-m-f-jumper-wires)
**Brand:** Robotistan

### General Description
A jumper wire set with a male connector on one end and a female connector on the other. Suitable for connecting from GPIO header pins to breadboards or modules.

### Technical Specifications

| Parameter | Value |
|---|---|
| Length | 30 cm |
| Total Wire Count | 40 |
| Color Variety | 10 different colors |
| Wires Per Color | 4 |
| Connector Type | Male – Female (M-F) |
| Pin Spacing Compatibility | 2.54 mm (standard) |
| Conductor Gauge | 26 AWG |
| Usage Area | Breadboard, Arduino, Raspberry Pi, ESP32 header pins, etc. |

---

## 8. 1/4W 330Ω Resistor Pack (10 Pieces)

**Source:** [robotistan.com](https://www.robotistan.com/14w-330r-direnc-paketi-10-adet)
**Brand:** Robotistan

### General Description
A standard through-hole resistor with carbon film construction, value coded using color bands. Widely used in LED current limiting, pull-up/pull-down, and signal divider circuits.

### Technical Specifications

| Parameter | Value |
|---|---|
| Resistance Value | 330 Ω (Ohm) |
| Power Rating | 1/4 W (0.25 W) |
| Tolerance | ±5% (typically gold band) |
| Package Contents | 10 pieces |
| Mounting Type | Through-Hole |
| Color Code | Orange – Orange – Brown – Gold |
| Typical Usage | LED series resistor (5V → LED), signal divider |

### LED Calculation (with ESP32-S3)
- ESP32-S3 GPIO output: ~3.3V
- LED forward voltage (red): ~2.0V
- Current = (3.3V – 2.0V) / 330Ω ≈ **3.9 mA** ✅ (safe)
- A 300–500Ω series resistor is also recommended for the WS2812B data signal

---

## 9. 16V 1000µF Electrolytic Capacitor Pack (5 Pieces)

**Source:** [robotistan.com](https://www.robotistan.com/16v-1000uf-kondansator-paketi-5-adet)
**Brand:** Robotistan

### General Description
A polar electrolytic capacitor used in electronic circuits for power supply filtering, bypass, and energy storage purposes. It is strongly recommended to add one across the power line for loads that draw sudden current, such as WS2812B LED rings and motors.

### Technical Specifications

| Parameter | Value |
|---|---|
| Capacitance | 1000 µF |
| Maximum Voltage | 16V |
| Type | Electrolytic (Polar) |
| Package Contents | 5 pieces |
| Mounting Type | Through-Hole (radial) |
| Polarity | Positive (+) and Negative (−) |
| Typical Usage | Power line filtering, decoupling, inrush current buffering |

### Usage Notes
- Connect the capacitor between the 5V power line and GND on the circuit (should be placed close to the NeoPixel power line)
- The long leg is **positive (+)**, the short leg or the leg marked with a white stripe is **negative (−)**
- Reverse connection can permanently damage or burst the capacitor

---

## Inter-Component Connection Summary (ESP32-S3)

```
ESP32-S3 (3.3V system)
    │
    ├─► Logic Level Converter (LV=3.3V ↔ HV=5V)
    │       ├─ HC-SR04 (5V, Trig & Echo pins)
    │       └─ WS2812B Ring DIN (5V data)
    │
    ├─► MAX4466 Microphone (powered directly with 3.3V, OUT → ADC)
    │
    ├─► 330Ω Resistor → LED / WS2812B data protection
    │
    └─► 1000µF Capacitor (5V power line filter, placed near WS2812B)
```

---

## Important Notes

| # | Topic | Description |
|---|---|---|
| 1 | **Voltage Mismatch** | ESP32-S3 GPIOs are 3.3V; HC-SR04 Echo and WS2812B data line are 5V. A Logic Level Converter is mandatory. |
| 2 | **NeoPixel Current** | 8 LEDs at full white draw ~160 mA. Ensure the 5V power supply can handle this current. |
| 3 | **Capacitor Orientation** | Electrolytic capacitors are polar; reverse connection causes failure. |
| 4 | **MAX4466 Noise** | High gain increases noise. Adjust the trim pot according to project requirements. |
| 5 | **Breadboard Current Limit** | Breadboard power rails are limited to ~1A; use separate connections for high-current circuits. |

---

*Report date: January 2025 | Target platform: ESP32-S3*
