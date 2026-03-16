#include "ultrasonic.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "ULTRASONIC";

// Ölçüm zaman aşımı (mikrosaniye).
// Ses 4 metrede gidip dönerse ~23ms sürer. 25ms yeterli bir üst sınır.
#define TIMEOUT_US 25000

esp_err_t ultrasonic_init(void)
{
    // Trig pini: biz sinyal göndereceğiz → Output
    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << PIN_ULTRASONIC_TRIG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&trig_conf);

    // Echo pini: sensörden sinyal alacağız → Input
    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << PIN_ULTRASONIC_ECHO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&echo_conf);

    // Trig pinini LOW'da başlat
    gpio_set_level(PIN_ULTRASONIC_TRIG, 0);

    ESP_LOGI(TAG, "Ultrasonik sensör baslatildi (Trig: GPIO%d, Echo: GPIO%d)",
             PIN_ULTRASONIC_TRIG, PIN_ULTRASONIC_ECHO);
    return ESP_OK;
}

esp_err_t ultrasonic_measure_cm(float *distance_cm)
{
    // 1. Adım: Trig pinine 10 mikrosaniye HIGH sinyali gönder
    //    Bu sensöre "ölç!" komutu veriyor.
    gpio_set_level(PIN_ULTRASONIC_TRIG, 1);
    esp_rom_delay_us(10);
    gpio_set_level(PIN_ULTRASONIC_TRIG, 0);

    // 2. Adım: Echo pininin HIGH olmasını bekle
    //    Sensör ses dalgasını yayıyor, Echo henüz LOW.
    int64_t start_wait = esp_timer_get_time();
    while (gpio_get_level(PIN_ULTRASONIC_ECHO) == 0) {
        if ((esp_timer_get_time() - start_wait) > TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout: Echo HIGH olmadi");
            return ESP_FAIL;
        }
    }

    // 3. Adım: Echo HIGH oldu! Zamanı kaydet.
    //    Ses dalgası yola çıktı.
    int64_t echo_start = esp_timer_get_time();

    // 4. Adım: Echo pininin LOW olmasını bekle
    //    Ses dalgası engele çarpıp geri döndüğünde Echo LOW olacak.
    while (gpio_get_level(PIN_ULTRASONIC_ECHO) == 1) {
        if ((esp_timer_get_time() - echo_start) > TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout: Echo LOW olmadi (engel yok veya cok uzak)");
            return ESP_FAIL;
        }
    }

    // 5. Adım: Süreyi hesapla ve cm'ye çevir
    //    Ses gidip geldi. Toplam süreyi 58'e bölünce mesafe cm olarak çıkar.
    int64_t echo_end = esp_timer_get_time();
    int64_t duration_us = echo_end - echo_start;
    float distance = (float)duration_us / 58.0f;

    // Geçersiz aralık kontrolü (HC-SR04: 2cm - 400cm arası güvenilir)
    if (distance < 2.0f || distance > 400.0f) {
        ESP_LOGW(TAG, "Gecersiz mesafe: %.1f cm (aralik disi)", distance);
        return ESP_FAIL;
    }

    *distance_cm = distance;
    return ESP_OK;
}
