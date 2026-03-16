#include "mic_adc.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "MIC";

// ADC handle'ları — init'te oluşturulur, read'de kullanılır
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

// RMS'in bu değere ulaşması "100" (çok gürültülü) sayılır.
// Pratikte ortama göre ayarlanması gerekebilir.
#define MAX_RMS_FOR_SCALE 500.0f

esp_err_t mic_adc_init(void)
{
    // ADC1 birimini başlat
    // Neden ADC1? Çünkü ADC2, Wi-Fi açıkken kullanılamaz (ESP32 kısıtlaması).
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = MIC_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    // Kanalı konfigüre et
    // Attenuation 12dB = 0V-3.1V aralığını okuyabilir (MAX4466 çıkışı bu aralıkta)
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = MIC_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,    // 12-bit çözünürlük: 0-4095
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, MIC_ADC_CHANNEL, &chan_cfg));

    // Kalibrasyon oluştur — ham ADC değerini gerçek voltaja çevirmek için
    // (Bizim projede ham değer yeterli ama kalibrasyon iyi pratik)
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = MIC_ADC_UNIT,
        .chan = MIC_ADC_CHANNEL,
        .atten = MIC_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC kalibrasyon olusturulamadi, ham degerler kullanilacak");
        cali_handle = NULL;
    }

    ESP_LOGI(TAG, "Mikrofon ADC baslatildi (GPIO%d, ADC1_CH%d)", PIN_MIC_ADC, MIC_ADC_CHANNEL);
    return ESP_OK;
}

esp_err_t mic_adc_read_noise_level(float *noise_level)
{
    int samples[MIC_SAMPLE_COUNT];

    // 1. Adım: Hızlıca 256 adet ADC okuması yap
    //    Ses dalgasının farklı anlarını yakalıyoruz.
    for (int i = 0; i < MIC_SAMPLE_COUNT; i++) {
        esp_err_t ret = adc_oneshot_read(adc_handle, MIC_ADC_CHANNEL, &samples[i]);
        if (ret != ESP_OK) {
            // Bu okuma başarısız olduysa atla
            samples[i] = 2048; // Orta nokta (sessizlik) varsay
        }
    }

    // 2. Adım: Ortalamayı hesapla (DC offset)
    //    Sessizlikte mikrofon çıkışı VCC/2 = ~1.65V = ~2048 ADC değeri.
    //    Ama tam 2048 olmayabilir, o yüzden gerçek ortalamayı buluyoruz.
    float sum = 0;
    for (int i = 0; i < MIC_SAMPLE_COUNT; i++) {
        sum += samples[i];
    }
    float mean = sum / MIC_SAMPLE_COUNT;

    // 3. Adım: RMS (Root Mean Square) hesapla
    //    Her okumanın ortalamadan ne kadar saptığını ölçüyoruz.
    //    Sessizlikte sapmalar küçük → düşük RMS.
    //    Gürültüde sapmalar büyük → yüksek RMS.
    float sum_sq = 0;
    for (int i = 0; i < MIC_SAMPLE_COUNT; i++) {
        float diff = samples[i] - mean;
        sum_sq += diff * diff;
    }
    float rms = sqrtf(sum_sq / MIC_SAMPLE_COUNT);

    // 4. Adım: 0-100 skalasına çevir
    float level = (rms / MAX_RMS_FOR_SCALE) * 100.0f;
    if (level > 100.0f) {
        level = 100.0f;
    }

    *noise_level = level;
    return ESP_OK;
}
