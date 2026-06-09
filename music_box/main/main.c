#include "dev_init.h"

#include "app_audio_verify.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "MAIN";

/** 全局设备句柄，应用层各模块通过此句柄访问已初始化的外设 */
dev_handles_t g_dev_handles;

// 打印内存使用情况
void displayMemoryUsage() {
    size_t totalDRAM = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t freeDRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t usedDRAM = totalDRAM - freeDRAM;

    size_t totalPSRAM = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t usedPSRAM = totalPSRAM - freePSRAM;

    size_t DRAM_largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    size_t PSRAM_largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    float dramUsagePercentage = (float)usedDRAM / totalDRAM * 100;
    float psramUsagePercentage = (float)usedPSRAM / totalPSRAM * 100;

    ESP_LOGI(TAG, "DRAM Total: %zu bytes, Used: %zu bytes, Free: %zu bytes,  DRAM_Largest_block: %zu bytes", totalDRAM, usedDRAM, freeDRAM, DRAM_largest_block);
    ESP_LOGI(TAG, "DRAM Used: %.2f%%", dramUsagePercentage);
    ESP_LOGI(TAG, "PSRAM Total: %zu bytes, Used: %zu bytes, Free: %zu bytes, PSRAM_Largest_block: %zu bytes", totalPSRAM, usedPSRAM, freePSRAM, PSRAM_largest_block);
    ESP_LOGI(TAG, "PSRAM Used: %.2f%%", psramUsagePercentage);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    /* 初始化所有板载外设 */
    g_dev_handles = dev_init_all();

    /* 播放测试音频（短暂测试，播放完后返回） */
    esp_err_t ret = app_audio_verify_run();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio verify failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "audio verify finished.");
    }

    while (true) {
        displayMemoryUsage();
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }
}
