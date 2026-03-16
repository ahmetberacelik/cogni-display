#pragma once

#include "esp_err.h"

// Trig ve Echo GPIO pinlerini konfigüre eder.
esp_err_t ultrasonic_init(void);

// Tek bir mesafe ölçümü yapar.
// Başarılı olursa distance_cm'e sonucu yazar ve ESP_OK döner.
// Timeout veya hata durumunda ESP_FAIL döner.
esp_err_t ultrasonic_measure_cm(float *distance_cm);
