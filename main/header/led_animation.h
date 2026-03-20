#pragma once

#include "esp_err.h"
#include <stdint.h>

// Animasyon türleri
typedef enum {
    ANIM_SOLID,          // Sabit renk
    ANIM_BREATHING,      // Nefes alma (parlaklık artıp azalır)
    ANIM_RAINBOW_CYCLE,  // Gökkuşağı döngüsü
    ANIM_BLINK,          // Yanıp sönme
    ANIM_WAVE,           // Dalga efekti
    ANIM_COLOR_WIPE,     // Sıralı dolum
    ANIM_COUNT           // Toplam animasyon sayısı (kod içi kullanım)
} animation_type_t;

// Gemini'den gelen strateji — hangi animasyon, hangi renk, hangi hız
typedef struct {
    animation_type_t animation;
    uint8_t r, g, b;     // RGB renk (0-255)
    uint8_t speed;       // Hız (1-100)
} led_strategy_t;

// LED strip'i başlat ve animasyon task'ını oluştur.
esp_err_t led_animation_init(void);

// Yeni strateji uygula (thread-safe, herhangi bir task'tan çağrılabilir).
void led_animation_set_strategy(const led_strategy_t *strategy);

// Mevcut stratejiyi oku (thread-safe).
led_strategy_t led_animation_get_strategy(void);

// Animasyon tipi string dönüşümleri (JSON parse/oluşturma için).
const char* led_animation_type_to_str(animation_type_t type);
animation_type_t led_animation_str_to_type(const char *str);
