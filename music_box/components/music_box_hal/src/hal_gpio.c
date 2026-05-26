#include "hal_gpio.h"

static const char *TAG = "HAL_GPIO";

/* ------------------------------------------------------------------ */
/*  内部辅助                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 将 hal_gpio_config_t 转换为 ESP-IDF gpio_config_t 并调用 gpio_config()
 */
static esp_err_t s_apply_config(const hal_gpio_config_t *cfg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << cfg->pin),
        .pull_up_en   = cfg->pull_up   ? GPIO_PULLUP_ENABLE   : GPIO_PULLUP_DISABLE,
        .pull_down_en = cfg->pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
    };

    switch (cfg->dir) {
    case HAL_GPIO_DIR_OUTPUT:
        io_conf.mode      = GPIO_MODE_OUTPUT;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        break;

    case HAL_GPIO_DIR_INPUT:
        io_conf.mode      = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        break;

    case HAL_GPIO_DIR_INPUT_INTR:
        io_conf.mode      = GPIO_MODE_INPUT;
        io_conf.intr_type = (gpio_int_type_t)cfg->intr_type;
        break;

    default:
        ESP_LOGE(TAG, "unknown dir: %d", cfg->dir);
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_config(&io_conf);
}

/* ------------------------------------------------------------------ */
/*  公开接口实现                                                       */
/* ------------------------------------------------------------------ */

esp_err_t hal_gpio_init(const hal_gpio_config_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = s_apply_config(cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed for pin %d: %s",
                 cfg->pin, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "pin %d initialized (dir=%d, pull_up=%d, pull_down=%d, intr=%d)",
             cfg->pin, cfg->dir, cfg->pull_up, cfg->pull_down, cfg->intr_type);
    return ESP_OK;
}

esp_err_t hal_gpio_set_level(gpio_num_t pin, uint32_t level)
{
    esp_err_t ret = gpio_set_level(pin, level);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_level pin %d failed: %s", pin, esp_err_to_name(ret));
    }
    return ret;
}

int hal_gpio_get_level(gpio_num_t pin)
{
    return gpio_get_level(pin);
}

esp_err_t hal_gpio_isr_service_install(void)
{
    /* ESP_ERR_INVALID_STATE 表示服务已安装，视为正常 */
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "ISR service already installed, skip");
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "install ISR service failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hal_gpio_isr_handler_add(gpio_num_t pin, hal_gpio_isr_cb_t cb, void *arg)
{
    if (cb == NULL) {
        ESP_LOGE(TAG, "cb is NULL for pin %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = gpio_isr_handler_add(pin, cb, arg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "isr_handler_add pin %d failed: %s", pin, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hal_gpio_isr_handler_remove(gpio_num_t pin)
{
    esp_err_t ret = gpio_isr_handler_remove(pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "isr_handler_remove pin %d failed: %s", pin, esp_err_to_name(ret));
    }
    return ret;
}
