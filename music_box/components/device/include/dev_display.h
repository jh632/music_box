#pragma once

#include <stdint.h>
#include "hal_i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_display_s *dev_display_handle_t;

typedef struct {
    i2c_port_t i2c_port;       // I2C port number
    int        sda_pin;        // GPIO pin for SDA
    int        scl_pin;        // GPIO pin for SCL
    uint16_t   device_address; // I2C device address (e.g. 0x3C)
    uint32_t   scl_speed_hz;   // I2C clock speed
    uint8_t    width;          // Display width in pixels
    uint8_t    height;         // Display height in pixels
} dev_display_config_t;

typedef struct {
    esp_err_t (*init)(const dev_display_config_t *cfg, dev_display_handle_t *out);
    esp_err_t (*deinit)(dev_display_handle_t h);
    esp_err_t (*clear)(dev_display_handle_t h);
    esp_err_t (*show_text)(dev_display_handle_t h, const char *line1, const char *line2);
    esp_err_t (*set_contrast)(dev_display_handle_t h, uint8_t contrast);
    esp_err_t (*display_on)(dev_display_handle_t h);
    esp_err_t (*display_off)(dev_display_handle_t h);
} dev_display_ops_t;

const dev_display_ops_t *dev_display_get_ops(void);

#ifdef __cplusplus
}
#endif
