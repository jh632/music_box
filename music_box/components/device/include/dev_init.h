#pragma once

#include "dev_audio.h"
#include "dev_button.h"
#include "dev_led_strip.h"
#include "dev_light_sensor.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 默认引脚分配（与硬件 PCB 对应）
 * ================================================================ */
#define DEV_PIN_AUDIO_BCLK      GPIO_NUM_16
#define DEV_PIN_AUDIO_WS        GPIO_NUM_17
#define DEV_PIN_AUDIO_DOUT      GPIO_NUM_18
#define DEV_PIN_AUDIO_SD        GPIO_NUM_15

#define DEV_PIN_OLED_SDA        GPIO_NUM_41
#define DEV_PIN_OLED_SCL        GPIO_NUM_42

#define DEV_PIN_LIGHT_SENSOR_DO GPIO_NUM_1

#define DEV_PIN_LED_START       GPIO_NUM_2
#define DEV_PIN_LED_END         GPIO_NUM_9   /* GPIO2 ~ GPIO9 = 8 LEDs */

#define DEV_PIN_BTN_MODE        GPIO_NUM_11  /* KEY1 - 自动/手动模式 */
#define DEV_PIN_BTN_TOGGLE      GPIO_NUM_12  /* KEY2 - 切/关 */
#define DEV_PIN_BTN_VOLUME      GPIO_NUM_13  /* KEY3 - 音量控制 */
#define DEV_PIN_BTN_PLAY        GPIO_NUM_14  /* KEY4 - 播放/暂停 */

/* ================================================================
 * 设备数量上限
 * ================================================================ */
#define DEV_LED_MAX             8
#define DEV_BUTTON_MAX          4   /* KEY1 ~ KEY4，Boot KEY 归系统保留 */

/* ================================================================
 * 句柄集合
 * ================================================================ */
typedef struct {
    dev_audio_handle_t          audio;
    dev_led_strip_handle_t      led_strip;
    dev_light_sensor_handle_t   light_sensor;
    dev_button_handle_t         buttons[DEV_BUTTON_MAX];
} dev_handles_t;

/* ================================================================
 * 函数声明
 * ================================================================ */

/**
 * @brief 初始化所有板上外设（OLED / Audio / LED / Light Sensor）
 *        返回包含各设备句柄的结构体。部分句柄可能为 NULL（对应设备初始化失败）。
 *
 * 注：按键因需要应用层提供回调，不由本函数创建。
 *     调用方可通过 dev_init_create_button() 逐一创建。
 */
dev_handles_t dev_init_all(void);

/**
 * @brief 便捷创建单个按键实例
 *
 * @param gpio        GPIO 引脚号
 * @param active_level 按下电平（0=低电平，1=高电平）
 * @param cb           事件回调
 * @param user_data    用户上下文
 * @param out_handle   输出句柄
 * @return esp_err_t
 */
esp_err_t dev_init_create_button(gpio_num_t gpio,
                                 uint8_t active_level,
                                 dev_btn_callback_t cb,
                                 void *user_data,
                                 dev_button_handle_t *out_handle);

#ifdef __cplusplus
}
#endif
