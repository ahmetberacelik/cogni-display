#pragma once

#include "esp_err.h"

// Configure Trig and Echo GPIO pins.
esp_err_t ultrasonic_init(void);

// Perform a single distance measurement.
// On success, writes the result to distance_cm and returns ESP_OK.
// On timeout or error, returns ESP_FAIL.
esp_err_t ultrasonic_measure_cm(float *distance_cm);
