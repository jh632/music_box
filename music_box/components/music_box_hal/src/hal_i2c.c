#include "hal_i2c.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "HAL_I2C";

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

static esp_err_t s_hal_i2c_bus_init(const hal_i2c_bus_config_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_bus != NULL) {
        ESP_LOGE(TAG, "bus already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = cfg->port,
        .sda_io_num = cfg->sda_pin,
        .scl_io_num = cfg->scl_pin,
        .clk_source = cfg->clk_source,
        .glitch_ignore_cnt = cfg->glitch_ignore_cnt,
        .intr_priority = cfg->intr_priority,
        .trans_queue_depth = cfg->trans_queue_depth,
        .flags.enable_internal_pullup = cfg->enable_internal_pullup,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new master bus failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_i2c_device_deinit(void)
{
    if (s_dev == NULL) {
        ESP_LOGE(TAG, "device is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2c_master_bus_rm_device(s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "remove device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_dev = NULL;
    return ESP_OK;
}

static esp_err_t s_hal_i2c_bus_deinit(void)
{
    if (s_bus == NULL) {
        ESP_LOGE(TAG, "bus is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_dev != NULL) {
        esp_err_t ret = s_hal_i2c_device_deinit();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    esp_err_t ret = i2c_del_master_bus(s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete master bus failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_bus = NULL;
    return ESP_OK;
}

static esp_err_t s_hal_i2c_device_init(const hal_i2c_dev_config_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_bus == NULL) {
        ESP_LOGE(TAG, "bus is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_dev != NULL) {
        ESP_LOGE(TAG, "device already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = cfg->address_bit_len,
        .device_address = cfg->device_address,
        .scl_speed_hz = cfg->scl_speed_hz,
        .scl_wait_us = cfg->scl_wait_us,
        .flags.disable_ack_check = cfg->disable_ack_check,
    };

    esp_err_t ret = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_i2c_transmit(const uint8_t *data, size_t len, int timeout_ms)
{
    if (s_dev == NULL || (data == NULL && len > 0)) {
        ESP_LOGE(TAG, "invalid transmit arg");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_transmit(s_dev, data, len, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "transmit failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_i2c_transmit_receive(const uint8_t *write_data,
                                            size_t write_len,
                                            uint8_t *read_data,
                                            size_t read_len,
                                            int timeout_ms)
{
    if (s_dev == NULL ||
        (write_data == NULL && write_len > 0) ||
        (read_data == NULL && read_len > 0)) {
        ESP_LOGE(TAG, "invalid transmit_receive arg");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_transmit_receive(s_dev,
                                                write_data,
                                                write_len,
                                                read_data,
                                                read_len,
                                                timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "transmit_receive failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_i2c_read_reg(uint8_t reg_addr,
                                    uint8_t *data,
                                    size_t len,
                                    int timeout_ms)
{
    if (data == NULL && len > 0) {
        ESP_LOGE(TAG, "invalid read_reg arg");
        return ESP_ERR_INVALID_ARG;
    }
    return s_hal_i2c_transmit_receive(&reg_addr, sizeof(reg_addr), data, len, timeout_ms);
}

static esp_err_t s_hal_i2c_write_reg(uint8_t reg_addr,
                                     const uint8_t *data,
                                     size_t len,
                                     int timeout_ms)
{
    if (data == NULL && len > 0) {
        ESP_LOGE(TAG, "invalid write_reg arg");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *write_buf = calloc(1, len + sizeof(reg_addr));
    if (write_buf == NULL) {
        ESP_LOGE(TAG, "OOM");
        return ESP_ERR_NO_MEM;
    }

    write_buf[0] = reg_addr;
    if (len > 0) {
        memcpy(&write_buf[1], data, len);
    }

    esp_err_t ret = s_hal_i2c_transmit(write_buf, len + sizeof(reg_addr), timeout_ms);
    free(write_buf);
    return ret;
}

static const hal_i2c_ops_t s_hal_i2c_ops = {
    .bus_init = s_hal_i2c_bus_init,
    .bus_deinit = s_hal_i2c_bus_deinit,
    .device_init = s_hal_i2c_device_init,
    .device_deinit = s_hal_i2c_device_deinit,
    .transmit = s_hal_i2c_transmit,
    .transmit_receive = s_hal_i2c_transmit_receive,
    .read_reg = s_hal_i2c_read_reg,
    .write_reg = s_hal_i2c_write_reg,
};

const hal_i2c_ops_t *hal_i2c_get_ops(void)
{
    return &s_hal_i2c_ops;
}
