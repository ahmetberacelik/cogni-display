#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// Initialize Wi-Fi in STA mode and attempt connection.
esp_err_t wifi_manager_init(void);

// Wait until connected. Returns error if timeout expires.
esp_err_t wifi_manager_wait_connected(TickType_t timeout_ticks);

// Check if Wi-Fi is currently connected.
bool wifi_manager_is_connected(void);
