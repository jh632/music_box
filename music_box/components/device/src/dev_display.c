#include "dev_display.h"

#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp32_hw_i2c.h"
#include "u8g2.h"

static const char *TAG = "DEV_DISPLAY";

struct dev_display_s {
    u8g2_t               u8g2;
    u8g2_esp32_i2c_ctx_t i2c_ctx;
    bool                 initialized;
};

static esp_err_t dev_display_init(const dev_display_config_t *cfg,
                                  dev_display_handle_t       *out_handle)
{
    if (cfg == NULL || out_handle == NULL || cfg->width == 0 || cfg->height == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    dev_display_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    /* 填充 I2C 上下文 */
    h->i2c_ctx = (u8g2_esp32_i2c_ctx_t){
        .cfg = {
            .i2c_port      = cfg->i2c_port,
            .sda_pin       = cfg->sda_pin,
            .scl_pin       = cfg->scl_pin,
            .clk_hz        = cfg->scl_speed_hz,
            .dev_addr_7bit = cfg->device_address,
            .timeout_ms    = 1000,
            .reset_pin     = -1,
        },
    };

    u8g2_esp32_i2c_set_default_context(&h->i2c_ctx);

    /* 注：如需适配其他尺寸/控制器，替换此处 setup 函数即可 */
    u8g2_Setup_ssd1306_128x64_noname_f(
        &h->u8g2,
        U8G2_R0,
        u8x8_byte_esp32_hw_i2c,
        u8x8_gpio_and_delay_esp32_i2c);

    u8g2_InitDisplay(&h->u8g2);
    u8g2_SetPowerSave(&h->u8g2, 0); /* 唤醒 */

    h->initialized = true;
    *out_handle = h;

    ESP_LOGI(TAG, "Display initialized (%dx%d, addr 0x%02X)",
             cfg->width, cfg->height, cfg->device_address);
    return ESP_OK;
}

static esp_err_t dev_display_deinit(dev_display_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (h->initialized) {
        u8g2_SetPowerSave(&h->u8g2, 1);
    }
    free(h);
    return ESP_OK;
}

static esp_err_t dev_display_clear(dev_display_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    u8g2_ClearBuffer(&h->u8g2);
    u8g2_SendBuffer(&h->u8g2);
    return ESP_OK;
}

static esp_err_t dev_display_show_text(dev_display_handle_t h,
                                       const char           *line1,
                                       const char           *line2)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    u8g2_ClearBuffer(&h->u8g2);
    u8g2_SetFont(&h->u8g2, u8g2_font_helvR10_tr);

    if (line1 != NULL) {
        u8g2_DrawStr(&h->u8g2, 0, 12, line1);
    }
    if (line2 != NULL) {
        u8g2_DrawStr(&h->u8g2, 0, 28, line2);
    }
    u8g2_SendBuffer(&h->u8g2);
    return ESP_OK;
}

static esp_err_t dev_display_set_contrast(dev_display_handle_t h,
                                          uint8_t              contrast)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    u8g2_SetContrast(&h->u8g2, contrast);
    return ESP_OK;
}

static esp_err_t dev_display_display_on(dev_display_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    u8g2_SetPowerSave(&h->u8g2, 0);
    return ESP_OK;
}

static esp_err_t dev_display_display_off(dev_display_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    u8g2_SetPowerSave(&h->u8g2, 1);
    return ESP_OK;
}

static const dev_display_ops_t display_ops = {
    .init         = dev_display_init,
    .deinit       = dev_display_deinit,
    .clear        = dev_display_clear,
    .show_text    = dev_display_show_text,
    .set_contrast = dev_display_set_contrast,
    .display_on   = dev_display_display_on,
    .display_off  = dev_display_display_off,
};

const dev_display_ops_t *dev_display_get_ops(void)
{
    return &display_ops;
}
