#include "hal_rmt.h"

static const char *TAG = "HAL_RMT";

struct hal_rmt_tx_s {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    rmt_symbol_word_t bit0;
    rmt_symbol_word_t bit1;
    rmt_symbol_word_t reset;
    bool msb_first;
    bool enabled;
};

static uint16_t s_ns_to_ticks(uint32_t ns, uint32_t resolution_hz)
{
    uint64_t ticks = ((uint64_t)ns * resolution_hz + 999999999ULL) / 1000000000ULL;
    if (ticks == 0) {
        ticks = 1;
    }
    if (ticks > 32767) {
        ticks = 32767;
    }
    return (uint16_t)ticks;
}

static size_t s_encode_byte_stream(const void *data,
                                   size_t data_size,
                                   size_t symbols_written,
                                   size_t symbols_free,
                                   rmt_symbol_word_t *symbols,
                                   bool *done,
                                   void *arg)
{
    hal_rmt_tx_handle_t h = (hal_rmt_tx_handle_t)arg;
    if (h == NULL || symbols_free < 8) {
        return 0;
    }

    size_t data_pos = symbols_written / 8;
    const uint8_t *bytes = (const uint8_t *)data;
    if (data_pos < data_size) {
        uint8_t byte = bytes[data_pos];
        for (size_t i = 0; i < 8; i++) {
            uint8_t bit_index = h->msb_first ? (7 - i) : i;
            symbols[i] = (byte & (1U << bit_index)) ? h->bit1 : h->bit0;
        }
        return 8;
    }

    symbols[0] = h->reset;
    *done = true;
    return 1;
}

esp_err_t hal_rmt_tx_init(const hal_rmt_tx_config_t *cfg, hal_rmt_tx_handle_t *out)
{
    if (cfg == NULL || out == NULL || cfg->resolution_hz == 0) {
        ESP_LOGE(TAG, "invalid tx init arg");
        return ESP_ERR_INVALID_ARG;
    }

    hal_rmt_tx_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "OOM");
        return ESP_ERR_NO_MEM;
    }

    h->bit0 = (rmt_symbol_word_t) {
        .level0 = 1,
        .duration0 = s_ns_to_ticks(cfg->bit0_high_ns, cfg->resolution_hz),
        .level1 = 0,
        .duration1 = s_ns_to_ticks(cfg->bit0_low_ns, cfg->resolution_hz),
    };
    h->bit1 = (rmt_symbol_word_t) {
        .level0 = 1,
        .duration0 = s_ns_to_ticks(cfg->bit1_high_ns, cfg->resolution_hz),
        .level1 = 0,
        .duration1 = s_ns_to_ticks(cfg->bit1_low_ns, cfg->resolution_hz),
    };
    uint16_t reset_ticks = s_ns_to_ticks(cfg->reset_ns, cfg->resolution_hz);
    if (reset_ticks < 2) {
        reset_ticks = 2;
    }
    h->reset = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks / 2,
        .level1 = 0,
        .duration1 = reset_ticks - (reset_ticks / 2),
    };
    h->msb_first = cfg->msb_first;

    rmt_tx_channel_config_t tx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = cfg->gpio_num,
        .mem_block_symbols = cfg->mem_block_symbols,
        .resolution_hz = cfg->resolution_hz,
        .trans_queue_depth = cfg->trans_queue_depth,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_cfg, &h->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new tx channel failed: %s", esp_err_to_name(ret));
        free(h);
        return ret;
    }

    rmt_simple_encoder_config_t encoder_cfg = {
        .callback = s_encode_byte_stream,
        .arg = h,
    };
    ret = rmt_new_simple_encoder(&encoder_cfg, &h->encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new encoder failed: %s", esp_err_to_name(ret));
        rmt_del_channel(h->channel);
        free(h);
        return ret;
    }

    *out = h;
    return ESP_OK;
}

esp_err_t hal_rmt_tx_deinit(hal_rmt_tx_handle_t h)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (h->enabled) {
        esp_err_t ret = rmt_disable(h->channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "disable before deinit failed: %s", esp_err_to_name(ret));
            return ret;
        }
        h->enabled = false;
    }

    esp_err_t ret = rmt_del_encoder(h->encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete encoder failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = rmt_del_channel(h->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    free(h);
    return ESP_OK;
}

esp_err_t hal_rmt_tx_enable(hal_rmt_tx_handle_t h)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = rmt_enable(h->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    h->enabled = true;
    return ESP_OK;
}

esp_err_t hal_rmt_tx_disable(hal_rmt_tx_handle_t h)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = rmt_disable(h->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "disable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    h->enabled = false;
    return ESP_OK;
}

esp_err_t hal_rmt_tx_transmit(hal_rmt_tx_handle_t h,
                              const void *data,
                              size_t len,
                              int loop_count)
{
    if (h == NULL || (data == NULL && len > 0)) {
        ESP_LOGE(TAG, "invalid transmit arg");
        return ESP_ERR_INVALID_ARG;
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count = loop_count,
    };

    esp_err_t ret = rmt_transmit(h->channel, h->encoder, data, len, &tx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "transmit failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hal_rmt_tx_wait_done(hal_rmt_tx_handle_t h, int timeout_ms)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = rmt_tx_wait_all_done(h->channel, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wait done failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
