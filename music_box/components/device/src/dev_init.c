#include "dev_init.h"

#include "OLED.h"
#include "esp_log.h"

static const char *TAG = "DEV_INIT";

dev_handles_t dev_init_all(void)
{
    dev_handles_t handles = {0};

    /* ------------------------------------------------------------
     * 1. OLED
     * ------------------------------------------------------------ */
    OLED_Init();
    OLED_Clear();
    ESP_LOGI(TAG, "OLED initialized (SDA=GPIO%d, SCL=GPIO%d)",
             DEV_PIN_OLED_SDA, DEV_PIN_OLED_SCL);

    /* ------------------------------------------------------------
     * 2. Audio (MAX98357A via I2S)
     * ------------------------------------------------------------ */
    const dev_audio_ops_t *audio_ops = dev_audio_get_ops();
    const dev_audio_config_t audio_cfg = {
        .port           = I2S_NUM_0,
        .bclk_pin       = DEV_PIN_AUDIO_BCLK,
        .ws_pin         = DEV_PIN_AUDIO_WS,
        .dout_pin       = DEV_PIN_AUDIO_DOUT,
        .mclk_pin       = I2S_GPIO_UNUSED,
        .sd_pin         = DEV_PIN_AUDIO_SD,
        .sample_rate_hz = 44100,
        .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
        .slot_mode      = I2S_SLOT_MODE_STEREO,
        .format         = HAL_I2S_STD_FMT_PHILIPS,
        .dma_desc_num   = 6,
        .dma_frame_num  = 512,
    };

    esp_err_t ret = audio_ops->init(&audio_cfg, &handles.audio);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio init failed: %s", esp_err_to_name(ret));
        return handles;
    }

    ret = audio_ops->set_volume(handles.audio, 4);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set volume failed: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "audio initialized");

    /* ------------------------------------------------------------
     * 3. LED Strip（8 个独立指示灯 GPIO2~GPIO9）
     * ------------------------------------------------------------ */
    const gpio_num_t led_pins[DEV_LED_MAX] = {
        DEV_PIN_LED_START + 0,
        DEV_PIN_LED_START + 1,
        DEV_PIN_LED_START + 2,
        DEV_PIN_LED_START + 3,
        DEV_PIN_LED_START + 4,
        DEV_PIN_LED_START + 5,
        DEV_PIN_LED_START + 6,
        DEV_PIN_LED_START + 7,
    };

    const dev_led_strip_ops_t *led_ops = dev_led_strip_get_ops();
    const dev_led_strip_config_t led_cfg = {
        .gpio_nums    = led_pins,
        .led_count    = DEV_LED_MAX,
        .active_level = 1,
    };

    ret = led_ops->init(&led_cfg, &handles.led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED strip init failed: %s", esp_err_to_name(ret));
        audio_ops->deinit(handles.audio);
        handles.audio = NULL;
        return handles;
    }

    ret = led_ops->all_off(handles.led_strip);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LED all-off failed: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "LED strip initialized (%d LEDs)", DEV_LED_MAX);

    /* ------------------------------------------------------------
     * 4. Light Sensor（GPIO1 DO 数字光敏）
     * ------------------------------------------------------------ */
    const dev_light_sensor_ops_t *sensor_ops = dev_light_sensor_get_ops();
    const dev_light_sensor_config_t sensor_cfg = {
        .do_pin = DEV_PIN_LIGHT_SENSOR_DO,
    };

    ret = sensor_ops->init(&sensor_cfg, &handles.light_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "light sensor init failed: %s", esp_err_to_name(ret));
        led_ops->deinit(handles.led_strip);
        handles.led_strip = NULL;
        audio_ops->deinit(handles.audio);
        handles.audio = NULL;
        return handles;
    }
    ESP_LOGI(TAG, "light sensor initialized (DO=GPIO%d)", DEV_PIN_LIGHT_SENSOR_DO);

    ESP_LOGI(TAG, "dev_init_all completed");
    return handles;
}

esp_err_t dev_init_create_button(gpio_num_t gpio,
                                 uint8_t active_level,
                                 dev_btn_callback_t cb,
                                 void *user_data,
                                 dev_button_handle_t *out_handle)
{
    const dev_button_ops_t *btn_ops = dev_button_get_ops();
    const dev_button_config_t btn_cfg = {
        .gpio_num     = gpio,
        .active_level = active_level,
        .callback     = cb,
        .user_data    = user_data,
    };

    return btn_ops->create(&btn_cfg, out_handle);
}
