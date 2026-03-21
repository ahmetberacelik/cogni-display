#include "mic_adc.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "MIC";

// ADC handles — created in init, used in read
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

// RMS reaching this value counts as "100" (very noisy).
// May need adjustment based on the environment.
#define MAX_RMS_FOR_SCALE 500.0f

esp_err_t mic_adc_init(void)
{
    // Initialize ADC1 unit
    // Why ADC1? Because ADC2 cannot be used while Wi-Fi is active (ESP32 limitation).
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = MIC_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    // Configure the channel
    // Attenuation 12dB = can read 0V-3.1V range (MAX4466 output is in this range)
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = MIC_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,    // 12-bit resolution: 0-4095
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, MIC_ADC_CHANNEL, &chan_cfg));

    // Create calibration — to convert raw ADC values to actual voltage
    // (Raw values are sufficient for our project, but calibration is good practice)
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = MIC_ADC_UNIT,
        .chan = MIC_ADC_CHANNEL,
        .atten = MIC_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration failed, raw values will be used");
        cali_handle = NULL;
    }

    ESP_LOGI(TAG, "Microphone ADC initialized (GPIO%d, ADC1_CH%d)", PIN_MIC_ADC, MIC_ADC_CHANNEL);
    return ESP_OK;
}

esp_err_t mic_adc_read_noise_level(float *noise_level)
{
    int samples[MIC_SAMPLE_COUNT];

    // Step 1: Quickly take 256 ADC readings
    //    Capturing different moments of the sound wave.
    for (int i = 0; i < MIC_SAMPLE_COUNT; i++) {
        esp_err_t ret = adc_oneshot_read(adc_handle, MIC_ADC_CHANNEL, &samples[i]);
        if (ret != ESP_OK) {
            // If this reading failed, skip it
            samples[i] = 2048; // Assume midpoint (silence)
        }
    }

    // Step 2: Calculate the mean (DC offset)
    //    At silence, mic output is VCC/2 = ~1.65V = ~2048 ADC value.
    //    But it may not be exactly 2048, so we find the actual mean.
    float sum = 0;
    for (int i = 0; i < MIC_SAMPLE_COUNT; i++) {
        sum += samples[i];
    }
    float mean = sum / MIC_SAMPLE_COUNT;

    // Step 3: Calculate RMS (Root Mean Square)
    //    Measures how much each reading deviates from the mean.
    //    At silence, deviations are small → low RMS.
    //    With noise, deviations are large → high RMS.
    float sum_sq = 0;
    for (int i = 0; i < MIC_SAMPLE_COUNT; i++) {
        float diff = samples[i] - mean;
        sum_sq += diff * diff;
    }
    float rms = sqrtf(sum_sq / MIC_SAMPLE_COUNT);

    // Step 4: Convert to 0-100 scale
    float level = (rms / MAX_RMS_FOR_SCALE) * 100.0f;
    if (level > 100.0f) {
        level = 100.0f;
    }

    *noise_level = level;
    return ESP_OK;
}
