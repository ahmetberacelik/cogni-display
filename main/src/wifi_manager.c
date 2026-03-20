#include "wifi_manager.h"
#include "config.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"

// Log mesajlarında "WIFI" etiketi görünsün
static const char *TAG = "WIFI";

// Event group: bir "bayrak sistemi". Bağlantı durumunu takip etmek için kullanıyoruz.
// Diğer tasklar "bağlanana kadar bekle" diyebilsin diye.
static EventGroupHandle_t s_wifi_event_group;

// Bayrak bitleri
#define CONNECTED_BIT  BIT0   // Wi-Fi bağlandı ve IP alındı
#define FAIL_BIT       BIT1   // Bağlantı başarısız oldu

// Kaç kez bağlanmayı denedik
static int s_retry_count = 0;

// -------------------------------------------------------------------
// Event Handler — Wi-Fi ve IP olaylarını dinleyen fonksiyon.
//
// ESP-IDF "olay tabanlı" çalışır: Bir şey olduğunda (bağlandı, koptu,
// IP aldı) bu fonksiyon otomatik çağrılır. Biz de buna göre tepki veririz.
// -------------------------------------------------------------------
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    // --- Wi-Fi başlatıldığında: bağlantıyı dene ---
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi baslatildi, baglaniliyor...");
        esp_wifi_connect();
    }
    // --- Bağlantı koptuğunda: tekrar dene ---
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRIES) {
            s_retry_count++;
            ESP_LOGW(TAG, "Baglanti koptu. Tekrar deneniyor (%d/%d)...",
                     s_retry_count, WIFI_MAX_RETRIES);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Maksimum deneme sayisina ulasildi!");
            xEventGroupSetBits(s_wifi_event_group, FAIL_BIT);
        }
    }
    // --- IP adresi alındığında: bağlantı başarılı! ---
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Baglanti basarili! IP adresimiz: " IPSTR,
                 IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    }
}

// -------------------------------------------------------------------
// wifi_manager_init — Wi-Fi'yi STA modunda başlatır.
//
// Adımlar:
//   1. NVS başlat (Wi-Fi kütüphanesi bunu kullanır)
//   2. Ağ arayüzü oluştur
//   3. Event handler'ları kaydet
//   4. SSID ve şifreyi ayarla
//   5. Bağlantıyı başlat
// -------------------------------------------------------------------
esp_err_t wifi_manager_init(void)
{
    // 1) NVS başlat
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS bozulmuşsa sil ve tekrar başlat
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return ret;

    // 2) Event group oluştur
    s_wifi_event_group = xEventGroupCreate();

    // 3) Ağ arayüzünü başlat
    ret = esp_netif_init();
    if (ret != ESP_OK) return ret;

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) return ret;

    // Varsayılan Wi-Fi STA arayüzünü oluştur
    esp_netif_create_default_wifi_sta();

    // 4) Wi-Fi driver'ını varsayılan ayarlarla başlat
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) return ret;

    // 5) Event handler'ları kaydet (Wi-Fi ve IP olaylarını dinle)
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &event_handler, NULL);

    // 6) SSID ve şifreyi ayarla
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    // 7) Wi-Fi'yi başlat (event_handler STA_START olayını alınca bağlanacak)
    ret = esp_wifi_start();
    return ret;
}

// -------------------------------------------------------------------
// wifi_manager_wait_connected — Bağlanana kadar bekle.
//
// Event group'taki CONNECTED_BIT veya FAIL_BIT set edilene kadar
// bu fonksiyon bloke olur (bekler). Timeout süresi geçerse de döner.
// -------------------------------------------------------------------
esp_err_t wifi_manager_wait_connected(TickType_t timeout_ticks)
{
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        CONNECTED_BIT | FAIL_BIT,   // Bu bitlerden birini bekle
        pdFALSE,                    // Bitleri temizleme
        pdFALSE,                    // Herhangi biri yeterli (AND değil)
        timeout_ticks               // Maksimum bekleme süresi
    );

    if (bits & CONNECTED_BIT) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

// -------------------------------------------------------------------
// wifi_manager_is_connected — Şu an bağlı mıyız?
// -------------------------------------------------------------------
bool wifi_manager_is_connected(void)
{
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & CONNECTED_BIT) != 0;
}