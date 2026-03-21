#include "led_animation.h"
#include "config.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "LED";

// LED strip handle — for communicating with the hardware
static led_strip_handle_t strip = NULL;

// Current strategy and its protecting lock (mutex)
static led_strategy_t current_strategy = {
    .animation = ANIM_RAINBOW_CYCLE,
    .r = 0, .g = 0, .b = 255,
    .speed = 50
};
static SemaphoreHandle_t strategy_mutex = NULL;

// ============================================================
//  Helper: HSV → RGB conversion (for rainbow animation)
// ============================================================
// HSV color space: Hue (color tone 0-360), Saturation (0-255), Value (brightness 0-255)
// Easier for humans to think about: "120 degrees = green, 240 = blue", etc.
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = h / 60;
    uint8_t remainder = (h - (region * 60)) * 255 / 60;

    uint8_t p = (v * (255 - s)) / 255;
    uint8_t q = (v * (255 - ((s * remainder) / 255))) / 255;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) / 255))) / 255;

    switch (region) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

// ============================================================
//  Speed → milliseconds conversion
// ============================================================
// speed=1 → 5000ms (very slow cycle), speed=100 → 500ms (fast cycle)
static uint32_t speed_to_cycle_ms(uint8_t speed)
{
    // Linear interpolation: speed 1-100 → 5000-500ms
    return 5000 - (uint32_t)(speed - 1) * 4500 / 99;
}

// ============================================================
//  Animation functions — each one draws a single "frame"
// ============================================================

// 1. SOLID: All LEDs lit with the same color
static void anim_solid(const led_strategy_t *s, uint32_t frame)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        led_strip_set_pixel(strip, i, s->r, s->g, s->b);
    }
}

// 2. BREATHING: Brightness rises and falls like a sine wave
static void anim_breathing(const led_strategy_t *s, uint32_t frame)
{
    uint32_t cycle_ms = speed_to_cycle_ms(s->speed);
    uint32_t cycle_frames = cycle_ms / 33;  // 30 FPS → each frame ~33ms
    if (cycle_frames == 0) cycle_frames = 1;

    // Sine wave brightness: 0.0 → 1.0 → 0.0
    float phase = (float)(frame % cycle_frames) / cycle_frames;
    float brightness = (sinf(phase * 2.0f * M_PI - M_PI / 2.0f) + 1.0f) / 2.0f;

    uint8_t r = (uint8_t)(s->r * brightness);
    uint8_t g = (uint8_t)(s->g * brightness);
    uint8_t b = (uint8_t)(s->b * brightness);

    for (int i = 0; i < NUM_LEDS; i++) {
        led_strip_set_pixel(strip, i, r, g, b);
    }
}

// 3. RAINBOW_CYCLE: Each LED has a different color, all colors rotate
static void anim_rainbow_cycle(const led_strategy_t *s, uint32_t frame)
{
    uint32_t cycle_ms = speed_to_cycle_ms(s->speed);
    uint32_t cycle_frames = cycle_ms / 33;
    if (cycle_frames == 0) cycle_frames = 1;

    uint16_t hue_offset = (frame % cycle_frames) * 360 / cycle_frames;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Each LED is spaced 360/8 = 45 degrees apart
        uint16_t hue = (hue_offset + i * 360 / NUM_LEDS) % 360;
        uint8_t r, g, b;
        hsv_to_rgb(hue, 255, 180, &r, &g, &b);
        led_strip_set_pixel(strip, i, r, g, b);
    }
}

// 4. BLINK: All LEDs toggle on/off
static void anim_blink(const led_strategy_t *s, uint32_t frame)
{
    uint32_t cycle_ms = speed_to_cycle_ms(s->speed);
    uint32_t cycle_frames = cycle_ms / 33;
    if (cycle_frames == 0) cycle_frames = 1;

    // First half of cycle: on, second half: off
    bool on = (frame % cycle_frames) < (cycle_frames / 2);

    for (int i = 0; i < NUM_LEDS; i++) {
        if (on) {
            led_strip_set_pixel(strip, i, s->r, s->g, s->b);
        } else {
            led_strip_set_pixel(strip, i, 0, 0, 0);
        }
    }
}

// 5. WAVE: Light travels around the ring
static void anim_wave(const led_strategy_t *s, uint32_t frame)
{
    uint32_t cycle_ms = speed_to_cycle_ms(s->speed);
    uint32_t cycle_frames = cycle_ms / 33;
    if (cycle_frames == 0) cycle_frames = 1;

    // Which LED is currently at the "bright" position
    float position = (float)(frame % cycle_frames) / cycle_frames * NUM_LEDS;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Calculate brightness based on distance from the bright spot
        float dist = fmodf(fabsf(i - position), NUM_LEDS);
        if (dist > NUM_LEDS / 2.0f) dist = NUM_LEDS - dist;
        float brightness = 1.0f - (dist / (NUM_LEDS / 2.0f));
        if (brightness < 0) brightness = 0;
        brightness = brightness * brightness;  // Sharper falloff

        led_strip_set_pixel(strip, i,
            (uint8_t)(s->r * brightness),
            (uint8_t)(s->g * brightness),
            (uint8_t)(s->b * brightness));
    }
}

// 6. COLOR_WIPE: LEDs light up one by one, then turn off one by one
static void anim_color_wipe(const led_strategy_t *s, uint32_t frame)
{
    uint32_t cycle_ms = speed_to_cycle_ms(s->speed);
    uint32_t cycle_frames = cycle_ms / 33;
    if (cycle_frames == 0) cycle_frames = 1;

    // Where in the cycle are we (0.0 → 1.0)
    float phase = (float)(frame % cycle_frames) / cycle_frames;
    // First half: light up sequentially. Second half: turn off sequentially.
    int lit_count;
    if (phase < 0.5f) {
        lit_count = (int)(phase * 2.0f * NUM_LEDS);
    } else {
        lit_count = NUM_LEDS - (int)((phase - 0.5f) * 2.0f * NUM_LEDS);
    }

    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < lit_count) {
            led_strip_set_pixel(strip, i, s->r, s->g, s->b);
        } else {
            led_strip_set_pixel(strip, i, 0, 0, 0);
        }
    }
}

// ============================================================
//  Animation task — runs continuously in the background
// ============================================================
static void led_animation_task(void *arg)
{
    uint32_t frame = 0;

    while (1) {
        // Safely read the current strategy
        xSemaphoreTake(strategy_mutex, portMAX_DELAY);
        led_strategy_t s = current_strategy;
        xSemaphoreGive(strategy_mutex);

        // Call the appropriate animation function
        switch (s.animation) {
            case ANIM_SOLID:         anim_solid(&s, frame);         break;
            case ANIM_BREATHING:     anim_breathing(&s, frame);     break;
            case ANIM_RAINBOW_CYCLE: anim_rainbow_cycle(&s, frame); break;
            case ANIM_BLINK:         anim_blink(&s, frame);         break;
            case ANIM_WAVE:          anim_wave(&s, frame);          break;
            case ANIM_COLOR_WIPE:    anim_color_wipe(&s, frame);    break;
            default:                 anim_solid(&s, frame);         break;
        }

        // Send colors to the LEDs
        led_strip_refresh(strip);

        frame++;
        // ~30 FPS (each frame 33ms)
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

// ============================================================
//  Public functions
// ============================================================

esp_err_t led_animation_init(void)
{
    // Configure LED strip hardware
    led_strip_config_t strip_config = {
        .strip_gpio_num = PIN_LED_DATA,
        .max_leds = NUM_LEDS,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    // RMT backend — generates the precise timing signals for WS2812B
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10 MHz
        .flags.with_dma = false,             // DMA unnecessary for 8 LEDs
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    led_strip_clear(strip);

    // Create mutex (for thread safety during strategy changes)
    strategy_mutex = xSemaphoreCreateMutex();

    // Start the animation task
    xTaskCreate(led_animation_task, "led_anim", LED_TASK_STACK_SIZE, NULL, 4, NULL);

    ESP_LOGI(TAG, "LED animation started (%d LEDs, GPIO%d)", NUM_LEDS, PIN_LED_DATA);
    return ESP_OK;
}

void led_animation_set_strategy(const led_strategy_t *strategy)
{
    xSemaphoreTake(strategy_mutex, portMAX_DELAY);
    current_strategy = *strategy;
    xSemaphoreGive(strategy_mutex);
    ESP_LOGI(TAG, "New strategy: %s, RGB(%d,%d,%d), speed=%d",
             led_animation_type_to_str(strategy->animation),
             strategy->r, strategy->g, strategy->b, strategy->speed);
}

led_strategy_t led_animation_get_strategy(void)
{
    xSemaphoreTake(strategy_mutex, portMAX_DELAY);
    led_strategy_t s = current_strategy;
    xSemaphoreGive(strategy_mutex);
    return s;
}

const char* led_animation_type_to_str(animation_type_t type)
{
    switch (type) {
        case ANIM_SOLID:         return "solid";
        case ANIM_BREATHING:     return "breathing";
        case ANIM_RAINBOW_CYCLE: return "rainbow_cycle";
        case ANIM_BLINK:         return "blink";
        case ANIM_WAVE:          return "wave";
        case ANIM_COLOR_WIPE:    return "color_wipe";
        default:                 return "solid";
    }
}

animation_type_t led_animation_str_to_type(const char *str)
{
    if (strcmp(str, "solid") == 0)         return ANIM_SOLID;
    if (strcmp(str, "breathing") == 0)     return ANIM_BREATHING;
    if (strcmp(str, "rainbow_cycle") == 0) return ANIM_RAINBOW_CYCLE;
    if (strcmp(str, "blink") == 0)         return ANIM_BLINK;
    if (strcmp(str, "wave") == 0)          return ANIM_WAVE;
    if (strcmp(str, "color_wipe") == 0)    return ANIM_COLOR_WIPE;
    return ANIM_SOLID;  // Unknown string → default
}
