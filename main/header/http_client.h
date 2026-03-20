#pragma once

#include "esp_err.h"
#include "led_animation.h"
#include <stdint.h>

// Trial verisi — sensörlerin topladığı bilgi + o anki strateji + yoğunluk.
// Bu paket sunucuya gönderilir.
typedef struct {
    uint32_t dwell_time_ms;            // Kişi kaç ms durdu
    float noise_level;                 // Ortam gürültüsü (0-100)
    led_strategy_t current_strategy;   // O sırada çalan animasyon
    float density_score;               // Yoğunluk skoru (0.0 - 1.0)
    char density_category[8];          // "low", "medium", "high"
    int person_count_2min;             // Son 2dk'daki kişi sayısı
} trial_data_t;

// Trial verisini sunucuya gönder, yeni strateji al.
// Başarılıysa ESP_OK döner ve new_strategy doldurulur.
// Başarısızsa hata kodu döner, new_strategy değişmez.
esp_err_t http_client_send_trial(const trial_data_t *trial,
                                  led_strategy_t *new_strategy);
