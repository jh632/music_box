#include "app_audio_verify.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "OLED.h"

static const char *TAG = "MAIN";

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
    /* main 只做系统初始化和应用层入口编排。 */
    ESP_ERROR_CHECK(nvs_flash_init());

    // esp_err_t ret = app_audio_verify_run();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "audio verify failed: %s", esp_err_to_name(ret));
    //     return;
    // }

    // ESP_LOGI(TAG, "audio verify finished.");
    OLED_Init();

    while (true) {
        //displayMemoryUsage(); //只有测试音乐结束才能打印，还没优化
        OLED_ShowMusicAnimation();
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
