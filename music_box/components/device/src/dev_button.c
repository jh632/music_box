#include "dev_button.h"

#include "multi_button.h"
#include "esp_log.h"
#include "hal_gpio.h"
#include "hal_timer.h"

#include <stdlib.h>

static const char *TAG = "DEV_BUTTON";

/* VERY_LONG 阈值（ms） */
#define DEV_BTN_VERY_LONG_MS  5000

struct dev_button_s {
    Button                 mb_btn;          /* multibutton 实例（必须是第一个成员） */
    dev_btn_callback_t     user_cb;         /* 用户回调 */
    void                  *user_data;       /* 用户上下文 */
    hal_timer_handle_t     timer;           /* 5ms 周期定时器（驱动 button_ticks） */
    bool                   very_long_fired; /* VERY_LONG_HOLD 是否已触发 */
};

static uint8_t dev_button_pin_level(uint8_t button_id)
{
    int level = hal_gpio_get_ops()->get_level((gpio_num_t)button_id);
    return (level != 0) ? 1 : 0;
}

static void dev_button_timer_cb(void *arg)
{
    (void)arg;
    button_ticks();
}

static void dev_button_multibutton_cb(Button *mb, void *user_data)
{
    (void)user_data;

    /* Button 是 struct dev_button_s 的第一个成员，直接转换 */
    dev_button_handle_t h = (dev_button_handle_t)mb;

    switch (mb->event) {

    case BTN_SINGLE_CLICK:
        h->user_cb(h, DEV_BTN_EVT_SHORT_UP, h->user_data);
        break;

    case BTN_DOUBLE_CLICK:
        h->user_cb(h, DEV_BTN_EVT_DOUBLE_UP, h->user_data);
        break;

    case BTN_LONG_PRESS_START:
        h->user_cb(h, DEV_BTN_EVT_LONG_HOLD, h->user_data);
        break;

    case BTN_LONG_PRESS_HOLD: {
        uint32_t duration = mb->ticks * TICKS_INTERVAL;
        if (!h->very_long_fired && duration >= DEV_BTN_VERY_LONG_MS) {
            h->very_long_fired = true;
            h->user_cb(h, DEV_BTN_EVT_VERY_LONG_HOLD, h->user_data);
        }
        break;
    }

    case BTN_PRESS_UP: {
        /*
         * 区分短按释放和长按释放：
         * - ticks < LONG_TICKS → 短按，由 SINGLE_CLICK / DOUBLE_CLICK 处理
         * - LONG_TICKS ≤ ticks → 长按释放
         */
        uint32_t duration = mb->ticks * TICKS_INTERVAL;
        if (duration >= DEV_BTN_VERY_LONG_MS) {
            h->user_cb(h, DEV_BTN_EVT_VERY_LONG_UP, h->user_data);
        } else if (duration >= LONG_TICKS * TICKS_INTERVAL) {
            h->user_cb(h, DEV_BTN_EVT_LONG_UP, h->user_data);
        }
        h->very_long_fired = false;
        break;
    }

    default:
        break;
    }
}

static esp_err_t dev_button_create(const dev_button_config_t *config,
                                   dev_button_handle_t       *out_handle)
{
    if (config == NULL || out_handle == NULL || config->callback == NULL ||
        config->gpio_num < 0 || config->gpio_num >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    const hal_gpio_ops_t  *gpio_ops  = hal_gpio_get_ops();
    const hal_timer_ops_t *timer_ops = hal_timer_get_ops();

    dev_button_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    /* 1. 配置 GPIO */
    hal_gpio_config_t gpio_cfg = {
        .pin       = config->gpio_num,
        .dir       = HAL_GPIO_DIR_INPUT,
        .pull_up   = (config->active_level == 1) ? false : true,
        .pull_down = (config->active_level == 1) ? true  : false,
        .intr_type = HAL_GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_ops->init(&gpio_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d init failed: %s", config->gpio_num, esp_err_to_name(ret));
        free(h);
        return ret;
    }

    /* 2. 初始化 multibutton */
    h->user_cb   = config->callback;
    h->user_data = config->user_data;

    button_init(&h->mb_btn, dev_button_pin_level,
                config->active_level, (uint8_t)config->gpio_num);

    /* 3. 注册事件回调 */
    button_attach(&h->mb_btn, BTN_PRESS_DOWN,       dev_button_multibutton_cb, NULL);
    button_attach(&h->mb_btn, BTN_PRESS_UP,         dev_button_multibutton_cb, NULL);
    button_attach(&h->mb_btn, BTN_SINGLE_CLICK,     dev_button_multibutton_cb, NULL);
    button_attach(&h->mb_btn, BTN_DOUBLE_CLICK,     dev_button_multibutton_cb, NULL);
    button_attach(&h->mb_btn, BTN_LONG_PRESS_START, dev_button_multibutton_cb, NULL);
    button_attach(&h->mb_btn, BTN_LONG_PRESS_HOLD,  dev_button_multibutton_cb, NULL);

    /* 4. 启动 */
    (void)button_start(&h->mb_btn);

    /* 5. 创建 5ms 周期定时器驱动 button_ticks */
    hal_timer_config_t timer_cfg = {
        .callback = dev_button_timer_cb,
        .arg      = h,
        .name     = "btn_scan",
    };
    ret = timer_ops->create(&timer_cfg, &h->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Timer create failed: %s", esp_err_to_name(ret));
        button_stop(&h->mb_btn);
        free(h);
        return ret;
    }

    ret = timer_ops->start_periodic(h->timer, TICKS_INTERVAL * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Timer start failed: %s", esp_err_to_name(ret));
        timer_ops->del(h->timer);
        button_stop(&h->mb_btn);
        free(h);
        return ret;
    }

    *out_handle = h;
    ESP_LOGI(TAG, "Button created on GPIO %d (active %s)",
             config->gpio_num, config->active_level ? "HIGH" : "LOW");
    return ESP_OK;
}

static esp_err_t dev_button_delete(dev_button_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const hal_timer_ops_t *timer_ops = hal_timer_get_ops();

    if (handle->timer != NULL) {
        timer_ops->stop(handle->timer);
        timer_ops->del(handle->timer);
    }

    button_stop(&handle->mb_btn);
    free(handle);
    return ESP_OK;
}

static const dev_button_ops_t button_ops = {
    .create = dev_button_create,
    .delete = dev_button_delete,
};

const dev_button_ops_t *dev_button_get_ops(void)
{
    return &button_ops;
}
