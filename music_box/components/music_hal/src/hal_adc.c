#include "hal_adc.h"

static const char *TAG = "HAL_ADC";

static adc_oneshot_unit_handle_t s_unit_handle;
static adc_cali_handle_t         s_cali_handle;
static adc_channel_t             s_channel;
static bool                      s_calibrated;
static bool                      s_inited;

static esp_err_t s_calibration_init(adc_unit_t unit,
                                    adc_channel_t channel,
                                    adc_atten_t atten,
                                    adc_bitwidth_t bitwidth)
{
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = unit,
        .chan     = channel,
        .atten    = atten,
        .bitwidth = bitwidth,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
    if (ret == ESP_OK) {
        s_calibrated = true;
        ESP_LOGI(TAG, "calibration success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "calibration init failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void s_calibration_deinit(void)
{
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(s_cali_handle));
}

/*---------------------------------------------------------------
        Ops Implementations
---------------------------------------------------------------*/
static esp_err_t s_hal_adc_init(const hal_adc_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_inited) {
        ESP_LOGE(TAG, "ADC already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 1. 创建 oneshot 单元
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = cfg->unit,
    };
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_unit_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new unit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. 配置通道
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = cfg->atten,
        .bitwidth = cfg->bitwidth,
    };
    ret = adc_oneshot_config_channel(s_unit_handle, cfg->channel, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config channel[%d] failed: %s", cfg->channel, esp_err_to_name(ret));
        adc_oneshot_del_unit(s_unit_handle);
        return ret;
    }

    s_channel = cfg->channel;

    // 3. 校准（可选）
    if (cfg->enable_calibration) {
        s_calibration_init(cfg->unit, cfg->channel, cfg->atten, cfg->bitwidth);
    }

    s_inited = true;
    return ESP_OK;
}

static esp_err_t s_hal_adc_deinit(void)
{
    if (!s_inited) {
        ESP_LOGE(TAG, "ADC is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_calibrated) {
        s_calibration_deinit();
    }

    esp_err_t ret = adc_oneshot_del_unit(s_unit_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "del unit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_inited = false;
    return ESP_OK;
}

static esp_err_t s_hal_adc_read_raw(int *out_raw)
{
    if (!s_inited || out_raw == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return adc_oneshot_read(s_unit_handle, s_channel, out_raw);
}

static esp_err_t s_hal_adc_read_voltage(int *out_mv)
{
    if (!s_inited || out_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int raw = 0;
    esp_err_t ret = adc_oneshot_read(s_unit_handle, s_channel, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_calibrated) {
        ret = adc_cali_raw_to_voltage(s_cali_handle, raw, out_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "raw->voltage failed: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    *out_mv = raw;
    return ESP_OK;
}

/*---------------------------------------------------------------
        Ops Table
---------------------------------------------------------------*/
static const hal_adc_ops_t s_hal_adc_ops = {
    .init         = s_hal_adc_init,
    .deinit       = s_hal_adc_deinit,
    .read_raw     = s_hal_adc_read_raw,
    .read_voltage = s_hal_adc_read_voltage,
};

const hal_adc_ops_t *hal_adc_get_ops(void)
{
    return &s_hal_adc_ops;
}