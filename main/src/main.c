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
//  State Machine — Three system states
// ============================================================
typedef enum {
    STATE_IDLE,              // No one around, scanning
    STATE_PERSON_DETECTED,   // Someone present, measuring dwell time and noise
    STATE_PERSON_LEFT,       // Person left, data will be sent
} app_state_t;

// ============================================================
//  Density Tracking — Events within the last 2 minutes
// ============================================================
typedef struct {
    int64_t timestamp_ms;      // When the person left
    uint32_t dwell_time_ms;    // How long they stayed
    float noise_level;         // Average noise at that time
} density_event_t;

// Calculate density: generate a score from events within the last 2 minutes
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

    // Normalize: person count (max 10), dwell (max 30s), noise (max 100)
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
//  Main Program
// ============================================================
void app_main(void)
{
    ESP_LOGI(TAG, "CogniDisplay starting...");

    // ---- 1. Initialize NVS (required for Wi-Fi) ----
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ---- 2. Initialize Wi-Fi and connect ----
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization failed!");
        return;
    }

    ESP_LOGI(TAG, "Waiting for Wi-Fi connection (30s)...");
    ret = wifi_manager_wait_connected(pdMS_TO_TICKS(30000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed!");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi connected.");

    // ---- 3. Initialize sensors ----
    ultrasonic_init();
    mic_adc_init();

    // ---- 4. Start LED animation ----
    led_animation_init();

    // Default strategy: rainbow cycle
    led_strategy_t current_strategy = {
        .animation = ANIM_RAINBOW_CYCLE,
        .r = 0, .g = 0, .b = 255,
        .speed = 50,
    };
    led_animation_set_strategy(&current_strategy);

    // ---- 5. Startup Calibration ----
    ESP_LOGI(TAG, "Calibration starting (%d seconds)...", CALIBRATION_DURATION_MS / 1000);

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
        ESP_LOGW(TAG, "Calibration failed, default baseline: %.1f cm", baseline_cm);
    }

    float detect_threshold = baseline_cm * BASELINE_DETECT_RATIO;
    float depart_threshold = baseline_cm * BASELINE_DEPART_RATIO;

    ESP_LOGI(TAG, "Calibration done! Baseline: %.1f cm, Detect threshold: %.1f cm, Depart threshold: %.1f cm",
             baseline_cm, detect_threshold, depart_threshold);

    // ---- 6. State machine variables ----
    app_state_t state = STATE_IDLE;
    int64_t detect_start_time = 0;
    float noise_sum = 0;
    int noise_count = 0;
    int consecutive_absent = 0;

    // Adaptive baseline tracking
    float baseline_candidate = 0;
    int64_t baseline_stable_since = 0;
    bool baseline_tracking = false;

    // Density tracking (circular buffer)
    density_event_t density_events[DENSITY_MAX_EVENTS];
    memset(density_events, 0, sizeof(density_events));
    int density_event_count = 0;
    int density_event_index = 0;

    // Idle mode
    int64_t last_person_time_ms = esp_timer_get_time() / 1000;
    bool idle_mode_active = false;

    ESP_LOGI(TAG, "System ready. Scanning started...");

    // ---- 7. Main loop ----
    while (1) {
        // Measure distance
        float distance_cm = 999.0f;
        esp_err_t meas_ret = ultrasonic_measure_cm(&distance_cm);
        if (meas_ret != ESP_OK) {
            distance_cm = 999.0f;
        }

        int64_t now_ms = esp_timer_get_time() / 1000;

        ESP_LOGI(TAG, "[%s] Distance: %.1f cm (baseline: %.1f)",
                 (state == STATE_IDLE) ? "IDLE" :
                 (state == STATE_PERSON_DETECTED) ? "DETECTED" : "LEFT",
                 distance_cm, baseline_cm);

        switch (state) {

        // ---- IDLE: No one around, scanning ----
        case STATE_IDLE:
            // --- Adaptive baseline update ---
            if (distance_cm < 400.0f) {
                if (!baseline_tracking) {
                    baseline_tracking = true;
                    baseline_candidate = distance_cm;
                    baseline_stable_since = now_ms;
                } else if (fabsf(distance_cm - baseline_candidate) < BASELINE_UPDATE_TOLERANCE_CM) {
                    // Still stable — has 60s passed?
                    if ((now_ms - baseline_stable_since) > BASELINE_UPDATE_TIMEOUT_MS) {
                        baseline_cm = baseline_candidate;
                        detect_threshold = baseline_cm * BASELINE_DETECT_RATIO;
                        depart_threshold = baseline_cm * BASELINE_DEPART_RATIO;
                        baseline_tracking = false;
                        ESP_LOGI(TAG, "Baseline updated: %.1f cm (detect: %.1f, depart: %.1f)",
                                 baseline_cm, detect_threshold, depart_threshold);
                    }
                } else {
                    // Distance changed, reset tracking
                    baseline_candidate = distance_cm;
                    baseline_stable_since = now_ms;
                }
            }

            // --- Person detection (dynamic threshold) ---
            if (distance_cm < detect_threshold) {
                state = STATE_PERSON_DETECTED;
                detect_start_time = esp_timer_get_time();
                noise_sum = 0;
                noise_count = 0;
                consecutive_absent = 0;
                last_person_time_ms = now_ms;
                baseline_tracking = false;

                // Exit idle mode
                if (idle_mode_active) {
                    idle_mode_active = false;
                    led_animation_set_strategy(&current_strategy);
                    ESP_LOGI(TAG, "Idle mode off, returning to normal mode.");
                }

                ESP_LOGI(TAG, "Person detected! Distance: %.1f cm (threshold: %.1f)",
                         distance_cm, detect_threshold);
            }

            // --- Idle timeout check ---
            if (!idle_mode_active && (now_ms - last_person_time_ms) > IDLE_TIMEOUT_MS) {
                idle_mode_active = true;
                led_strategy_t idle_strategy = {
                    .animation = ANIM_SOLID,
                    .r = 255, .g = 255, .b = 255,
                    .speed = 50,
                };
                led_animation_set_strategy(&idle_strategy);
                ESP_LOGW(TAG, "No one for 2 minutes. Idle mode: solid white.");
            }
            break;

        // ---- PERSON_DETECTED: Someone present, measuring ----
        case STATE_PERSON_DETECTED:
            // Accumulate noise measurements
            {
                float noise = 0;
                if (mic_adc_read_noise_level(&noise) == ESP_OK) {
                    noise_sum += noise;
                    noise_count++;
                }
            }

            // Is the person still here? (dynamic threshold)
            if (distance_cm > depart_threshold) {
                consecutive_absent++;
                if (consecutive_absent >= PERSON_LEFT_CONFIRM_COUNT) {
                    state = STATE_PERSON_LEFT;
                    ESP_LOGI(TAG, "Person left.");
                }
            } else {
                consecutive_absent = 0;
            }
            break;

        // ---- PERSON_LEFT: Send data, get new strategy ----
        case STATE_PERSON_LEFT:
            {
                // Calculate dwell time
                int64_t now_us = esp_timer_get_time();
                uint32_t dwell_time_ms = (uint32_t)((now_us - detect_start_time) / 1000);

                // Average noise
                float avg_noise = (noise_count > 0) ? (noise_sum / noise_count) : 0;

                ESP_LOGI(TAG, "Dwell time: %lu ms, Average noise: %.1f",
                         (unsigned long)dwell_time_ms, avg_noise);

                // Ignore very short detections
                if (dwell_time_ms < MIN_DWELL_TIME_MS) {
                    ESP_LOGW(TAG, "Too short (%lu ms), ignoring.",
                             (unsigned long)dwell_time_ms);
                    state = STATE_IDLE;
                    break;
                }

                // Record event in density buffer
                density_events[density_event_index] = (density_event_t){
                    .timestamp_ms = now_ms,
                    .dwell_time_ms = dwell_time_ms,
                    .noise_level = avg_noise,
                };
                density_event_index = (density_event_index + 1) % DENSITY_MAX_EVENTS;
                if (density_event_count < DENSITY_MAX_EVENTS) {
                    density_event_count++;
                }

                // Calculate density
                float density_score = 0;
                const char *density_category = "low";
                int person_count_2min = 0;
                float avg_dwell = 0;
                calculate_density(density_events, density_event_count, now_ms,
                                  &density_score, &density_category,
                                  &person_count_2min, &avg_dwell);

                ESP_LOGI(TAG, "Density: %.2f (%s), Last 2min people: %d, Avg dwell: %.0f ms",
                         density_score, density_category, person_count_2min, avg_dwell);

                last_person_time_ms = now_ms;

                // Send to server
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
                        ESP_LOGI(TAG, "New strategy applied: %s",
                                 led_animation_type_to_str(current_strategy.animation));
                    } else {
                        ESP_LOGW(TAG, "Server error, keeping current strategy.");
                    }
                } else {
                    ESP_LOGW(TAG, "Wi-Fi not connected, data not sent.");
                }

                state = STATE_IDLE;
            }
            break;
        }

        // Wait 100ms (10 scans per second)
        vTaskDelay(pdMS_TO_TICKS(ULTRASONIC_SCAN_INTERVAL_MS));
    }
}
