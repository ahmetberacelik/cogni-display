#include "wifi_manager.h"
#include "config.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"

// Tag for log messages
static const char *TAG = "WIFI";

// Event group: a flag system for tracking connection status.
// Other tasks can wait until connected.
static EventGroupHandle_t s_wifi_event_group;

// Flag bits
#define CONNECTED_BIT  BIT0   // Wi-Fi connected and IP obtained
#define FAIL_BIT       BIT1   // Connection failed

// How many times we have attempted to connect
static int s_retry_count = 0;

// -------------------------------------------------------------------
// Event Handler — Listens to Wi-Fi and IP events.
//
// ESP-IDF is event-driven: when something happens (connected,
// disconnected, got IP) this function is called automatically.
// -------------------------------------------------------------------
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    // --- Wi-Fi started: attempt connection ---
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi started, connecting...");
        esp_wifi_connect();
    }
    // --- Disconnected: retry ---
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRIES) {
            s_retry_count++;
            ESP_LOGW(TAG, "Connection lost. Retrying (%d/%d)...",
                     s_retry_count, WIFI_MAX_RETRIES);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Maximum retry count reached!");
            xEventGroupSetBits(s_wifi_event_group, FAIL_BIT);
        }
    }
    // --- Got IP address: connection successful! ---
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connection successful! Our IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    }
}

// -------------------------------------------------------------------
// wifi_manager_init — Initialize Wi-Fi in STA mode.
//
// Steps:
//   1. Initialize NVS (Wi-Fi library uses it)
//   2. Create network interface
//   3. Register event handlers
//   4. Set SSID and password
//   5. Start connection
// -------------------------------------------------------------------
esp_err_t wifi_manager_init(void)
{
    // 1) Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS corrupted, erase and reinitialize
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return ret;

    // 2) Create event group
    s_wifi_event_group = xEventGroupCreate();

    // 3) Initialize network interface
    ret = esp_netif_init();
    if (ret != ESP_OK) return ret;

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) return ret;

    // Create default Wi-Fi STA interface
    esp_netif_create_default_wifi_sta();

    // 4) Initialize Wi-Fi driver with default settings
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) return ret;

    // 5) Register event handlers (listen for Wi-Fi and IP events)
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &event_handler, NULL);

    // 6) Set SSID and password
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    // 7) Start Wi-Fi (event_handler will connect on STA_START event)
    ret = esp_wifi_start();
    return ret;
}

// -------------------------------------------------------------------
// wifi_manager_wait_connected — Wait until connected.
//
// Blocks until CONNECTED_BIT or FAIL_BIT is set in the event group.
// Returns after timeout if neither bit is set.
// -------------------------------------------------------------------
esp_err_t wifi_manager_wait_connected(TickType_t timeout_ticks)
{
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        CONNECTED_BIT | FAIL_BIT,   // Wait for either of these bits
        pdFALSE,                    // Don't clear bits
        pdFALSE,                    // Any bit is enough (not AND)
        timeout_ticks               // Maximum wait time
    );

    if (bits & CONNECTED_BIT) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

// -------------------------------------------------------------------
// wifi_manager_is_connected — Are we currently connected?
// -------------------------------------------------------------------
bool wifi_manager_is_connected(void)
{
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & CONNECTED_BIT) != 0;
}
