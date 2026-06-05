#include "hal_gpio.h"
#include "esp_log.h"

static const char *TAG = "HAL_GPIO";

static gpio_int_type_t s_map_intr_type(hal_gpio_intr_type_t intr_type)
{
    switch (intr_type) {
    case HAL_GPIO_INTR_DISABLE:
        return GPIO_INTR_DISABLE;
    case HAL_GPIO_INTR_POSEDGE:
        return GPIO_INTR_POSEDGE;
    case HAL_GPIO_INTR_NEGEDGE:
        return GPIO_INTR_NEGEDGE;
    case HAL_GPIO_INTR_ANYEDGE:
        return GPIO_INTR_ANYEDGE;
    case HAL_GPIO_INTR_LOW_LEVEL:
        return GPIO_INTR_LOW_LEVEL;
    case HAL_GPIO_INTR_HIGH_LEVEL:
        return GPIO_INTR_HIGH_LEVEL;
    default:
        return GPIO_INTR_DISABLE;
    }
}

static esp_err_t s_apply_config(const hal_gpio_config_t *cfg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << cfg->pin),
        .pull_up_en = cfg->pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = cfg->pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
    };

    switch (cfg->dir) {
    case HAL_GPIO_DIR_OUTPUT:
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        break;

    case HAL_GPIO_DIR_INPUT:
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        break;

    case HAL_GPIO_DIR_INPUT_INTR:
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = s_map_intr_type(cfg->intr_type);
        break;

    default:
        ESP_LOGE(TAG, "unknown dir: %d", cfg->dir);
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_config(&io_conf);
}

static esp_err_t s_hal_gpio_init(const hal_gpio_config_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = s_apply_config(cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed for pin %d: %s", cfg->pin, esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

static esp_err_t s_hal_gpio_set_level(gpio_num_t pin, uint32_t level)
{
    esp_err_t ret = gpio_set_level(pin, level);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_level pin %d failed: %s", pin, esp_err_to_name(ret));
    }
    return ret;
}

static int s_hal_gpio_get_level(gpio_num_t pin)
{
    return gpio_get_level(pin);
}

static esp_err_t s_hal_gpio_isr_service_install(void)
{
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "install ISR service failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_gpio_isr_handler_add(gpio_num_t pin, hal_gpio_isr_cb_t cb, void *arg)
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

static esp_err_t s_hal_gpio_isr_handler_remove(gpio_num_t pin)
{
    esp_err_t ret = gpio_isr_handler_remove(pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "isr_handler_remove pin %d failed: %s", pin, esp_err_to_name(ret));
    }
    return ret;
}

static const hal_gpio_ops_t s_hal_gpio_ops = {
    .init = s_hal_gpio_init,
    .set_level = s_hal_gpio_set_level,
    .get_level = s_hal_gpio_get_level,
    .isr_service_install = s_hal_gpio_isr_service_install,
    .isr_handler_add = s_hal_gpio_isr_handler_add,
    .isr_handler_remove = s_hal_gpio_isr_handler_remove,
};

const hal_gpio_ops_t *hal_gpio_get_ops(void)
{
    return &s_hal_gpio_ops;
}
