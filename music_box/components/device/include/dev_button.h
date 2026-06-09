#ifndef DEV_BUTTON_H_
#define DEV_BUTTON_H_

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_button_s *dev_button_handle_t;

typedef enum {
    /* 释放时触发（确认按压类型） */
    DEV_BTN_EVT_SHORT_UP,         // 短按释放（单击）
    DEV_BTN_EVT_DOUBLE_UP,        // 双击释放
    DEV_BTN_EVT_LONG_UP,          // 长按释放（1s~5s间松手）
    DEV_BTN_EVT_VERY_LONG_UP,     // 超长按释放（≥5s后松手）
    /* 按住时触发（达到阈值立即通知） */
    DEV_BTN_EVT_LONG_HOLD,        // 按住达到长按阈值（1s）
    DEV_BTN_EVT_VERY_LONG_HOLD,   // 按住达到超长按阈值（5s）
} dev_btn_event_t;

typedef void (*dev_btn_callback_t)(dev_button_handle_t handle,
                                   dev_btn_event_t event,
                                   void *user_data);

typedef struct {
    gpio_num_t         gpio_num;
    uint8_t            active_level;     // 1 = 高电平按下, 0 = 低电平按下
    dev_btn_callback_t callback;
    void              *user_data;
} dev_button_config_t;

typedef struct {
    esp_err_t (*create)(const dev_button_config_t *config, dev_button_handle_t *out_handle);
    esp_err_t (*delete)(dev_button_handle_t handle);
} dev_button_ops_t;

const dev_button_ops_t *dev_button_get_ops(void);

#ifdef __cplusplus
}
#endif

#endif // DEV_BUTTON_H_
