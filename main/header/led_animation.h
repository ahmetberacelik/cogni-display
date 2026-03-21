#pragma once

#include "esp_err.h"
#include <stdint.h>

// Animation types
typedef enum {
    ANIM_SOLID,          // Solid color
    ANIM_BREATHING,      // Breathing (brightness rises and falls)
    ANIM_RAINBOW_CYCLE,  // Rainbow cycle
    ANIM_BLINK,          // Blinking
    ANIM_WAVE,           // Wave effect
    ANIM_COLOR_WIPE,     // Sequential fill
    ANIM_COUNT           // Total animation count (internal use)
} animation_type_t;

// Strategy from Gemini — which animation, which color, which speed
typedef struct {
    animation_type_t animation;
    uint8_t r, g, b;     // RGB color (0-255)
    uint8_t speed;       // Speed (1-100)
} led_strategy_t;

// Initialize LED strip and create the animation task.
esp_err_t led_animation_init(void);

// Apply a new strategy (thread-safe, can be called from any task).
void led_animation_set_strategy(const led_strategy_t *strategy);

// Read the current strategy (thread-safe).
led_strategy_t led_animation_get_strategy(void);

// Animation type to/from string conversions (for JSON parse/build).
const char* led_animation_type_to_str(animation_type_t type);
animation_type_t led_animation_str_to_type(const char *str);
