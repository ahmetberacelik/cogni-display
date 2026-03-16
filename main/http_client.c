#include "http_client.h"
#include "led_animation.h"
#include "config.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "HTTP";

// -----------------------------------------------------------------
// Sunucudan gelen cevabı saklamak için basit bir buffer yapısı.
//
// HTTP cevabı parça parça gelebilir. Her parça geldiğinde
// event handler bu buffer'a ekler. Sonunda tamamını okuruz.
// -----------------------------------------------------------------
typedef struct {
    char *data;       // Cevap metni
    int len;          // Mevcut uzunluk
} response_buffer_t;

// -----------------------------------------------------------------
// HTTP Event Handler
//
// esp_http_client olaya dayalı çalışır. Sunucudan veri geldiğinde
// bu fonksiyon çağrılır ve gelen veriyi buffer'a ekleriz.
// -----------------------------------------------------------------
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    response_buffer_t *buf = (response_buffer_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        // Sunucudan bir veri parçası geldi, buffer'a ekle
        char *new_data = realloc(buf->data, buf->len + evt->data_len + 1);
        if (new_data == NULL) {
            ESP_LOGE(TAG, "Bellek yetersiz!");
            return ESP_FAIL;
        }
        buf->data = new_data;
        memcpy(buf->data + buf->len, evt->data, evt->data_len);
        buf->len += evt->data_len;
        buf->data[buf->len] = '\0';  // String sonu işareti
    }

    return ESP_OK;
}

// -----------------------------------------------------------------
// http_client_send_trial
//
// 1. Trial verisini cJSON ile JSON string'e çevirir
// 2. HTTP POST ile sunucuya gönderir
// 3. Sunucunun cevabını (yeni strateji) parse eder
// -----------------------------------------------------------------
esp_err_t http_client_send_trial(const trial_data_t *trial,
                                  led_strategy_t *new_strategy)
{
    esp_err_t ret = ESP_FAIL;

    // ---- 1. JSON oluştur ----
    // Şu formatı üretiyoruz:
    // {
    //   "dwell_time_ms": 4500,
    //   "noise_level": 42.3,
    //   "current_strategy": { "animation": "breathing", "r": 0, ... }
    // }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "dwell_time_ms", trial->dwell_time_ms);
    cJSON_AddNumberToObject(root, "noise_level", trial->noise_level);

    cJSON *strategy = cJSON_CreateObject();
    cJSON_AddStringToObject(strategy, "animation",
                            led_animation_type_to_str(trial->current_strategy.animation));
    cJSON_AddNumberToObject(strategy, "r", trial->current_strategy.r);
    cJSON_AddNumberToObject(strategy, "g", trial->current_strategy.g);
    cJSON_AddNumberToObject(strategy, "b", trial->current_strategy.b);
    cJSON_AddNumberToObject(strategy, "speed", trial->current_strategy.speed);
    cJSON_AddItemToObject(root, "current_strategy", strategy);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON olusturulamadi!");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Gonderilen JSON: %s", json_str);

    // ---- 2. HTTP POST gönder ----
    response_buffer_t response = { .data = NULL, .len = 0 };

    esp_http_client_config_t config = {
        .url = SERVER_URL,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .user_data = &response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP istegi basarisiz: %s", esp_err_to_name(err));
        goto cleanup;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP cevap kodu: %d", status_code);

    if (status_code != 200) {
        ESP_LOGE(TAG, "Sunucu hata dondu: %d", status_code);
        goto cleanup;
    }

    if (response.data == NULL) {
        ESP_LOGE(TAG, "Sunucudan cevap gelmedi!");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Gelen cevap: %s", response.data);

    // ---- 3. Cevabı parse et ----
    // Beklenen format:
    // {"animation": "rainbow_cycle", "r": 255, "g": 100, "b": 0, "speed": 75}

    cJSON *resp_json = cJSON_Parse(response.data);
    if (resp_json == NULL) {
        ESP_LOGE(TAG, "Cevap JSON parse edilemedi!");
        goto cleanup;
    }

    cJSON *anim = cJSON_GetObjectItem(resp_json, "animation");
    cJSON *r = cJSON_GetObjectItem(resp_json, "r");
    cJSON *g = cJSON_GetObjectItem(resp_json, "g");
    cJSON *b = cJSON_GetObjectItem(resp_json, "b");
    cJSON *spd = cJSON_GetObjectItem(resp_json, "speed");

    if (anim && r && g && b && spd) {
        new_strategy->animation = led_animation_str_to_type(anim->valuestring);
        new_strategy->r = (uint8_t)r->valueint;
        new_strategy->g = (uint8_t)g->valueint;
        new_strategy->b = (uint8_t)b->valueint;
        new_strategy->speed = (uint8_t)spd->valueint;
        ret = ESP_OK;

        ESP_LOGI(TAG, "Yeni strateji: %s, RGB(%d,%d,%d), hiz=%d",
                 led_animation_type_to_str(new_strategy->animation),
                 new_strategy->r, new_strategy->g, new_strategy->b,
                 new_strategy->speed);
    } else {
        ESP_LOGE(TAG, "Cevap JSON'da eksik alan var!");
    }

    cJSON_Delete(resp_json);

cleanup:
    free(json_str);
    free(response.data);
    esp_http_client_cleanup(client);
    return ret;
}
