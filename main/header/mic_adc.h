#pragma once

#include "esp_err.h"

// Configure ADC hardware and perform calibration.
esp_err_t mic_adc_init(void);

// Measure ambient noise level (0.0 = silent, 100.0 = very noisy).
// Takes 256 fast readings and produces a result via RMS calculation.
esp_err_t mic_adc_read_noise_level(float *noise_level);
