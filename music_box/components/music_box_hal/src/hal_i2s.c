/*
 * hal_i2s.c — HAL 层 I2S standard TX 封装实现
 */

#include "hal_i2s.h"

static const char *TAG = "HAL_I2S";

struct hal_i2s_s {
    i2s_chan_handle_t tx_chan;
    bool enabled;
};

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

esp_err_t hal_i2s_init(const hal_i2s_config_t *cfg, hal_i2s_handle_t *out)
{
    if (cfg == NULL || out == NULL || cfg->sample_rate_hz == 0) {
        ESP_LOGE(TAG, "invalid init arg");
        return ESP_ERR_INVALID_ARG;
    }

    hal_i2s_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "OOM");
        return ESP_ERR_NO_MEM;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(cfg->port, I2S_ROLE_MASTER);
    if (cfg->dma_desc_num > 0) {
        chan_cfg.dma_desc_num = cfg->dma_desc_num;
    }
    if (cfg->dma_frame_num > 0) {
        chan_cfg.dma_frame_num = cfg->dma_frame_num;
    }

    esp_err_t ret = i2s_new_channel(&chan_cfg, &h->tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new channel failed: %s", esp_err_to_name(ret));
        free(h);
        return ret;
    }

    i2s_std_slot_config_t slot_cfg = {0};
    ret = s_make_slot_config(cfg, &slot_cfg);
    if (ret != ESP_OK) {
        i2s_del_channel(h->tx_chan);
        free(h);
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

    ret = i2s_channel_init_std_mode(h->tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init std mode failed: %s", esp_err_to_name(ret));
        i2s_del_channel(h->tx_chan);
        free(h);
        return ret;
    }

    *out = h;
    return ESP_OK;
}

esp_err_t hal_i2s_deinit(hal_i2s_handle_t h)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (h->enabled) {
        esp_err_t ret = i2s_channel_disable(h->tx_chan);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "disable before deinit failed: %s", esp_err_to_name(ret));
            return ret;
        }
        h->enabled = false;
    }

    esp_err_t ret = i2s_del_channel(h->tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    free(h);
    return ESP_OK;
}

esp_err_t hal_i2s_enable(hal_i2s_handle_t h)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2s_channel_enable(h->tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    h->enabled = true;
    return ESP_OK;
}

esp_err_t hal_i2s_disable(hal_i2s_handle_t h)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2s_channel_disable(h->tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "disable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    h->enabled = false;
    return ESP_OK;
}

esp_err_t hal_i2s_write(hal_i2s_handle_t h,
                        const void *data,
                        size_t size,
                        size_t *bytes_written,
                        uint32_t timeout_ms)
{
    if (h == NULL || (data == NULL && size > 0)) {
        ESP_LOGE(TAG, "invalid write arg");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2s_channel_write(h->tx_chan, data, size, bytes_written, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hal_i2s_preload(hal_i2s_handle_t h,
                          const void *data,
                          size_t size,
                          size_t *bytes_loaded)
{
    if (h == NULL || (data == NULL && size > 0)) {
        ESP_LOGE(TAG, "invalid preload arg");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2s_channel_preload_data(h->tx_chan, data, size, bytes_loaded);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "preload failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static const hal_i2s_ops_t s_hal_i2s_ops = {
    .init = hal_i2s_init,
    .deinit = hal_i2s_deinit,
    .enable = hal_i2s_enable,
    .disable = hal_i2s_disable,
    .write = hal_i2s_write,
    .preload = hal_i2s_preload,
};

const hal_i2s_ops_t *hal_i2s_get_ops(void)
{
    return &s_hal_i2s_ops;
}
