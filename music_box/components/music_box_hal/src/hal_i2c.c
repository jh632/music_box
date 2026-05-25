/*
 * hal_i2c.c — HAL 层 I2C master 封装实现
 */

#include "hal_i2c.h"

static const char *TAG = "HAL_I2C";

struct hal_i2c_bus_s {
    i2c_master_bus_handle_t bus;
};

struct hal_i2c_dev_s {
    i2c_master_dev_handle_t dev;
};

esp_err_t hal_i2c_bus_init(const hal_i2c_bus_config_t *cfg,
                           hal_i2c_bus_handle_t *out_bus)
{
    if (cfg == NULL || out_bus == NULL) {
        ESP_LOGE(TAG, "invalid bus init arg");
        return ESP_ERR_INVALID_ARG;
    }

    hal_i2c_bus_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "OOM");
        return ESP_ERR_NO_MEM;
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

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &h->bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new master bus failed: %s", esp_err_to_name(ret));
        free(h);
        return ret;
    }

    *out_bus = h;
    return ESP_OK;
}

esp_err_t hal_i2c_bus_deinit(hal_i2c_bus_handle_t bus)
{
    if (bus == NULL) {
        ESP_LOGE(TAG, "bus is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_del_master_bus(bus->bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete master bus failed: %s", esp_err_to_name(ret));
        return ret;
    }

    free(bus);
    return ESP_OK;
}

esp_err_t hal_i2c_bus_add_device(hal_i2c_bus_handle_t bus,
                                 const hal_i2c_dev_config_t *cfg,
                                 hal_i2c_dev_handle_t *out_dev)
{
    if (bus == NULL || cfg == NULL || out_dev == NULL) {
        ESP_LOGE(TAG, "invalid add device arg");
        return ESP_ERR_INVALID_ARG;
    }

    hal_i2c_dev_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "OOM");
        return ESP_ERR_NO_MEM;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = cfg->address_bit_len,
        .device_address = cfg->device_address,
        .scl_speed_hz = cfg->scl_speed_hz,
        .scl_wait_us = cfg->scl_wait_us,
        .flags.disable_ack_check = cfg->disable_ack_check,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus->bus, &dev_cfg, &h->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(ret));
        free(h);
        return ret;
    }

    *out_dev = h;
    return ESP_OK;
}

esp_err_t hal_i2c_device_remove(hal_i2c_dev_handle_t dev)
{
    if (dev == NULL) {
        ESP_LOGE(TAG, "dev is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_bus_rm_device(dev->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "remove device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    free(dev);
    return ESP_OK;
}

esp_err_t hal_i2c_transmit(hal_i2c_dev_handle_t dev,
                           const uint8_t *data,
                           size_t len,
                           int timeout_ms)
{
    if (dev == NULL || (data == NULL && len > 0)) {
        ESP_LOGE(TAG, "invalid transmit arg");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_transmit(dev->dev, data, len, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "transmit failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hal_i2c_transmit_receive(hal_i2c_dev_handle_t dev,
                                   const uint8_t *write_data,
                                   size_t write_len,
                                   uint8_t *read_data,
                                   size_t read_len,
                                   int timeout_ms)
{
    if (dev == NULL ||
        (write_data == NULL && write_len > 0) ||
        (read_data == NULL && read_len > 0)) {
        ESP_LOGE(TAG, "invalid transmit_receive arg");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_transmit_receive(dev->dev,
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

esp_err_t hal_i2c_read_reg(hal_i2c_dev_handle_t dev,
                           uint8_t reg_addr,
                           uint8_t *data,
                           size_t len,
                           int timeout_ms)
{
    if (data == NULL && len > 0) {
        ESP_LOGE(TAG, "invalid read_reg arg");
        return ESP_ERR_INVALID_ARG;
    }
    return hal_i2c_transmit_receive(dev, &reg_addr, sizeof(reg_addr), data, len, timeout_ms);
}

esp_err_t hal_i2c_write_reg(hal_i2c_dev_handle_t dev,
                            uint8_t reg_addr,
                            const uint8_t *data,
                            size_t len,
                            int timeout_ms)
{
    if (dev == NULL || (data == NULL && len > 0)) {
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

    esp_err_t ret = hal_i2c_transmit(dev, write_buf, len + sizeof(reg_addr), timeout_ms);
    free(write_buf);
    return ret;
}

static const hal_i2c_ops_t s_hal_i2c_ops = {
    .bus_init = hal_i2c_bus_init,
    .bus_deinit = hal_i2c_bus_deinit,
    .bus_add_device = hal_i2c_bus_add_device,
    .device_remove = hal_i2c_device_remove,
    .transmit = hal_i2c_transmit,
    .transmit_receive = hal_i2c_transmit_receive,
    .read_reg = hal_i2c_read_reg,
    .write_reg = hal_i2c_write_reg,
};

const hal_i2c_ops_t *hal_i2c_get_ops(void)
{
    return &s_hal_i2c_ops;
}
