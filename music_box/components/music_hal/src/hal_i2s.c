#include "hal_i2s.h"

#include "esp_log.h"

static const char *TAG = "HAL_I2S";

static i2s_chan_handle_t s_tx_chan;
static bool s_enabled;

static esp_err_t s_make_slot_config(const hal_i2s_config_t *cfg,
                                    i2s_std_slot_config_t *slot_cfg)
{
    if (cfg == NULL || slot_cfg == NULL) {
        ESP_LOGE(TAG, "invalid slot config arg");
        return ESP_ERR_INVALID_ARG;
    }

    switch (cfg->format) {
    case HAL_I2S_STD_FMT_PHILIPS:
        *slot_cfg = (i2s_std_slot_config_t)I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(cfg->data_bit_width,
                                                                               cfg->slot_mode);
        break;
    case HAL_I2S_STD_FMT_MSB:
        *slot_cfg = (i2s_std_slot_config_t)I2S_STD_MSB_SLOT_DEFAULT_CONFIG(cfg->data_bit_width,
                                                                           cfg->slot_mode);
        break;
    default:
        ESP_LOGE(TAG, "unknown format: %d", cfg->format);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t s_hal_i2s_init(const hal_i2s_config_t *cfg)
{
    if (cfg == NULL || cfg->sample_rate_hz == 0) {
        ESP_LOGE(TAG, "invalid init arg");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_tx_chan != NULL) {
        ESP_LOGE(TAG, "I2S already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(cfg->port, I2S_ROLE_MASTER);
    if (cfg->dma_desc_num > 0) {
        chan_cfg.dma_desc_num = cfg->dma_desc_num;
    }
    if (cfg->dma_frame_num > 0) {
        chan_cfg.dma_frame_num = cfg->dma_frame_num;
    }

    i2s_chan_handle_t tx_chan = NULL;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_slot_config_t slot_cfg = {0};
    ret = s_make_slot_config(cfg, &slot_cfg);
    if (ret != ESP_OK) {
        i2s_del_channel(tx_chan);
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate_hz),
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .mclk = cfg->mclk_pin,
            .bclk = cfg->bclk_pin,
            .ws = cfg->ws_pin,
            .dout = cfg->dout_pin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init std mode failed: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_chan);
        return ret;
    }

    s_tx_chan = tx_chan;
    s_enabled = false;
    return ESP_OK;
}

static esp_err_t s_hal_i2s_deinit(void)
{
    if (s_tx_chan == NULL) {
        ESP_LOGE(TAG, "I2S is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_enabled) {
        esp_err_t ret = i2s_channel_disable(s_tx_chan);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "disable before deinit failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_enabled = false;
    }

    esp_err_t ret = i2s_del_channel(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_tx_chan = NULL;
    return ESP_OK;
}

static esp_err_t s_hal_i2s_enable(void)
{
    if (s_tx_chan == NULL) {
        ESP_LOGE(TAG, "I2S is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_enabled = true;
    return ESP_OK;
}

static esp_err_t s_hal_i2s_disable(void)
{
    if (s_tx_chan == NULL) {
        ESP_LOGE(TAG, "I2S is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_disable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "disable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_enabled = false;
    return ESP_OK;
}

static esp_err_t s_hal_i2s_write(const void *data,
                                 size_t size,
                                 size_t *bytes_written,
                                 uint32_t timeout_ms)
{
    if (s_tx_chan == NULL || (data == NULL && size > 0)) {
        ESP_LOGE(TAG, "invalid write arg");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2s_channel_write(s_tx_chan, data, size, bytes_written, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_i2s_preload(const void *data,
                                   size_t size,
                                   size_t *bytes_loaded)
{
    if (s_tx_chan == NULL || (data == NULL && size > 0)) {
        ESP_LOGE(TAG, "invalid preload arg");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2s_channel_preload_data(s_tx_chan, data, size, bytes_loaded);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "preload failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static const hal_i2s_ops_t s_hal_i2s_ops = {
    .init = s_hal_i2s_init,
    .deinit = s_hal_i2s_deinit,
    .enable = s_hal_i2s_enable,
    .disable = s_hal_i2s_disable,
    .write = s_hal_i2s_write,
    .preload = s_hal_i2s_preload,
};

const hal_i2s_ops_t *hal_i2s_get_ops(void)
{
    return &s_hal_i2s_ops;
}
