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

// ============================================================
//  Density (Yoğunluk) Takibi — Son 2dk'daki olaylar
// ============================================================
typedef struct {
    int64_t timestamp_ms;      // Kişi ne zaman ayrıldı
    uint32_t dwell_time_ms;    // Ne kadar durdu
    float noise_level;         // O sıradaki ortalama gürültü
} density_event_t;

// Yoğunluk hesapla: son 2dk'daki olaylardan skor üret
static void calculate_density(const density_event_t *events, int event_count,
                              int64_t now_ms,
                              float *out_score, const char **out_category,
                              int *out_person_count, float *out_avg_dwell)
{
    int count = 0;
    float dwell_sum = 0;
    float noise_sum = 0;

    for (int i = 0; i < event_count; i++) {
        if ((now_ms - events[i].timestamp_ms) < DENSITY_WINDOW_MS) {
            count++;
            dwell_sum += events[i].dwell_time_ms;
            noise_sum += events[i].noise_level;
        }
    }

    *out_person_count = count;

    if (count == 0) {
        *out_score = 0;
        *out_category = "low";
        *out_avg_dwell = 0;
        return;
    }

    *out_avg_dwell = dwell_sum / count;

    // Normalize: kişi sayısı (max 10), dwell (max 30sn), gürültü (max 100)
    float person_norm = fminf((float)count / DENSITY_PERSON_MAX, 1.0f);
    float dwell_norm = fminf(*out_avg_dwell / DENSITY_DWELL_MAX_MS, 1.0f);
    float noise_norm = fminf((noise_sum / count) / 100.0f, 1.0f);

    float score = (person_norm * 0.4f) + (dwell_norm * 0.3f) + (noise_norm * 0.3f);
    score = fminf(score, 1.0f);

    *out_score = score;

    if (score <= 0.3f) {
        *out_category = "low";
    } else if (score <= 0.6f) {
        *out_category = "medium";
    } else {
        *out_category = "high";
    }
}

// ============================================================
//  Ana Program
// ============================================================
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

    // ---- 5. Başlangıç Kalibrasyonu ----
    ESP_LOGI(TAG, "Kalibrasyon basliyor (%d saniye)...", CALIBRATION_DURATION_MS / 1000);

    float calib_sum = 0;
    int calib_count = 0;
    int calib_samples = CALIBRATION_DURATION_MS / CALIBRATION_SAMPLE_INTERVAL_MS;

    for (int i = 0; i < calib_samples; i++) {
        float d = 0;
        if (ultrasonic_measure_cm(&d) == ESP_OK && d >= 2.0f && d <= 400.0f) {
            calib_sum += d;
            calib_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(CALIBRATION_SAMPLE_INTERVAL_MS));
    }

    float baseline_cm;
    if (calib_count > 0) {
        baseline_cm = calib_sum / calib_count;
    } else {
        baseline_cm = BASELINE_DEFAULT_CM;
        ESP_LOGW(TAG, "Kalibrasyon basarisiz, varsayilan baseline: %.1f cm", baseline_cm);
    }

    float detect_threshold = baseline_cm * BASELINE_DETECT_RATIO;
    float depart_threshold = baseline_cm * BASELINE_DEPART_RATIO;

    ESP_LOGI(TAG, "Kalibrasyon tamam! Baseline: %.1f cm, Tespit esigi: %.1f cm, Ayrilma esigi: %.1f cm",
             baseline_cm, detect_threshold, depart_threshold);

    // ---- 6. State machine değişkenleri ----
    app_state_t state = STATE_IDLE;
    int64_t detect_start_time = 0;
    float noise_sum = 0;
    int noise_count = 0;
    int consecutive_absent = 0;

    // Adaptif baseline takibi
    float baseline_candidate = 0;
    int64_t baseline_stable_since = 0;
    bool baseline_tracking = false;

    // Yoğunluk takibi (circular buffer)
    density_event_t density_events[DENSITY_MAX_EVENTS];
    memset(density_events, 0, sizeof(density_events));
    int density_event_count = 0;
    int density_event_index = 0;

    // Idle modu
    int64_t last_person_time_ms = esp_timer_get_time() / 1000;
    bool idle_mode_active = false;

    ESP_LOGI(TAG, "Sistem hazir. Tarama basliyor...");

    // ---- 7. Ana döngü ----
    while (1) {
        // Mesafe ölç
        float distance_cm = 999.0f;
        esp_err_t meas_ret = ultrasonic_measure_cm(&distance_cm);
        if (meas_ret != ESP_OK) {
            distance_cm = 999.0f;
        }

        int64_t now_ms = esp_timer_get_time() / 1000;

        ESP_LOGI(TAG, "[%s] Mesafe: %.1f cm (baseline: %.1f)",
                 (state == STATE_IDLE) ? "IDLE" :
                 (state == STATE_PERSON_DETECTED) ? "DETECTED" : "LEFT",
                 distance_cm, baseline_cm);

        switch (state) {

        // ---- IDLE: Kimse yok, tarama yapılıyor ----
        case STATE_IDLE:
            // --- Adaptif baseline güncelleme ---
            if (distance_cm < 400.0f) {
                if (!baseline_tracking) {
                    baseline_tracking = true;
                    baseline_candidate = distance_cm;
                    baseline_stable_since = now_ms;
                } else if (fabsf(distance_cm - baseline_candidate) < BASELINE_UPDATE_TOLERANCE_CM) {
                    // Hâlâ stabil — 60sn geçti mi?
                    if ((now_ms - baseline_stable_since) > BASELINE_UPDATE_TIMEOUT_MS) {
                        baseline_cm = baseline_candidate;
                        detect_threshold = baseline_cm * BASELINE_DETECT_RATIO;
                        depart_threshold = baseline_cm * BASELINE_DEPART_RATIO;
                        baseline_tracking = false;
                        ESP_LOGI(TAG, "Baseline guncellendi: %.1f cm (tespit: %.1f, ayrilma: %.1f)",
                                 baseline_cm, detect_threshold, depart_threshold);
                    }
                } else {
                    // Mesafe değişti, takibi sıfırla
                    baseline_candidate = distance_cm;
                    baseline_stable_since = now_ms;
                }
            }

            // --- Kişi tespiti (dinamik eşik) ---
            if (distance_cm < detect_threshold) {
                state = STATE_PERSON_DETECTED;
                detect_start_time = esp_timer_get_time();
                noise_sum = 0;
                noise_count = 0;
                consecutive_absent = 0;
                last_person_time_ms = now_ms;
                baseline_tracking = false;

                // Idle moddan çık
                if (idle_mode_active) {
                    idle_mode_active = false;
                    led_animation_set_strategy(&current_strategy);
                    ESP_LOGI(TAG, "Idle mod kapandi, normal moda donuluyor.");
                }

                ESP_LOGI(TAG, "Kisi tespit edildi! Mesafe: %.1f cm (esik: %.1f)",
                         distance_cm, detect_threshold);
            }

            // --- Idle timeout kontrolü ---
            if (!idle_mode_active && (now_ms - last_person_time_ms) > IDLE_TIMEOUT_MS) {
                idle_mode_active = true;
                led_strategy_t idle_strategy = {
                    .animation = ANIM_SOLID,
                    .r = 255, .g = 255, .b = 255,
                    .speed = 50,
                };
                led_animation_set_strategy(&idle_strategy);
                ESP_LOGW(TAG, "2 dakika boyunca kimse gelmedi. Idle mod: beyaz sabit.");
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

            // Kişi hâlâ burada mı? (dinamik eşik)
            if (distance_cm > depart_threshold) {
                consecutive_absent++;
                if (consecutive_absent >= PERSON_LEFT_CONFIRM_COUNT) {
                    state = STATE_PERSON_LEFT;
                    ESP_LOGI(TAG, "Kisi ayrildi.");
                }
            } else {
                consecutive_absent = 0;
            }
            break;

        // ---- PERSON_LEFT: Veri gönder, yeni strateji al ----
        case STATE_PERSON_LEFT:
            {
                // Dwell time hesapla
                int64_t now_us = esp_timer_get_time();
                uint32_t dwell_time_ms = (uint32_t)((now_us - detect_start_time) / 1000);

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

                // Olayı density buffer'a kaydet
                density_events[density_event_index] = (density_event_t){
                    .timestamp_ms = now_ms,
                    .dwell_time_ms = dwell_time_ms,
                    .noise_level = avg_noise,
                };
                density_event_index = (density_event_index + 1) % DENSITY_MAX_EVENTS;
                if (density_event_count < DENSITY_MAX_EVENTS) {
                    density_event_count++;
                }

                // Yoğunluk hesapla
                float density_score = 0;
                const char *density_category = "low";
                int person_count_2min = 0;
                float avg_dwell = 0;
                calculate_density(density_events, density_event_count, now_ms,
                                  &density_score, &density_category,
                                  &person_count_2min, &avg_dwell);

                ESP_LOGI(TAG, "Yogunluk: %.2f (%s), Son 2dk kisi: %d, Ort dwell: %.0f ms",
                         density_score, density_category, person_count_2min, avg_dwell);

                last_person_time_ms = now_ms;

                // Sunucuya gönder
                if (wifi_manager_is_connected()) {
                    trial_data_t trial = {
                        .dwell_time_ms = dwell_time_ms,
                        .noise_level = avg_noise,
                        .current_strategy = current_strategy,
                        .density_score = density_score,
                        .person_count_2min = person_count_2min,
                    };
                    strncpy(trial.density_category, density_category, sizeof(trial.density_category) - 1);
                    trial.density_category[sizeof(trial.density_category) - 1] = '\0';

                    led_strategy_t new_strategy = {0};
                    ret = http_client_send_trial(&trial, &new_strategy);

                    if (ret == ESP_OK) {
                        current_strategy = new_strategy;
                        led_animation_set_strategy(&current_strategy);
                        ESP_LOGI(TAG, "Yeni strateji uygulandi: %s",
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
