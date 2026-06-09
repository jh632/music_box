#include "dev_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "DEV_BUTTON";

/* 共享定时器相关 */
static dev_button_handle_t s_button_list  = NULL;
static uint32_t            s_button_count = 0;
static SemaphoreHandle_t   s_list_mutex   = NULL;

static esp_err_t ensure_mutex_init(void)
{
    if (s_list_mutex == NULL) {
        s_list_mutex = xSemaphoreCreateMutex();
        if (s_list_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create list mutex");
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

struct dev_button_s {
    gpio_num_t         gpio;
    uint8_t            active_level;
    dev_btn_callback_t callback;
    void              *user_data;

    /* 时间阈值 (ms) */
    uint32_t debounce_th;        // 消抖
    uint32_t short_press_th;     // 短按上限
    uint32_t double_press_th;    // 双击窗口
    uint32_t long_press_th;      // 长按阈值
    uint32_t very_long_press_th; // 超长按阈值

    /* 消抖状态 */
    bool     cur_pressed;        // 当前原始电平
    bool     stable_pressed;     // 消抖后按下状态
    uint32_t debounce_change_ts; // 原始电平变化时刻

    /* 按压追踪 */
    uint32_t press_down_ts;        // 按下时刻
    bool     long_hold_fired;      // LONG_HOLD 已触发
    bool     very_long_hold_fired; // VERY_LONG_HOLD 已触发

    /* 双击检测 */
    bool     wait_second_press;   // 等待第二次按下
    bool     second_press_active; // 第二次按下已开始
    uint32_t first_release_ts;    // 第一次释放时刻

    struct dev_button_s *next;    // 链表指针
};

static bool read_pressed(dev_button_handle_t h)
{
    int level = hal_gpio_get_ops()->get_level(h->gpio);
    return (level != 0) == (h->active_level != 0);
}

/**
 * @brief 周期扫描回调：消抖 + 事件检测
 *
 * 事件模型：
 *   按住达到阈值 → LONG_HOLD / VERY_LONG_HOLD（立即通知）
 *   释放时判断类型 → SHORT_UP / DOUBLE_UP / LONG_UP / VERY_LONG_UP
 * 
 * @param arg 
 */
static void button_timer_callback(void *arg)
{
    (void)arg;

    /* 非阻塞 trylock，锁被持有时跳过此周期 */
    if (xSemaphoreTake(s_list_mutex, 0) != pdTRUE) {
        return;
    }

    uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000ULL);

    /* 第一遍：锁内运行状态机，仅记录待发事件，不直接调用回调 */
    typedef struct {
        dev_button_handle_t h;
        dev_btn_event_t     evt;
    } pending_evt_t;

    pending_evt_t pending[32];
    int           pending_cnt = 0;

    for (dev_button_handle_t h = s_button_list; h != NULL; h = h->next) {
        bool raw = read_pressed(h);

        /* --- 消抖 --- */
        if (raw != h->cur_pressed) {
            h->cur_pressed        = raw;
            h->debounce_change_ts = ts;
        }

        if (h->stable_pressed != h->cur_pressed && (ts - h->debounce_change_ts) >= h->debounce_th) {
            h->stable_pressed = h->cur_pressed;

            if (h->stable_pressed) {
                /* === 按下沿 === */
                if (h->wait_second_press) {
                    uint32_t gap = ts - h->first_release_ts;
                    if (gap <= h->double_press_th) {
                        h->second_press_active = true;
                    } else {
                        pending[pending_cnt].h   = h;
                        pending[pending_cnt].evt = DEV_BTN_EVT_SHORT_UP;
                        pending_cnt++;
                        h->wait_second_press   = false;
                        h->second_press_active = false;
                    }
                }
                h->press_down_ts        = ts;
                h->long_hold_fired      = false;
                h->very_long_hold_fired = false;

            } else {
                /* === 释放沿 === */
                uint32_t duration = ts - h->press_down_ts;

                if (duration <= h->short_press_th) {
                    /* 短按区间：进入双击检测 */
                    if (h->wait_second_press && h->second_press_active) {
                        pending[pending_cnt].h   = h;
                        pending[pending_cnt].evt = DEV_BTN_EVT_DOUBLE_UP;
                        pending_cnt++;
                        h->wait_second_press = false;
                    } else {
                        h->wait_second_press = true;
                        h->first_release_ts  = ts;
                    }
                } else if (duration < h->long_press_th) {
                    /* 介于短按和长按之间：若是第二击则仍判定为双击 */
                    if (h->wait_second_press && h->second_press_active) {
                        pending[pending_cnt].h   = h;
                        pending[pending_cnt].evt = DEV_BTN_EVT_DOUBLE_UP;
                        pending_cnt++;
                    } else {
                        pending[pending_cnt].h   = h;
                        pending[pending_cnt].evt = DEV_BTN_EVT_SHORT_UP;
                        pending_cnt++;
                    }
                    h->wait_second_press = false;
                } else if (duration < h->very_long_press_th) {
                    /* 长按释放 */
                    pending[pending_cnt].h   = h;
                    pending[pending_cnt].evt = DEV_BTN_EVT_LONG_UP;
                    pending_cnt++;
                    if (h->wait_second_press) {
                        h->wait_second_press = false;
                    }
                } else {
                    /* 超长按释放 */
                    pending[pending_cnt].h   = h;
                    pending[pending_cnt].evt = DEV_BTN_EVT_VERY_LONG_UP;
                    pending_cnt++;
                    if (h->wait_second_press) {
                        h->wait_second_press = false;
                    }
                }
                h->second_press_active = false;
            }
        }

        /* --- 双击超时：确认为单击 --- */
        if (h->wait_second_press && !h->stable_pressed && !h->second_press_active &&
            (ts - h->first_release_ts) >= h->double_press_th) {
            pending[pending_cnt].h   = h;
            pending[pending_cnt].evt = DEV_BTN_EVT_SHORT_UP;
            pending_cnt++;
            h->wait_second_press = false;
        }

        /* --- 按住时 HOLD 检测 --- */
        if (h->stable_pressed) {
            uint32_t duration = ts - h->press_down_ts;

            if (!h->long_hold_fired && duration >= h->long_press_th) {
                h->long_hold_fired = true;
                pending[pending_cnt].h   = h;
                pending[pending_cnt].evt = DEV_BTN_EVT_LONG_HOLD;
                pending_cnt++;
            }
            if (!h->very_long_hold_fired && duration >= h->very_long_press_th) {
                h->very_long_hold_fired = true;
                pending[pending_cnt].h   = h;
                pending[pending_cnt].evt = DEV_BTN_EVT_VERY_LONG_HOLD;
                pending_cnt++;
            }
        } /* if (h->stable_pressed) 结束 */
    }     /* for 结束 */

    xSemaphoreGive(s_list_mutex);

    /* 第二遍：出锁后分发所有待发事件 */
    for (int i = 0; i < pending_cnt; i++) {
        pending[i].h->callback(pending[i].h, pending[i].evt, pending[i].h->user_data);
    }
}

/**
 * @brief 创建按键实例，配置GPIO并启动周期扫描定时器
 */
static esp_err_t dev_button_create(const dev_button_config_t *config,
                                   dev_button_handle_t       *out_handle)
{
    if (config == NULL || out_handle == NULL || config->callback == NULL || config->gpio_num < 0 ||
        config->gpio_num >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    const hal_gpio_ops_t  *gpio_ops  = hal_gpio_get_ops();
    const hal_timer_ops_t *timer_ops = hal_timer_get_ops();

    dev_button_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    hal_gpio_config_t gpio_cfg = {
        .pin       = config->gpio_num,
        .dir       = HAL_GPIO_DIR_INPUT,
        .pull_up   = !config->active_level,
        .pull_down = config->active_level,
        .intr_type = HAL_GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_ops->init(&gpio_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d init failed", config->gpio_num);
        free(h);
        return ret;
    }

    ret = ensure_mutex_init();
    if (ret != ESP_OK) {
        free(h);
        return ret;
    }

    h->gpio         = config->gpio_num;
    h->active_level = config->active_level;
    h->callback     = config->callback;
    h->user_data    = config->user_data;
    h->debounce_th  = config->debounce_ms ? config->debounce_ms : DEV_BTN_DEFAULT_DEBOUNCE_MS;
    h->short_press_th =
        config->short_press_ms ? config->short_press_ms : DEV_BTN_DEFAULT_SHORT_PRESS_MS;
    h->double_press_th =
        config->double_press_ms ? config->double_press_ms : DEV_BTN_DEFAULT_DOUBLE_PRESS_MS;
    h->long_press_th =
        config->long_press_ms ? config->long_press_ms : DEV_BTN_DEFAULT_LONG_PRESS_MS;
    h->very_long_press_th =
        config->very_long_ms ? config->very_long_ms : DEV_BTN_DEFAULT_VERY_LONG_MS;

    /* 读取初始电平 */
    bool pressed          = read_pressed(h);
    h->cur_pressed        = pressed;
    h->stable_pressed     = pressed;
    h->debounce_change_ts = (uint32_t)(esp_timer_get_time() / 1000ULL);

    /* 共享定时器：第一个按键创建时启动，后续按键共用 */
    xSemaphoreTake(s_list_mutex, portMAX_DELAY);

    if (s_button_count == 0) {
        hal_timer_config_t timer_cfg = {
            .callback = button_timer_callback,
            .arg      = NULL, /* 回调遍历全局链表，不使用 arg */
        };
        ret = timer_ops->create(&timer_cfg);
        if (ret != ESP_OK) {
            xSemaphoreGive(s_list_mutex);
            ESP_LOGE(TAG, "Timer create failed");
            free(h);
            return ret;
        }
        ret = timer_ops->start_periodic(SCAN_PERIOD_US);
        if (ret != ESP_OK) {
            timer_ops->del();
            xSemaphoreGive(s_list_mutex);
            ESP_LOGE(TAG, "Timer start failed");
            free(h);
            return ret;
        }
    }

    /* 插入链表头 */
    h->next       = s_button_list;
    s_button_list = h;
    s_button_count++;

    xSemaphoreGive(s_list_mutex);

    *out_handle = h;
    ESP_LOGI(TAG,
             "Button created on GPIO %d (active %s)",
             h->gpio,
             h->active_level ? "HIGH" : "LOW");
    return ESP_OK;
}

/**
 * @brief 删除按键实例，停止扫描并释放资源
 */
static esp_err_t dev_button_delete(dev_button_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const hal_timer_ops_t *timer_ops = hal_timer_get_ops();

    xSemaphoreTake(s_list_mutex, portMAX_DELAY);

    /* 从单链表中查找并摘除 */
    dev_button_handle_t *prev = &s_button_list;
    dev_button_handle_t  cur  = s_button_list;
    while (cur != NULL && cur != handle) {
        prev = &cur->next;
        cur  = cur->next;
    }

    if (cur == NULL) {
        xSemaphoreGive(s_list_mutex);
        ESP_LOGW(TAG, "Handle not found in list");
        return ESP_ERR_NOT_FOUND;
    }

    *prev = cur->next;
    s_button_count--;

    if (s_button_count == 0) {
        timer_ops->stop();
        timer_ops->del();
    }

    xSemaphoreGive(s_list_mutex);

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
