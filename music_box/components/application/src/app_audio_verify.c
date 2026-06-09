#include "app_audio_verify.h"

#include <stdbool.h>
#include "dev_audio.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_AUDIO_VERIFY";

#define APP_AUDIO_VERIFY_SPIFFS_LABEL       "storage"
#define APP_AUDIO_VERIFY_MOUNT_POINT        "/spiffs"
#define APP_AUDIO_VERIFY_FILE_PATH          APP_AUDIO_VERIFY_MOUNT_POINT "/Your_name.mp3"
#define APP_AUDIO_VERIFY_DEFAULT_VOLUME     4
#define APP_AUDIO_VERIFY_POLL_MS            200
#define APP_AUDIO_VERIFY_START_TIMEOUT_MS   5000

static esp_err_t app_audio_verify_mount_spiffs(bool *mounted)
{
    if (mounted == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *mounted = false;

    esp_vfs_spiffs_conf_t conf = {
        .base_path              = APP_AUDIO_VERIFY_MOUNT_POINT,
        .partition_label        = APP_AUDIO_VERIFY_SPIFFS_LABEL,
        .max_files              = 4,
        .format_if_mount_failed = false,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    *mounted = true;

    size_t total = 0;
    size_t used  = 0;
    ret = esp_spiffs_info(APP_AUDIO_VERIFY_SPIFFS_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS mounted, total=%u, used=%u", (unsigned)total, (unsigned)used);
    } else {
        ESP_LOGW(TAG, "SPIFFS mounted, but info failed: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

static void app_audio_verify_unmount_spiffs(bool mounted)
{
    if (mounted) {
        esp_vfs_spiffs_unregister(APP_AUDIO_VERIFY_SPIFFS_LABEL);
        ESP_LOGI(TAG, "SPIFFS unmounted");
    }
}

static esp_err_t app_audio_verify_wait_done(const dev_audio_ops_t *audio_ops,
                                            dev_audio_handle_t audio)
{
    bool     saw_playing   = false;
    uint32_t wait_start_ms = 0;

    while (true) {
        dev_audio_state_t state = DEV_AUDIO_STATE_IDLE;
        esp_err_t ret = audio_ops->get_state(audio, &state);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "get audio state failed: %s", esp_err_to_name(ret));
            return ret;
        }

        if (state == DEV_AUDIO_STATE_PLAYING) {
            if (!saw_playing) {
                ESP_LOGI(TAG, "audio state: PLAYING");
            }
            saw_playing = true;
        } else if (state == DEV_AUDIO_STATE_IDLE) {
            if (saw_playing) {
                ESP_LOGI(TAG, "audio state: IDLE, playback finished");
                return ESP_OK;
            }

            if (wait_start_ms >= APP_AUDIO_VERIFY_START_TIMEOUT_MS) {
                ESP_LOGE(TAG, "audio did not enter PLAYING state");
                return ESP_ERR_TIMEOUT;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_AUDIO_VERIFY_POLL_MS));
        if (!saw_playing) {
            wait_start_ms += APP_AUDIO_VERIFY_POLL_MS;
        }
    }
}

esp_err_t app_audio_verify_run(void)
{
    const dev_audio_ops_t *audio_ops = dev_audio_get_ops();
    dev_audio_handle_t    audio      = NULL;
    bool                  mounted    = false;

    const dev_audio_config_t audio_cfg = {
        .port            = I2S_NUM_0,
        .bclk_pin        = GPIO_NUM_16,
        .ws_pin          = GPIO_NUM_17,
        .dout_pin        = GPIO_NUM_18,
        .mclk_pin        = I2S_GPIO_UNUSED,
        .sd_pin          = GPIO_NUM_15,
        .sample_rate_hz  = 44100,
        .data_bit_width  = I2S_DATA_BIT_WIDTH_16BIT,
        .slot_mode       = I2S_SLOT_MODE_STEREO,
        .format          = HAL_I2S_STD_FMT_PHILIPS,
        .dma_desc_num    = 6,
        .dma_frame_num   = 512,
    };

    esp_err_t ret = app_audio_verify_mount_spiffs(&mounted);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = audio_ops->init(&audio_cfg, &audio);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio init failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ESP_LOGI(TAG, "audio initialized");

    ret = audio_ops->set_volume(audio, APP_AUDIO_VERIFY_DEFAULT_VOLUME);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set volume failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "start playing: %s", APP_AUDIO_VERIFY_FILE_PATH);
    ret = audio_ops->play(audio, APP_AUDIO_VERIFY_FILE_PATH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "play failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = app_audio_verify_wait_done(audio_ops, audio);

cleanup:
    if (audio != NULL) {
        if (ret != ESP_OK) {
            (void)audio_ops->stop(audio);
        }

        esp_err_t deinit_ret = audio_ops->deinit(audio);
        if (deinit_ret != ESP_OK) {
            ESP_LOGE(TAG, "audio deinit failed: %s", esp_err_to_name(deinit_ret));
            if (ret == ESP_OK) {
                ret = deinit_ret;
            }
        }
    }

    app_audio_verify_unmount_spiffs(mounted);
    return ret;
}
