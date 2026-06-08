#include "dev_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_player.h"
#include "esp_log.h"
#include "hal_gpio.h"

static const char *TAG = "DEV_AUDIO";

#define DEV_AUDIO_DEFAULT_VOLUME 60
#define DEV_AUDIO_MAX_VOLUME     100

struct dev_audio_s {
    const hal_i2s_ops_t  *i2s_ops;
    const hal_gpio_ops_t *gpio_ops;
    dev_audio_config_t    cfg;
    uint8_t               volume;
    bool                  i2s_inited;
    bool                  i2s_enabled;
    bool                  player_inited;
    bool                  unsupported_bits_logged;
    uint32_t              current_sample_rate_hz;
    i2s_data_bit_width_t  current_data_bit_width;
    i2s_slot_mode_t       current_slot_mode;
};

static dev_audio_handle_t s_audio_handle;

static bool dev_audio_is_valid_gpio(gpio_num_t pin)
{
    return pin >= 0 && pin < GPIO_NUM_MAX;
}

static bool dev_audio_has_sd_pin(dev_audio_handle_t h)
{
    return h->cfg.sd_pin >= 0;
}

static esp_err_t dev_audio_set_sd(dev_audio_handle_t h, bool enable)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!dev_audio_has_sd_pin(h)) {
        return ESP_OK;
    }

    return h->gpio_ops->set_level(h->cfg.sd_pin, enable ? 1 : 0);
}

static esp_err_t dev_audio_init_sd_pin(dev_audio_handle_t h)
{
    if (!dev_audio_has_sd_pin(h)) {
        return ESP_OK;
    }
    if (!dev_audio_is_valid_gpio(h->cfg.sd_pin)) {
        ESP_LOGE(TAG, "invalid sd pin: %d", h->cfg.sd_pin);
        return ESP_ERR_INVALID_ARG;
    }

    hal_gpio_config_t gpio_cfg = {
        .pin       = h->cfg.sd_pin,
        .dir       = HAL_GPIO_DIR_OUTPUT,
        .pull_up   = false,
        .pull_down = false,
        .intr_type = HAL_GPIO_INTR_DISABLE,
    };

    esp_err_t ret = h->gpio_ops->init(&gpio_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD GPIO %d init failed: %s", h->cfg.sd_pin, esp_err_to_name(ret));
        return ret;
    }

    /* MAX98357A 的 SD 低电平关闭输出，初始化时先保持静音。 */
    ret = dev_audio_set_sd(h, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD GPIO %d set low failed: %s", h->cfg.sd_pin, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t dev_audio_disable_i2s(dev_audio_handle_t h)
{
    if (!h->i2s_enabled) {
        return ESP_OK;
    }

    esp_err_t ret = h->i2s_ops->disable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S disable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    h->i2s_enabled = false;
    return ESP_OK;
}

static esp_err_t dev_audio_deinit_i2s(dev_audio_handle_t h)
{
    esp_err_t ret = dev_audio_disable_i2s(h);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!h->i2s_inited) {
        return ESP_OK;
    }

    ret = h->i2s_ops->deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S deinit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    h->i2s_inited = false;
    return ESP_OK;
}

static esp_err_t dev_audio_config_i2s(dev_audio_handle_t h,
                                      uint32_t sample_rate_hz,
                                      i2s_data_bit_width_t data_bit_width,
                                      i2s_slot_mode_t slot_mode)
{
    if (h == NULL || sample_rate_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (h->i2s_inited && h->current_sample_rate_hz == sample_rate_hz &&
        h->current_data_bit_width == data_bit_width && h->current_slot_mode == slot_mode) {
        if (!h->i2s_enabled) {
            esp_err_t ret = h->i2s_ops->enable();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S enable failed: %s", esp_err_to_name(ret));
                return ret;
            }
            h->i2s_enabled = true;
        }
        return ESP_OK;
    }

    esp_err_t ret = dev_audio_deinit_i2s(h);
    if (ret != ESP_OK) {
        return ret;
    }

    hal_i2s_config_t i2s_cfg = {
        .port            = h->cfg.port,
        .bclk_pin        = h->cfg.bclk_pin,
        .ws_pin          = h->cfg.ws_pin,
        .dout_pin        = h->cfg.dout_pin,
        .mclk_pin        = h->cfg.mclk_pin,
        .sample_rate_hz  = sample_rate_hz,
        .data_bit_width  = data_bit_width,
        .slot_mode       = slot_mode,
        .format          = h->cfg.format,
        .dma_desc_num    = h->cfg.dma_desc_num,
        .dma_frame_num   = h->cfg.dma_frame_num,
    };

    ret = h->i2s_ops->init(&i2s_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    h->i2s_inited = true;

    ret = h->i2s_ops->enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S enable failed: %s", esp_err_to_name(ret));
        (void)h->i2s_ops->deinit();
        h->i2s_inited = false;
        return ret;
    }

    h->i2s_enabled             = true;
    h->current_sample_rate_hz  = sample_rate_hz;
    h->current_data_bit_width  = data_bit_width;
    h->current_slot_mode       = slot_mode;
    h->unsupported_bits_logged = false;
    return ESP_OK;
}

static void dev_audio_apply_volume(dev_audio_handle_t h, void *audio_buffer, size_t len)
{
    uint8_t volume = h->volume;
    if (volume >= DEV_AUDIO_MAX_VOLUME || len == 0) {
        return;
    }

    if (h->current_data_bit_width != I2S_DATA_BIT_WIDTH_16BIT) {
        if (!h->unsupported_bits_logged) {
            ESP_LOGW(TAG,
                     "software volume only supports 16-bit PCM, bits=%d",
                     h->current_data_bit_width);
            h->unsupported_bits_logged = true;
        }
        return;
    }

    if (volume == 0) {
        memset(audio_buffer, 0, len);
        return;
    }

    /* MP3 解码输出为 16-bit PCM，这里直接原地缩放后再交给 I2S。 */
    int16_t *samples = (int16_t *)audio_buffer;
    size_t   count   = len / sizeof(int16_t);
    for (size_t i = 0; i < count; i++) {
        samples[i] = (int16_t)(((int32_t)samples[i] * volume) / DEV_AUDIO_MAX_VOLUME);
    }
}

static esp_err_t dev_audio_player_mute(AUDIO_PLAYER_MUTE_SETTING setting)
{
    dev_audio_handle_t h = s_audio_handle;
    if (h == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return dev_audio_set_sd(h, setting == AUDIO_PLAYER_UNMUTE);
}

static esp_err_t dev_audio_player_set_clock(uint32_t rate,
                                            uint32_t bits_cfg,
                                            i2s_slot_mode_t ch)
{
    dev_audio_handle_t h = s_audio_handle;
    if (h == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return dev_audio_config_i2s(h, rate, (i2s_data_bit_width_t)bits_cfg, ch);
}

static esp_err_t dev_audio_player_write(void *audio_buffer,
                                        size_t len,
                                        size_t *bytes_written,
                                        uint32_t timeout_ms)
{
    dev_audio_handle_t h = s_audio_handle;
    if (h == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (audio_buffer == NULL && len > 0) {
        return ESP_ERR_INVALID_ARG;
    }

    dev_audio_apply_volume(h, audio_buffer, len);
    return h->i2s_ops->write(audio_buffer, len, bytes_written, timeout_ms);
}

static void dev_audio_player_callback(audio_player_cb_ctx_t *ctx)
{
    if (ctx == NULL || ctx->user_ctx == NULL) {
        return;
    }

    dev_audio_handle_t h = (dev_audio_handle_t)ctx->user_ctx;
    switch (ctx->audio_event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
    case AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT:
        (void)dev_audio_set_sd(h, true);
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
    case AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN:
    case AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN_FILE_TYPE:
    default:
        (void)dev_audio_set_sd(h, false);
        break;
    }
}

static dev_audio_state_t dev_audio_map_state(audio_player_state_t state)
{
    switch (state) {
    case AUDIO_PLAYER_STATE_PLAYING:
        return DEV_AUDIO_STATE_PLAYING;
    case AUDIO_PLAYER_STATE_PAUSE:
        return DEV_AUDIO_STATE_PAUSED;
    case AUDIO_PLAYER_STATE_IDLE:
    case AUDIO_PLAYER_STATE_SHUTDOWN:
    default:
        return DEV_AUDIO_STATE_IDLE;
    }
}

static esp_err_t dev_audio_check_handle(dev_audio_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (h != s_audio_handle || !h->player_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

static esp_err_t dev_audio_validate_config(const dev_audio_config_t *cfg)
{
    if (cfg == NULL || cfg->sample_rate_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!dev_audio_is_valid_gpio(cfg->bclk_pin) || !dev_audio_is_valid_gpio(cfg->ws_pin) ||
        !dev_audio_is_valid_gpio(cfg->dout_pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->sd_pin >= 0 && !dev_audio_is_valid_gpio(cfg->sd_pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t dev_audio_init(const dev_audio_config_t *cfg, dev_audio_handle_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = dev_audio_validate_config(cfg);
    if (ret != ESP_OK) {
        return ret;
    }
    if (s_audio_handle != NULL) {
        ESP_LOGE(TAG, "audio already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    dev_audio_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    h->i2s_ops  = hal_i2s_get_ops();
    h->gpio_ops = hal_gpio_get_ops();
    h->cfg      = *cfg;
    h->volume   = DEV_AUDIO_DEFAULT_VOLUME;
    if (h->cfg.data_bit_width == 0) {
        h->cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
    }
    if (h->cfg.slot_mode == 0) {
        h->cfg.slot_mode = I2S_SLOT_MODE_STEREO;
    }

    ret = dev_audio_init_sd_pin(h);
    if (ret != ESP_OK) {
        free(h);
        return ret;
    }

    /* 没有外部 codec 配置通道，默认按 MAX98357A 常用的 16-bit stereo 输出。 */
    ret = dev_audio_config_i2s(h, h->cfg.sample_rate_hz, h->cfg.data_bit_width, h->cfg.slot_mode);
    if (ret != ESP_OK) {
        free(h);
        return ret;
    }

    s_audio_handle = h;

    audio_player_config_t player_cfg = {
        .mute_fn    = dev_audio_player_mute,
        .clk_set_fn = dev_audio_player_set_clock,
        .write_fn   = dev_audio_player_write,
        .priority   = 1,
        .coreID     = 0,
    };

    ret = audio_player_new(player_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio player init failed: %s", esp_err_to_name(ret));
        s_audio_handle = NULL;
        (void)dev_audio_deinit_i2s(h);
        free(h);
        return ret;
    }
    h->player_inited = true;

    ret = audio_player_callback_register(dev_audio_player_callback, h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio callback register failed: %s", esp_err_to_name(ret));
        (void)audio_player_delete();
        s_audio_handle = NULL;
        (void)dev_audio_deinit_i2s(h);
        free(h);
        return ret;
    }

    *out = h;
    ESP_LOGI(TAG, "audio initialized, sample_rate=%lu", (unsigned long)cfg->sample_rate_hz);
    return ESP_OK;
}

static esp_err_t dev_audio_deinit(dev_audio_handle_t h)
{
    esp_err_t ret = dev_audio_check_handle(h);
    if (ret != ESP_OK) {
        return ret;
    }

    (void)dev_audio_set_sd(h, false);
    (void)audio_player_stop();

    ret = audio_player_delete();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio player delete failed: %s", esp_err_to_name(ret));
        return ret;
    }
    h->player_inited = false;
    s_audio_handle   = NULL;

    ret = dev_audio_deinit_i2s(h);
    free(h);
    return ret;
}

static esp_err_t dev_audio_play(dev_audio_handle_t h, const char *filepath)
{
    esp_err_t ret = dev_audio_check_handle(h);
    if (ret != ESP_OK) {
        return ret;
    }
    if (filepath == NULL || filepath[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open audio file failed: %s", filepath);
        return ESP_FAIL;
    }

    ret = audio_player_play(fp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio play failed: %s", esp_err_to_name(ret));
        fclose(fp);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t dev_audio_pause(dev_audio_handle_t h)
{
    esp_err_t ret = dev_audio_check_handle(h);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = audio_player_pause();
    if (ret == ESP_OK) {
        (void)dev_audio_set_sd(h, false);
    }
    return ret;
}

static esp_err_t dev_audio_resume(dev_audio_handle_t h)
{
    esp_err_t ret = dev_audio_check_handle(h);
    if (ret != ESP_OK) {
        return ret;
    }

    audio_player_state_t old_state = audio_player_get_state();
    ret = audio_player_resume();
    if (ret == ESP_OK && old_state == AUDIO_PLAYER_STATE_PAUSE) {
        (void)dev_audio_set_sd(h, true);
    }
    return ret;
}

static esp_err_t dev_audio_stop(dev_audio_handle_t h)
{
    esp_err_t ret = dev_audio_check_handle(h);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = audio_player_stop();
    (void)dev_audio_set_sd(h, false);
    return ret;
}

static esp_err_t dev_audio_set_volume(dev_audio_handle_t h, uint8_t vol)
{
    esp_err_t ret = dev_audio_check_handle(h);
    if (ret != ESP_OK) {
        return ret;
    }

    if (vol > DEV_AUDIO_MAX_VOLUME) {
        ESP_LOGW(TAG, "volume clipped to %d", DEV_AUDIO_MAX_VOLUME);
        vol = DEV_AUDIO_MAX_VOLUME;
    }

    h->volume = vol;
    return ESP_OK;
}

static esp_err_t dev_audio_get_state(dev_audio_handle_t h, dev_audio_state_t *state)
{
    esp_err_t ret = dev_audio_check_handle(h);
    if (ret != ESP_OK) {
        return ret;
    }
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *state = dev_audio_map_state(audio_player_get_state());
    return ESP_OK;
}

static const dev_audio_ops_t audio_ops = {
    .init       = dev_audio_init,
    .deinit     = dev_audio_deinit,
    .play       = dev_audio_play,
    .pause      = dev_audio_pause,
    .resume     = dev_audio_resume,
    .stop       = dev_audio_stop,
    .set_volume = dev_audio_set_volume,
    .get_state  = dev_audio_get_state,
};

const dev_audio_ops_t *dev_audio_get_ops(void)
{
    return &audio_ops;
}
