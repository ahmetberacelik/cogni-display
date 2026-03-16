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
//
//    SCAN_INTERVAL  : Her ölçüm arası bekleme (ms)
//    DETECT_THRESHOLD: Bu mesafeden yakınsa "kişi var"
//    LEFT_THRESHOLD  : Bu mesafeden uzaksa "kişi gitti"
//    (İkisi farklı çünkü sınırda sürekli var/yok titremesini önler)
//    LEFT_CONFIRM    : Kaç ardışık "uzak" okuma ile ayrılma onaylanır
//    MIN_DWELL_TIME  : Bundan kısa süreler yoksayılır
// ------------------------------------------------------------
#define ULTRASONIC_SCAN_INTERVAL_MS     100
#define PERSON_DETECT_THRESHOLD_CM      120
#define PERSON_LEFT_THRESHOLD_CM        150
#define PERSON_LEFT_CONFIRM_COUNT       5
#define MIN_DWELL_TIME_MS               1000

// ------------------------------------------------------------
// 6) Sabit Nesne Tespiti
//
//    Vitrinin önüne konan kutu/sandalye gibi sabit nesneleri
//    kişiden ayırt etmek için: varyans + zaman aşımı kontrolü.
//    Kişi kıpırdanır (varyans yüksek), nesne kıpırdanmaz (varyans düşük).
// ------------------------------------------------------------
#define STATIC_OBJECT_TIMEOUT_MS         60000   // 60sn hareketsizlik → sabit nesne
#define STATIC_OBJECT_VARIANCE_THRESHOLD 3.0f    // Standart sapma < 3cm → sabit
#define STATIC_OBJECT_SAMPLE_COUNT       20      // Varyans hesabı için ölçüm sayısı
#define STATIC_OBJECT_TOLERANCE_CM       10.0f   // Sabit nesne mesafesinden ±10cm tolerans
#define STATIC_OBJECT_MAX_COUNT          10      // Maksimum kaydedilebilir sabit nesne sayısı

// ------------------------------------------------------------
// 7) LED Ayarları (WS2812B NeoPixel Ring)
// ------------------------------------------------------------
#define NUM_LEDS                8
#define LED_ANIMATION_FPS       30
#define LED_TASK_STACK_SIZE     4096