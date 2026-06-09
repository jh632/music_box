#include "dev_led_strip.h"

static const char *TAG = "DEV_LED_STRIP";

struct dev_led_strip_s {
    const hal_gpio_ops_t *gpio_ops;
    gpio_num_t           *gpio_nums;
    bool                 *states;
    uint32_t              led_count;
    uint8_t               active_level;
    uint32_t              chase_pos; // 流水灯当前位置索引
};

/**
 * @brief 设置实际有效电平
 *
 * @param h LED 设备句柄
 * @param on 是否点亮
 *
 * @return 实际输出电平，0 或 1
 */
static uint32_t led_set_active_level(dev_led_strip_handle_t h, bool on)
{
    /* 有效电平换算：on=true 时输出 active_level，否则输出反相电平 */
    bool active;
    if (h->active_level != 0)
    {
        active = true;
    }
    else
    {
        active = false;
    }

    return (on == active) ? 1 : 0;
}

/**
 * @brief 写入单个 LED 的开关状态
 *
 * @param h LED 设备句柄
 * @param index LED 索引
 * @param on 是否点亮
 *
 * @return esp_err_t
 */
static esp_err_t write_led(dev_led_strip_handle_t h, uint32_t index, bool on)
{
    if (h == NULL || index >= h->led_count) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = h->gpio_ops->set_level(h->gpio_nums[index], led_set_active_level(h, on));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d set level failed", h->gpio_nums[index]);
        return ret;
    }

    h->states[index] = on;
    return ESP_OK;
}

/**
 * @brief 初始化多路 LED 设备
 *
 * @param cfg LED 配置
 * @param out 输出 LED 设备句柄
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_init(const dev_led_strip_config_t *cfg, dev_led_strip_handle_t *out)
{
    if (cfg == NULL || out == NULL || cfg->gpio_nums == NULL || cfg->led_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    dev_led_strip_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    h->gpio_nums = calloc(cfg->led_count, sizeof(*h->gpio_nums));
    h->states = calloc(cfg->led_count, sizeof(*h->states));
    if (h->gpio_nums == NULL || h->states == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        free(h->states);
        free(h->gpio_nums);
        free(h);
        return ESP_ERR_NO_MEM;
    }

    h->gpio_ops     = hal_gpio_get_ops();
    h->led_count    = cfg->led_count;
    h->active_level = cfg->active_level;

    for (uint32_t i = 0; i < h->led_count; i++) {
        if (cfg->gpio_nums[i] < 0 || cfg->gpio_nums[i] >= GPIO_NUM_MAX) {
            free(h->states);
            free(h->gpio_nums);
            free(h);
            return ESP_ERR_INVALID_ARG;
        }

        h->gpio_nums[i] = cfg->gpio_nums[i];
        hal_gpio_config_t gpio_cfg = {
            .pin       = h->gpio_nums[i],
            .dir       = HAL_GPIO_DIR_OUTPUT,
            .pull_up   = false,
            .pull_down = false,
            .intr_type = HAL_GPIO_INTR_DISABLE,
        };

        esp_err_t ret = h->gpio_ops->init(&gpio_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "GPIO %d init failed", h->gpio_nums[i]);
            free(h->states);
            free(h->gpio_nums);
            free(h);
            return ret;
        }

        ret = write_led(h, i, false);
        if (ret != ESP_OK) {
            free(h->states);
            free(h->gpio_nums);
            free(h);
            return ret;
        }
    }

    *out = h;
    ESP_LOGI(TAG, "LED strip created, count=%lu", (unsigned long)h->led_count);
    return ESP_OK;
}

/**
 * @brief 释放 LED 设备并关闭所有 LED
 *
 * @param h LED 设备句柄
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_deinit(dev_led_strip_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < h->led_count; i++) {
        (void)write_led(h, i, false);
    }

    free(h->states);
    free(h->gpio_nums);
    free(h);
    return ESP_OK;
}

/**
 * @brief 点亮指定 LED
 *
 * @param h LED 设备句柄
 * @param index LED 索引
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_on(dev_led_strip_handle_t h, uint32_t index)
{
    return write_led(h, index, true);
}

/**
 * @brief 熄灭指定 LED
 *
 * @param h LED 设备句柄
 * @param index LED 索引
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_off(dev_led_strip_handle_t h, uint32_t index)
{
    return write_led(h, index, false);
}

/**
 * @brief 设置指定 LED 的开关状态
 *
 * @param h LED 设备句柄
 * @param index LED 索引
 * @param on 是否点亮
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_set(dev_led_strip_handle_t h, uint32_t index, bool on)
{
    return write_led(h, index, on);
}

/**
 * @brief 点亮全部 LED
 *
 * @param h LED 设备句柄
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_all_on(dev_led_strip_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < h->led_count; i++) {
        esp_err_t ret = write_led(h, i, true);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

/**
 * @brief 熄灭全部 LED
 *
 * @param h LED 设备句柄
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_all_off(dev_led_strip_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < h->led_count; i++) {
        esp_err_t ret = write_led(h, i, false);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

/**
 * @brief 翻转指定 LED 的开关状态
 *
 * @param h LED 设备句柄
 * @param index LED 索引
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_toggle(dev_led_strip_handle_t h, uint32_t index)
{
    if (h == NULL || index >= h->led_count) {
        return ESP_ERR_INVALID_ARG;
    }

    return write_led(h, index, !h->states[index]);
}

/**
 * @brief 推进一次流水灯效果
 *
 * @param h LED 设备句柄
 *
 * @return esp_err_t
 */
static esp_err_t dev_led_strip_chase_step(dev_led_strip_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < h->led_count; i++) {
        /* 流水灯当前位置：每次只点亮 chase_pos 对应的 LED */
        esp_err_t ret = write_led(h, i, i == h->chase_pos);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    h->chase_pos = (h->chase_pos + 1) % h->led_count;
    return ESP_OK;
}

static const dev_led_strip_ops_t led_strip_ops = {
    .init       = dev_led_strip_init,
    .deinit     = dev_led_strip_deinit,
    .on         = dev_led_strip_on,
    .off        = dev_led_strip_off,
    .set        = dev_led_strip_set,
    .all_on     = dev_led_strip_all_on,
    .all_off    = dev_led_strip_all_off,
    .toggle     = dev_led_strip_toggle,
    .chase_step = dev_led_strip_chase_step,
};

const dev_led_strip_ops_t *dev_led_strip_get_ops(void)
{
    return &led_strip_ops;
}
