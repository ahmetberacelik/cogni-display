#include "ultrasonic.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "ULTRASONIC";

// Measurement timeout (microseconds).
// Some HC-SR04 clones keep Echo HIGH for up to 38ms when no obstacle is present.
// 50ms provides a safe upper limit.
#define TIMEOUT_US 50000

esp_err_t ultrasonic_init(void)
{
    // Trig pin: we send signals → Output
    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << PIN_ULTRASONIC_TRIG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&trig_conf);

    // Echo pin: we receive signals from the sensor → Input
    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << PIN_ULTRASONIC_ECHO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&echo_conf);

    // Start Trig pin at LOW
    gpio_set_level(PIN_ULTRASONIC_TRIG, 0);

    ESP_LOGI(TAG, "Ultrasonic sensor initialized (Trig: GPIO%d, Echo: GPIO%d)",
             PIN_ULTRASONIC_TRIG, PIN_ULTRASONIC_ECHO);
    return ESP_OK;
}

esp_err_t ultrasonic_measure_cm(float *distance_cm)
{
    // Step 0: Echo may still be HIGH from the previous measurement.
    //    Wait for it to go LOW before triggering a new one.
    int64_t pre_wait = esp_timer_get_time();
    while (gpio_get_level(PIN_ULTRASONIC_ECHO) == 1) {
        if ((esp_timer_get_time() - pre_wait) > TIMEOUT_US) {
            ESP_LOGW(TAG, "Echo pin stuck HIGH at start, resetting...");
            // Send a short trig pulse to reset the sensor
            gpio_set_level(PIN_ULTRASONIC_TRIG, 1);
            esp_rom_delay_us(10);
            gpio_set_level(PIN_ULTRASONIC_TRIG, 0);
            esp_rom_delay_us(60000);  // Wait 60ms for full sensor reset
            return ESP_FAIL;
        }
    }

    // Minimum wait between measurements (for sensor recovery)
    esp_rom_delay_us(50);

    // Step 1: Send a 10 microsecond HIGH signal to the Trig pin
    //    This tells the sensor to start measuring.
    gpio_set_level(PIN_ULTRASONIC_TRIG, 1);
    esp_rom_delay_us(10);
    gpio_set_level(PIN_ULTRASONIC_TRIG, 0);

    // Step 2: Wait for Echo pin to go HIGH
    //    The sensor is emitting the sound wave, Echo is still LOW.
    int64_t start_wait = esp_timer_get_time();
    while (gpio_get_level(PIN_ULTRASONIC_ECHO) == 0) {
        if ((esp_timer_get_time() - start_wait) > TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout: Echo did not go HIGH");
            return ESP_FAIL;
        }
    }

    // Step 3: Echo went HIGH! Record the time.
    //    The sound wave has been sent.
    int64_t echo_start = esp_timer_get_time();

    // Step 4: Wait for Echo pin to go LOW
    //    Echo goes LOW when the sound wave bounces back from an obstacle.
    while (gpio_get_level(PIN_ULTRASONIC_ECHO) == 1) {
        if ((esp_timer_get_time() - echo_start) > TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout: Echo did not go LOW (no obstacle or too far)");
            return ESP_FAIL;
        }
    }

    // Step 5: Calculate duration and convert to cm
    //    Sound traveled round-trip. Dividing total time by 58 gives distance in cm.
    int64_t echo_end = esp_timer_get_time();
    int64_t duration_us = echo_end - echo_start;
    float distance = (float)duration_us / 58.0f;

    // Invalid range check (HC-SR04: reliable between 2cm - 400cm)
    if (distance < 2.0f || distance > 400.0f) {
        ESP_LOGW(TAG, "Invalid distance: %.1f cm (out of range)", distance);
        return ESP_FAIL;
    }

    *distance_cm = distance;
    return ESP_OK;
}
