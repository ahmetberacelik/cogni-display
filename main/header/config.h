#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// ============================================================
// CogniDisplay - Central Configuration File
// All pin assignments, constants, and settings are defined here.
// ============================================================

// ------------------------------------------------------------
// 1) Wi-Fi Settings
// ------------------------------------------------------------
#define WIFI_SSID           "Mobilsel Paylaşım"
#define WIFI_PASS           "ab250cd18ef2"
#define WIFI_MAX_RETRIES    10

// ------------------------------------------------------------
// 2) Server Settings
// ------------------------------------------------------------
#define SERVER_URL          "http://10.125.146.144:5000/api/trial"
#define HTTP_TIMEOUT_MS     10000

// ------------------------------------------------------------
// 3) GPIO Pin Assignments
//
//    GPIO4 -> Level Converter CH1 -> HC-SR04 Trig
//    GPIO5 -> Level Converter CH2 -> HC-SR04 Echo
//    GPIO6 -> MAX4466 OUT (direct, 3.3V compatible)
//    GPIO7 -> Level Converter CH3 -> 330ohm -> WS2812B DIN
// ------------------------------------------------------------
#define PIN_ULTRASONIC_TRIG GPIO_NUM_4
#define PIN_ULTRASONIC_ECHO GPIO_NUM_5
#define PIN_MIC_ADC         GPIO_NUM_6
#define PIN_LED_DATA        GPIO_NUM_7

// ------------------------------------------------------------
// 4) ADC Settings (Microphone)
//
//    GPIO6 = ADC1 Channel 5 (on ESP32-S3)
//    We use ADC1 because ADC2 cannot be used while Wi-Fi is active.
// ------------------------------------------------------------
#define MIC_ADC_UNIT        ADC_UNIT_1
#define MIC_ADC_CHANNEL     ADC_CHANNEL_5
#define MIC_ADC_ATTEN       ADC_ATTEN_DB_12
#define MIC_SAMPLE_COUNT    256

// ------------------------------------------------------------
// 5) Ultrasonic Sensor Settings
// ------------------------------------------------------------
#define ULTRASONIC_SCAN_INTERVAL_MS     100     // Wait between measurements (ms)
#define PERSON_LEFT_CONFIRM_COUNT       5       // Consecutive "far" readings to confirm departure
#define MIN_DWELL_TIME_MS               1000    // Durations shorter than this are ignored

// ------------------------------------------------------------
// 6) Baseline Calibration
//
//    At startup, the sensor measures the environment and determines
//    the "empty environment distance" (baseline).
//    Person detection uses dynamic thresholds relative to the baseline.
//    If the environment changes (object placed/removed), baseline auto-updates.
// ------------------------------------------------------------
#define CALIBRATION_DURATION_MS         4000    // Startup calibration duration
#define CALIBRATION_SAMPLE_INTERVAL_MS  100     // Calibration sample interval
#define BASELINE_DETECT_RATIO           0.70f   // Below baseline * 0.70 → person present
#define BASELINE_DEPART_RATIO           0.85f   // Above baseline * 0.85 → person left
#define BASELINE_UPDATE_TIMEOUT_MS      60000   // 60s same distance → update baseline
#define BASELINE_UPDATE_TOLERANCE_CM    5.0f    // Within ±5cm counts as "same distance"
#define BASELINE_DEFAULT_CM             200.0f  // Default if calibration fails

// ------------------------------------------------------------
// 7) Density Tracking
//
//    Tracks person traffic within the last 2 minutes.
//    Score: (person_count*0.4) + (avg_dwell*0.3) + (noise*0.3)
// ------------------------------------------------------------
#define DENSITY_WINDOW_MS               120000  // 2-minute window
#define DENSITY_MAX_EVENTS              20      // Maximum tracked events
#define DENSITY_DWELL_MAX_MS            15000   // Dwell above 15s = 1.0 normalized
#define DENSITY_PERSON_MAX              5       // 5 people = 1.0 normalized

// ------------------------------------------------------------
// 8) Idle Mode
//
//    If no one comes, LED turns solid white. No requests sent to server.
// ------------------------------------------------------------
#define IDLE_TIMEOUT_MS                 120000  // 2 minutes with no one → idle

// ------------------------------------------------------------
// 9) LED Settings (WS2812B NeoPixel Ring)
// ------------------------------------------------------------
#define NUM_LEDS                8
#define LED_ANIMATION_FPS       30
#define LED_TASK_STACK_SIZE     4096
