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
// Simple buffer structure to store the server response.
//
// HTTP responses may arrive in chunks. Each chunk is appended
// by the event handler. We read the full response at the end.
// -----------------------------------------------------------------
typedef struct {
    char *data;       // Response text
    int len;          // Current length
} response_buffer_t;

// -----------------------------------------------------------------
// HTTP Event Handler
//
// esp_http_client is event-driven. When data arrives from the
// server, this function is called to append it to the buffer.
// -----------------------------------------------------------------
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    response_buffer_t *buf = (response_buffer_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        // A chunk of data arrived from the server, append to buffer
        char *new_data = realloc(buf->data, buf->len + evt->data_len + 1);
        if (new_data == NULL) {
            ESP_LOGE(TAG, "Out of memory!");
            return ESP_FAIL;
        }
        buf->data = new_data;
        memcpy(buf->data + buf->len, evt->data, evt->data_len);
        buf->len += evt->data_len;
        buf->data[buf->len] = '\0';  // Null terminator
    }

    return ESP_OK;
}

// -----------------------------------------------------------------
// http_client_send_trial
//
// 1. Converts trial data to JSON string using cJSON
// 2. Sends it to the server via HTTP POST
// 3. Parses the server response (new strategy)
// -----------------------------------------------------------------
esp_err_t http_client_send_trial(const trial_data_t *trial,
                                  led_strategy_t *new_strategy)
{
    esp_err_t ret = ESP_FAIL;

    // ---- 1. Build JSON ----
    // Produces the following format:
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

    // Density data
    cJSON_AddNumberToObject(root, "density_score", trial->density_score);
    cJSON_AddStringToObject(root, "density_category", trial->density_category);
    cJSON_AddNumberToObject(root, "person_count_2min", trial->person_count_2min);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON!");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Sent JSON: %s", json_str);

    // ---- 2. Send HTTP POST ----
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
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP response code: %d", status_code);

    if (status_code != 200) {
        ESP_LOGE(TAG, "Server returned error: %d", status_code);
        goto cleanup;
    }

    if (response.data == NULL) {
        ESP_LOGE(TAG, "No response from server!");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Received response: %s", response.data);

    // ---- 3. Parse response ----
    // Expected format:
    // {"animation": "rainbow_cycle", "r": 255, "g": 100, "b": 0, "speed": 75}

    cJSON *resp_json = cJSON_Parse(response.data);
    if (resp_json == NULL) {
        ESP_LOGE(TAG, "Failed to parse response JSON!");
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

        ESP_LOGI(TAG, "New strategy: %s, RGB(%d,%d,%d), speed=%d",
                 led_animation_type_to_str(new_strategy->animation),
                 new_strategy->r, new_strategy->g, new_strategy->b,
                 new_strategy->speed);

        // Log the AI reasoning
        cJSON *reason = cJSON_GetObjectItem(resp_json, "reason");
        if (reason && cJSON_IsString(reason)) {
            ESP_LOGI(TAG, "Gemini reasoning: %s", reason->valuestring);
        }
    } else {
        ESP_LOGE(TAG, "Response JSON has missing fields!");
    }

    cJSON_Delete(resp_json);

cleanup:
    free(json_str);
    free(response.data);
    esp_http_client_cleanup(client);
    return ret;
}
