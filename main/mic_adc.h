#pragma once

#include "esp_err.h"

// ADC donanımını konfigüre eder ve kalibrasyonu yapar.
esp_err_t mic_adc_init(void);

// Ortam gürültü seviyesini ölçer (0.0 = sessiz, 100.0 = çok gürültülü).
// 256 adet hızlı okuma yapıp RMS hesabıyla sonuç üretir.
esp_err_t mic_adc_read_noise_level(float *noise_level);
