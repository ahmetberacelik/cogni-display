#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// Wi-Fi STA modunda başlat ve bağlantıyı dene.
esp_err_t wifi_manager_init(void);

// Bağlantı kurulana kadar bekle. Timeout süresi geçerse hata döner.
esp_err_t wifi_manager_wait_connected(TickType_t timeout_ticks);

// Şu an Wi-Fi bağlı mı?
bool wifi_manager_is_connected(void);