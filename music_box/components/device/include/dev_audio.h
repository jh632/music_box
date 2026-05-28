#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "hal_i2s.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_audio_s *dev_audio_handle_t;

typedef enum {
    DEV_AUDIO_STATE_IDLE = 0,
    DEV_AUDIO_STATE_PLAYING,
    DEV_AUDIO_STATE_PAUSED,
} dev_audio_state_t;

typedef struct {
    i2s_port_t port;
    gpio_num_t bclk_pin;
    gpio_num_t ws_pin;
    gpio_num_t dout_pin;
    gpio_num_t mclk_pin;
    gpio_num_t sd_pin;
    uint32_t sample_rate_hz;
    i2s_data_bit_width_t data_bit_width;
    i2s_slot_mode_t slot_mode;
    hal_i2s_std_format_t format;
    uint32_t dma_desc_num;
    uint32_t dma_frame_num;
} dev_audio_config_t;

typedef struct {
    esp_err_t (*init)(const dev_audio_config_t *cfg, dev_audio_handle_t *out);
    esp_err_t (*deinit)(dev_audio_handle_t h);
    esp_err_t (*play)(dev_audio_handle_t h, const char *filepath);
    esp_err_t (*pause)(dev_audio_handle_t h);
    esp_err_t (*resume)(dev_audio_handle_t h);
    esp_err_t (*stop)(dev_audio_handle_t h);
    esp_err_t (*set_volume)(dev_audio_handle_t h, uint8_t vol);
    esp_err_t (*get_state)(dev_audio_handle_t h, dev_audio_state_t *state);
} dev_audio_ops_t;

const dev_audio_ops_t *dev_audio_get_ops(void);

#ifdef __cplusplus
}
#endif
