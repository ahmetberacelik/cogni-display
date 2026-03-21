# CogniDisplay — Connection Schematic

> All sensor and module connections on the breadboard with the ESP32-S3 DevKitC.

---

## Overview

```
                        ┌─────────────────┐
                        │    ESP32-S3      │
                        │    DevKitC       │
                        │                  │
                  3.3V ─┤ 3V3         5V   ├─ 5V (from USB)
                   GND ─┤ GND        GND   ├─ GND
          (Trig)  GPIO4─┤ 4                ├
          (Echo)  GPIO5─┤ 5                ├
    (Microphone)  GPIO6─┤ 6                ├
     (LED Data)   GPIO7─┤ 7                ├
                        │                  │
                        └─────────────────┘
```

---

## Power Distribution

Prepare the breadboard power rails as follows:

| Rail        | Source                | Connected Components                          |
|-------------|----------------------|----------------------------------------------|
| **5V rail** | ESP32-S3 `5V` pin    | HC-SR04 VCC, Level Converter HV, NeoPixel 5V |
| **3.3V**    | ESP32-S3 `3V3` pin   | Level Converter LV, MAX4466 VCC              |
| **GND rail**| ESP32-S3 `GND` pin   | All components' GND (common ground)           |

> **Critical:** All components' GND must be connected to the same GND rail. Signals will not work properly without a common ground.

---

## 1. Level Converter (3.3V ↔ 5V) Connections

Think of the level converter as the **central** module — it acts as a bridge between the ESP32 side (LV/low voltage) and the sensor side (HV/high voltage).

### Power Pins

| Level Converter Pin   | Connection          |
|-----------------------|---------------------|
| **LV**                | 3.3V rail           |
| **HV**                | 5V rail             |
| **GND** (LV side)     | GND rail            |
| **GND** (HV side)     | GND rail            |

### Signal Channels

| Channel | LV Pin (3.3V side)       | HV Pin (5V side)              | Signal Direction   |
|---------|--------------------------|-------------------------------|--------------------|
| CH1     | LV1 ← ESP32 **GPIO4**   | HV1 → HC-SR04 **Trig**       | ESP32 → Sensor     |
| CH2     | LV2 ← ESP32 **GPIO5**   | HV2 ← HC-SR04 **Echo**       | Sensor → ESP32     |
| CH3     | LV3 ← ESP32 **GPIO7**   | HV3 → 330Ω → NeoPixel **DIN** | ESP32 → LED Ring |
| CH4     | (unused)                 | (unused)                      | —                  |

---

## 2. HC-SR04 Ultrasonic Sensor Connections

The HC-SR04 operates at 5V, its pins **must not be connected directly to the ESP32**. They go through the level converter.

| HC-SR04 Pin    | Connection                      |
|----------------|--------------------------------|
| **VCC**        | 5V rail                        |
| **Trig**       | Level Converter **HV1**        |
| **Echo**       | Level Converter **HV2**        |
| **GND**        | GND rail                       |

```
ESP32 GPIO4 ──► LV1 ═══ HV1 ──► HC-SR04 Trig
ESP32 GPIO5 ◄── LV2 ═══ HV2 ◄── HC-SR04 Echo
                    Level
                  Converter
```

---

## 3. MAX4466 Microphone Connections

The MAX4466 operates at 3.3V, **no level converter needed**, it connects directly to the ESP32.

| MAX4466 Pin  | Connection             |
|--------------|------------------------|
| **VCC**      | 3.3V rail              |
| **GND**      | GND rail               |
| **OUT**      | ESP32 **GPIO6**        |

```
MAX4466 OUT ──────► ESP32 GPIO6 (ADC1_CH5)
MAX4466 VCC ──────► 3.3V rail
MAX4466 GND ──────► GND rail
```

> **Note:** GPIO6 was specifically chosen because it is on the ADC1 channel. ADC2 does not work when Wi-Fi is enabled.

---

## 4. WS2812B NeoPixel Ring Connections

The NeoPixel operates at 5V, the data line goes through the level converter. A **330Ω resistor** is added to the data line (signal protection).

| NeoPixel Pin  | Connection                                 |
|---------------|--------------------------------------------|
| **5V**        | 5V rail                                    |
| **GND**       | GND rail                                   |
| **DIN**       | 330Ω resistor ← Level Converter **HV3**   |
| **DOUT**      | Not connected (unless daisy-chaining LEDs) |

```
ESP32 GPIO7 ──► LV3 ═══ HV3 ──► [330Ω] ──► NeoPixel DIN
                    Level
                  Converter
```

---

## 5. Capacitor (1000µF) Connection

Added to the 5V power line to smooth out the NeoPixel's sudden current draws.

| Capacitor Leg              | Connection |
|----------------------------|-----------|
| **Long leg (+) positive**  | 5V rail   |
| **Short leg (−) negative** | GND rail  |

> **Warning:** Reverse connection will permanently damage the capacitor. Long leg = positive.

Place the capacitor on the breadboard close to the NeoPixel's power connection.

---

## Summary Table of All Connections

| ESP32-S3 Pin   | Destination                       | Intermediate Component           |
|----------------|-----------------------------------|--------------------------|
| **3V3**        | Level Converter LV, MAX4466 VCC   | —                        |
| **5V**         | Level Converter HV, HC-SR04 VCC, NeoPixel 5V, Capacitor (+) | — |
| **GND**        | All components' GND, Capacitor (−) | —                       |
| **GPIO4**      | HC-SR04 Trig                      | Level Converter CH1      |
| **GPIO5**      | HC-SR04 Echo                      | Level Converter CH2      |
| **GPIO6**      | MAX4466 OUT                       | Direct (no converter)    |
| **GPIO7**      | NeoPixel DIN                      | Level Converter CH3 + 330Ω resistor |

---

## Breadboard Layout Suggestion

```
┌──────────────────────────────────────────────────────────┐
│  [5V Rail]  ════════════════════════════════════════════   │
│  [GND Rail] ════════════════════════════════════════════   │
│                                                          │
│  ┌───────────┐   ┌──────────┐   ┌────────┐   ┌───────┐  │
│  │ Level     │   │ HC-SR04  │   │MAX4466 │   │NeoPixel│ │
│  │ Converter │   │          │   │        │   │ Ring   │  │
│  │           │   │ VCC GND  │   │VCC GND │   │5V  GND│  │
│  │LV1 ── HV1├───┤ Trig     │   │OUT     │   │DIN    │  │
│  │LV2 ── HV2├───┤ Echo     │   │  │     │   │  ▲    │  │
│  │LV3 ── HV3├───┤          │   │  │     │   │  │    │  │
│  │LV  ── HV │   └──────────┘   │  │     │   │[330Ω]│  │
│  │GND ── GND│                   └──┼─────┘   │  │    │  │
│  └─┬──┬──┬──┘                      │         └──┼────┘  │
│    │  │  │                         │            │        │
│    │  │  └─── GPIO7 ───────────────┼────────────┘        │
│    │  └────── GPIO5                │                     │
│    └───────── GPIO4                │                     │
│                          GPIO6 ────┘                     │
│                                                          │
│          ┌────────────────┐                              │
│          │   ESP32-S3     │   [1000µF]                   │
│          │   DevKitC      │   (+)──5V rail               │
│          │ (outside the   │   (−)──GND rail              │
│          │  breadboard,   │                              │
│          │  connected via │                              │
│          │  M-F cables)   │                              │
│          └────────────────┘                              │
│                                                          │
│  [GND Rail] ════════════════════════════════════════════   │
│  [5V Rail]  ════════════════════════════════════════════   │
└──────────────────────────────────────────────────────────┘
```

> **Tip:** The ESP32-S3 pins may not make reliable contact in cheap breadboards. Keep the ESP32 outside the breadboard and connect to the breadboard using M-F (female-male) jumper cables.

---

## Checklist

After making the connections, go through this checklist:

- [ ] Are all GNDs connected to the same GND rail?
- [ ] Is the level converter's LV pin connected to 3.3V and HV pin connected to 5V?
- [ ] Do the HC-SR04's Trig and Echo pins go through the level converter?
- [ ] Is the MAX4466 connected directly to GPIO6 (no level converter)?
- [ ] Is there a 330Ω resistor on the line going to the NeoPixel DIN?
- [ ] Is the 1000µF capacitor connected between 5V-GND with correct polarity (long leg = +)?
- [ ] Is the ESP32 powered via USB (5V pin active)?

---

*Created: March 2025 | Project: CogniDisplay*
