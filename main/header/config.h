#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// ============================================================
// CogniDisplay - Merkezi Konfigürasyon Dosyası
// Tüm pin atamaları, sabitler ve ayarlar burada tanımlanır.
// ============================================================

// ------------------------------------------------------------
// 1) Wi-Fi Ayarları
// ------------------------------------------------------------
#define WIFI_SSID           "Mobilsel Paylaşım"
#define WIFI_PASS           "ab250cd18ef2"
#define WIFI_MAX_RETRIES    10

// ------------------------------------------------------------
// 2) Sunucu Ayarları
// ------------------------------------------------------------
#define SERVER_URL          "http://10.125.146.144:5000/api/trial"
#define HTTP_TIMEOUT_MS     10000

// ------------------------------------------------------------
// 3) GPIO Pin Atamaları
//
//    GPIO4 -> Level Converter CH1 -> HC-SR04 Trig
//    GPIO5 -> Level Converter CH2 -> HC-SR04 Echo
//    GPIO6 -> MAX4466 OUT (doğrudan, 3.3V uyumlu)
//    GPIO7 -> Level Converter CH3 -> 330ohm -> WS2812B DIN
// ------------------------------------------------------------
#define PIN_ULTRASONIC_TRIG GPIO_NUM_4
#define PIN_ULTRASONIC_ECHO GPIO_NUM_5
#define PIN_MIC_ADC         GPIO_NUM_6
#define PIN_LED_DATA        GPIO_NUM_7

// ------------------------------------------------------------
// 4) ADC Ayarları (Mikrofon)
//
//    GPIO6 = ADC1 Kanal 5 (ESP32-S3'te)
//    ADC1 kullanıyoruz çünkü ADC2, Wi-Fi açıkken çalışmaz.
// ------------------------------------------------------------
#define MIC_ADC_UNIT        ADC_UNIT_1
#define MIC_ADC_CHANNEL     ADC_CHANNEL_5
#define MIC_ADC_ATTEN       ADC_ATTEN_DB_12
#define MIC_SAMPLE_COUNT    256

// ------------------------------------------------------------
// 5) Ultrasonik Sensör Ayarları
// ------------------------------------------------------------
#define ULTRASONIC_SCAN_INTERVAL_MS     100     // Her ölçüm arası bekleme (ms)
#define PERSON_LEFT_CONFIRM_COUNT       5       // Kaç ardışık "uzak" okuma ile ayrılma onaylanır
#define MIN_DWELL_TIME_MS               1000    // Bundan kısa süreler yoksayılır

// ------------------------------------------------------------
// 6) Baseline Kalibrasyonu
//
//    Açılışta sensör ortamı ölçer ve "boş ortam mesafesi" (baseline) belirler.
//    Kişi tespiti baseline'a göre dinamik eşiklerle yapılır.
//    Ortam değişirse (nesne konulur/kaldırılır) baseline otomatik güncellenir.
// ------------------------------------------------------------
#define CALIBRATION_DURATION_MS         4000    // Açılış kalibrasyonu süresi
#define CALIBRATION_SAMPLE_INTERVAL_MS  100     // Kalibrasyon ölçüm aralığı
#define BASELINE_DETECT_RATIO           0.70f   // baseline * 0.70 altı → kişi var
#define BASELINE_DEPART_RATIO           0.85f   // baseline * 0.85 üstü → kişi gitti
#define BASELINE_UPDATE_TIMEOUT_MS      60000   // 60sn aynı mesafe → baseline güncelle
#define BASELINE_UPDATE_TOLERANCE_CM    5.0f    // ±5cm içinde "aynı mesafe" sayılır
#define BASELINE_DEFAULT_CM             200.0f  // Kalibrasyon başarısızsa varsayılan

// ------------------------------------------------------------
// 7) Yoğunluk Takibi (Density)
//
//    Son 2 dakikadaki kişi trafiğini takip eder.
//    Skor: (kisi_sayisi*0.4) + (ort_dwell*0.3) + (gurultu*0.3)
// ------------------------------------------------------------
#define DENSITY_WINDOW_MS               120000  // 2 dakikalık pencere
#define DENSITY_MAX_EVENTS              20      // Maksimum takip edilen olay
#define DENSITY_DWELL_MAX_MS            15000   // 15sn üstü dwell = 1.0 normalize
#define DENSITY_PERSON_MAX              5       // 5 kişi = 1.0 normalize

// ------------------------------------------------------------
// 8) Idle (Tenha) Modu
//
//    Kimse gelmezse LED kırmızı sabit yanar. Sunucuya istek gitmez.
// ------------------------------------------------------------
#define IDLE_TIMEOUT_MS                 120000  // 2 dakika kimse gelmezse idle

// ------------------------------------------------------------
// 9) LED Ayarları (WS2812B NeoPixel Ring)
// ------------------------------------------------------------
#define NUM_LEDS                8
#define LED_ANIMATION_FPS       30
#define LED_TASK_STACK_SIZE     4096