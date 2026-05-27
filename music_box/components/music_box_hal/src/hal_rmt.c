#include "hal_rmt.h"

#include <string.h>
#include "driver/rmt_common.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

static const char *TAG = "HAL_RMT";

typedef struct {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    rmt_symbol_word_t bit0;
    rmt_symbol_word_t bit1;
    rmt_symbol_word_t reset;
    bool msb_first;
    bool enabled;
    bool inited;
} hal_rmt_ctx_t;

static hal_rmt_ctx_t s_rmt;

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
    hal_rmt_ctx_t *ctx = (hal_rmt_ctx_t *)arg;
    if (ctx == NULL || symbols_free < 8) {
        return 0;
    }

    size_t data_pos = symbols_written / 8;
    const uint8_t *bytes = (const uint8_t *)data;
    if (data_pos < data_size) {
        uint8_t byte = bytes[data_pos];
        for (size_t i = 0; i < 8; i++) {
            uint8_t bit_index = ctx->msb_first ? (7 - i) : i;
            symbols[i] = (byte & (1U << bit_index)) ? ctx->bit1 : ctx->bit0;
        }
        return 8;
    }

    symbols[0] = ctx->reset;
    *done = true;
    return 1;
}

static esp_err_t s_hal_rmt_tx_init(const hal_rmt_tx_config_t *cfg)
{
    if (cfg == NULL || cfg->resolution_hz == 0) {
        ESP_LOGE(TAG, "invalid tx init arg");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_rmt.inited) {
        ESP_LOGE(TAG, "RMT already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_rmt, 0, sizeof(s_rmt));

    s_rmt.bit0 = (rmt_symbol_word_t) {
        .level0 = 1,
        .duration0 = s_ns_to_ticks(cfg->bit0_high_ns, cfg->resolution_hz),
        .level1 = 0,
        .duration1 = s_ns_to_ticks(cfg->bit0_low_ns, cfg->resolution_hz),
    };
    s_rmt.bit1 = (rmt_symbol_word_t) {
        .level0 = 1,
        .duration0 = s_ns_to_ticks(cfg->bit1_high_ns, cfg->resolution_hz),
        .level1 = 0,
        .duration1 = s_ns_to_ticks(cfg->bit1_low_ns, cfg->resolution_hz),
    };

    uint16_t reset_ticks = s_ns_to_ticks(cfg->reset_ns, cfg->resolution_hz);
    if (reset_ticks < 2) {
        reset_ticks = 2;
    }
    s_rmt.reset = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks / 2,
        .level1 = 0,
        .duration1 = reset_ticks - (reset_ticks / 2),
    };
    s_rmt.msb_first = cfg->msb_first;

    rmt_tx_channel_config_t tx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = cfg->gpio_num,
        .mem_block_symbols = cfg->mem_block_symbols,
        .resolution_hz = cfg->resolution_hz,
        .trans_queue_depth = cfg->trans_queue_depth,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_cfg, &s_rmt.channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new tx channel failed: %s", esp_err_to_name(ret));
        memset(&s_rmt, 0, sizeof(s_rmt));
        return ret;
    }

    rmt_simple_encoder_config_t encoder_cfg = {
        .callback = s_encode_byte_stream,
        .arg = &s_rmt,
    };
    ret = rmt_new_simple_encoder(&encoder_cfg, &s_rmt.encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new encoder failed: %s", esp_err_to_name(ret));
        rmt_del_channel(s_rmt.channel);
        memset(&s_rmt, 0, sizeof(s_rmt));
        return ret;
    }

    s_rmt.inited = true;
    return ESP_OK;
}

static esp_err_t s_hal_rmt_tx_deinit(void)
{
    if (!s_rmt.inited) {
        ESP_LOGE(TAG, "RMT is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_rmt.enabled) {
        esp_err_t ret = rmt_disable(s_rmt.channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "disable before deinit failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_rmt.enabled = false;
    }

    esp_err_t ret = rmt_del_encoder(s_rmt.encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete encoder failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = rmt_del_channel(s_rmt.channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    memset(&s_rmt, 0, sizeof(s_rmt));
    return ESP_OK;
}

static esp_err_t s_hal_rmt_tx_enable(void)
{
    if (!s_rmt.inited) {
        ESP_LOGE(TAG, "RMT is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = rmt_enable(s_rmt.channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_rmt.enabled = true;
    return ESP_OK;
}

static esp_err_t s_hal_rmt_tx_disable(void)
{
    if (!s_rmt.inited) {
        ESP_LOGE(TAG, "RMT is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = rmt_disable(s_rmt.channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "disable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_rmt.enabled = false;
    return ESP_OK;
}

static esp_err_t s_hal_rmt_tx_transmit(const void *data, size_t len, int loop_count)
{
    if (!s_rmt.inited || (data == NULL && len > 0)) {
        ESP_LOGE(TAG, "invalid transmit arg");
        return ESP_ERR_INVALID_ARG;
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count = loop_count,
    };

    esp_err_t ret = rmt_transmit(s_rmt.channel, s_rmt.encoder, data, len, &tx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "transmit failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_rmt_tx_wait_done(int timeout_ms)
{
    if (!s_rmt.inited) {
        ESP_LOGE(TAG, "RMT is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = rmt_tx_wait_all_done(s_rmt.channel, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wait done failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static const hal_rmt_ops_t s_hal_rmt_ops = {
    .tx_init = s_hal_rmt_tx_init,
    .tx_deinit = s_hal_rmt_tx_deinit,
    .tx_enable = s_hal_rmt_tx_enable,
    .tx_disable = s_hal_rmt_tx_disable,
    .tx_transmit = s_hal_rmt_tx_transmit,
    .tx_wait_done = s_hal_rmt_tx_wait_done,
};

const hal_rmt_ops_t *hal_rmt_get_ops(void)
{
    return &s_hal_rmt_ops;
}
