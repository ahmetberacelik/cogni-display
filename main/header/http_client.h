#pragma once

#include "esp_err.h"
#include "led_animation.h"
#include <stdint.h>

// Trial data — sensor readings + current strategy + density.
// This packet is sent to the server.
typedef struct {
    uint32_t dwell_time_ms;            // How long the person stayed (ms)
    float noise_level;                 // Ambient noise level (0-100)
    led_strategy_t current_strategy;   // Animation playing at the time
    float density_score;               // Density score (0.0 - 1.0)
    char density_category[8];          // "low", "medium", "high"
    int person_count_2min;             // Person count in the last 2 minutes
} trial_data_t;

// Send trial data to the server and get a new strategy.
// Returns ESP_OK on success and fills new_strategy.
// Returns error code on failure, new_strategy remains unchanged.
esp_err_t http_client_send_trial(const trial_data_t *trial,
                                  led_strategy_t *new_strategy);
