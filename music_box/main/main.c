/*
 * main.c — 编排入口
 *
 * 只负责层级初始化调用，不出现任何具体模块的配置参数。
 * 具体初始化逻辑由各层的 _init_all() 负责。
 */

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    /* ESP-IDF 系统组件 */
    ESP_ERROR_CHECK(nvs_flash_init());

    /*
     * 分层初始化（顺序即依赖顺序）
     * TODO: 待各层实现后逐步解注释
     *
     * ESP_ERROR_CHECK(dev_init_all());
     * ESP_ERROR_CHECK(app_init_all());
     */

    ESP_LOGI(TAG, "Music Box started.");
}
