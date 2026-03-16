#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "http_client.h"
#include "ultrasonic.h"
#include "mic_adc.h"
#include "led_animation.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

// ============================================================
//  State Machine — Sistemin üç durumu
// ============================================================
typedef enum {
    STATE_IDLE,              // Kimse yok, tarama yapılıyor
    STATE_PERSON_DETECTED,   // Biri var, süre ve gürültü ölçülüyor
    STATE_PERSON_LEFT,       // Kişi gitti, veri gönderilecek
} app_state_t;

void app_main(void)
{
    ESP_LOGI(TAG, "CogniDisplay baslatiliyor...");

    // ---- 1. NVS başlat (Wi-Fi için gerekli) ----
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ---- 2. Wi-Fi başlat ve bağlan ----
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi baslatilamadi!");
        return;
    }

    ESP_LOGI(TAG, "Wi-Fi baglanti bekleniyor (30sn)...");
    ret = wifi_manager_wait_connected(pdMS_TO_TICKS(30000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi baglantisi basarisiz!");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi bagli.");

    // ---- 3. Sensörleri başlat ----
    ultrasonic_init();
    mic_adc_init();

    // ---- 4. LED animasyonu başlat ----
    led_animation_init();

    // Varsayılan strateji: gökkuşağı döngüsü
    led_strategy_t current_strategy = {
        .animation = ANIM_RAINBOW_CYCLE,
        .r = 0, .g = 0, .b = 255,
        .speed = 50,
    };
    led_animation_set_strategy(&current_strategy);

    // ---- 5. State machine değişkenleri ----
    app_state_t state = STATE_IDLE;
    int64_t detect_start_time = 0;     // Kişi ne zaman tespit edildi (µs)
    float noise_sum = 0;               // Toplam gürültü (ortalama için)
    int noise_count = 0;               // Kaç ölçüm yapıldı
    int consecutive_absent = 0;        // Ardışık "kişi yok" okuma sayısı

    // Sabit nesne tespiti değişkenleri
    float distance_buffer[STATIC_OBJECT_SAMPLE_COUNT];  // Son N mesafe ölçümü
    int buffer_index = 0;              // Dairesel buffer yazma pozisyonu
    int buffer_filled = 0;             // Buffer'da kaç geçerli ölçüm var
    float static_objects[STATIC_OBJECT_MAX_COUNT];  // Sabit nesnelerin mesafeleri
    int static_object_count = 0;                     // Kaç sabit nesne kayıtlı

    ESP_LOGI(TAG, "Sistem hazir. Tarama basliyor...");

    // ---- 6. Ana döngü ----
    while (1) {
        // Mesafe ölç
        float distance_cm = 999.0f;
        esp_err_t meas_ret = ultrasonic_measure_cm(&distance_cm);
        if (meas_ret != ESP_OK) {
            distance_cm = 999.0f;  // Hata → kişi yok say
        }

        switch (state) {

        // ---- IDLE: Kimse yok, tarama yapılıyor ----
        case STATE_IDLE:
            if (distance_cm < PERSON_DETECT_THRESHOLD_CM) {
                // Sabit nesne bölgesinde mi kontrol et
                bool is_static = false;
                for (int i = 0; i < static_object_count; i++) {
                    if (fabsf(distance_cm - static_objects[i]) < STATIC_OBJECT_TOLERANCE_CM) {
                        is_static = true;
                        break;
                    }
                }
                if (is_static) {
                    // Sabit nesne bölgesi — yoksay
                    break;
                }

                // Biri geldi!
                state = STATE_PERSON_DETECTED;
                detect_start_time = esp_timer_get_time();
                noise_sum = 0;
                noise_count = 0;
                consecutive_absent = 0;
                buffer_index = 0;
                buffer_filled = 0;
                ESP_LOGI(TAG, "Kisi tespit edildi! Mesafe: %.1f cm", distance_cm);
            }
            break;

        // ---- PERSON_DETECTED: Biri var, ölçüm yapılıyor ----
        case STATE_PERSON_DETECTED:
            // Gürültü ölçümü biriktir
            {
                float noise = 0;
                if (mic_adc_read_noise_level(&noise) == ESP_OK) {
                    noise_sum += noise;
                    noise_count++;
                }
            }

            // Mesafe ölçümünü dairesel buffer'a ekle (varyans hesabı için)
            if (distance_cm < 400.0f) {
                distance_buffer[buffer_index] = distance_cm;
                buffer_index = (buffer_index + 1) % STATIC_OBJECT_SAMPLE_COUNT;
                if (buffer_filled < STATIC_OBJECT_SAMPLE_COUNT) {
                    buffer_filled++;
                }
            }

            // Sabit nesne kontrolü: yeterli süre geçti mi?
            {
                int64_t elapsed_ms = (esp_timer_get_time() - detect_start_time) / 1000;
                if (elapsed_ms > STATIC_OBJECT_TIMEOUT_MS && buffer_filled == STATIC_OBJECT_SAMPLE_COUNT) {
                    // Standart sapma hesapla
                    float sum = 0;
                    for (int i = 0; i < STATIC_OBJECT_SAMPLE_COUNT; i++) {
                        sum += distance_buffer[i];
                    }
                    float mean = sum / STATIC_OBJECT_SAMPLE_COUNT;

                    float sum_sq = 0;
                    for (int i = 0; i < STATIC_OBJECT_SAMPLE_COUNT; i++) {
                        float diff = distance_buffer[i] - mean;
                        sum_sq += diff * diff;
                    }
                    float stddev = sqrtf(sum_sq / STATIC_OBJECT_SAMPLE_COUNT);

                    if (stddev < STATIC_OBJECT_VARIANCE_THRESHOLD) {
                        // Sabit nesne tespit edildi! Listeye ekle.
                        if (static_object_count < STATIC_OBJECT_MAX_COUNT) {
                            static_objects[static_object_count] = mean;
                            static_object_count++;
                            ESP_LOGW(TAG, "Sabit nesne #%d tespit edildi! Mesafe: %.1f cm (sapma: %.2f cm)",
                                     static_object_count, mean, stddev);
                        } else {
                            ESP_LOGW(TAG, "Sabit nesne listesi dolu (%d/%d), yeni nesne kaydedilemedi.",
                                     static_object_count, STATIC_OBJECT_MAX_COUNT);
                        }
                        state = STATE_IDLE;
                        break;
                    }
                }
            }

            // Kişi hâlâ burada mı?
            if (distance_cm > PERSON_LEFT_THRESHOLD_CM) {
                consecutive_absent++;
                if (consecutive_absent >= PERSON_LEFT_CONFIRM_COUNT) {
                    // 5 ardışık "uzak" okuma → kişi gerçekten gitti
                    state = STATE_PERSON_LEFT;
                    ESP_LOGI(TAG, "Kisi ayrildi.");
                }
            } else {
                consecutive_absent = 0;  // Hâlâ burada, sayacı sıfırla
            }
            break;

        // ---- PERSON_LEFT: Veri gönder, yeni strateji al ----
        case STATE_PERSON_LEFT:
            {
                // Dwell time hesapla
                int64_t now = esp_timer_get_time();
                uint32_t dwell_time_ms = (uint32_t)((now - detect_start_time) / 1000);

                // Ortalama gürültü
                float avg_noise = (noise_count > 0) ? (noise_sum / noise_count) : 0;

                ESP_LOGI(TAG, "Dwell time: %lu ms, Ortalama gurultu: %.1f",
                         (unsigned long)dwell_time_ms, avg_noise);

                // Çok kısa süreli tespitleri yoksay
                if (dwell_time_ms < MIN_DWELL_TIME_MS) {
                    ESP_LOGW(TAG, "Cok kisa sure (%lu ms), yoksayiliyor.",
                             (unsigned long)dwell_time_ms);
                    state = STATE_IDLE;
                    break;
                }

                // Sunucuya gönder
                if (wifi_manager_is_connected()) {
                    trial_data_t trial = {
                        .dwell_time_ms = dwell_time_ms,
                        .noise_level = avg_noise,
                        .current_strategy = current_strategy,
                    };

                    led_strategy_t new_strategy = {0};
                    ret = http_client_send_trial(&trial, &new_strategy);

                    if (ret == ESP_OK) {
                        current_strategy = new_strategy;
                        led_animation_set_strategy(&current_strategy);
                        ESP_LOGI(TAG, "Yeni strateji uygulandı: %s",
                                 led_animation_type_to_str(current_strategy.animation));
                    } else {
                        ESP_LOGW(TAG, "Sunucu hatasi, mevcut strateji korunuyor.");
                    }
                } else {
                    ESP_LOGW(TAG, "Wi-Fi bagli degil, veri gonderilemedi.");
                }

                state = STATE_IDLE;
            }
            break;
        }

        // 100ms bekle (saniyede 10 tarama)
        vTaskDelay(pdMS_TO_TICKS(ULTRASONIC_SCAN_INTERVAL_MS));
    }
}
