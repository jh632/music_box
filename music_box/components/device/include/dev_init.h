#pragma once

#include "dev_audio.h"
#include "dev_button.h"
#include "dev_display.h"
#include "dev_led_strip.h"
#include "dev_light_sensor.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dev_audio_handle_t audio;
    dev_led_strip_handle_t led_strip;
    dev_light_sensor_handle_t light_sensor;
    dev_display_handle_t display;
    dev_button_handle_t button;
} dev_handles_t;

esp_err_t dev_init_all(dev_handles_t *out);

#ifdef __cplusplus
}
#endif
