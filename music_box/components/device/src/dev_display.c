#include "OLED.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

#define OLED_I2C_PORT        I2C_NUM_0
#define OLED_I2C_SCL_GPIO    GPIO_NUM_41
#define OLED_I2C_SDA_GPIO    GPIO_NUM_42
#define OLED_I2C_FREQ_HZ     400000
#define OLED_I2C_ADDR        0x3C
#define OLED_I2C_TIMEOUT_MS  1000

#define OLED_WIDTH           128
#define OLED_HEIGHT          64
#define OLED_PAGES           (OLED_HEIGHT / 8)
#define OLED_BUF_SIZE        (OLED_WIDTH * OLED_PAGES)

static const char *TAG = "OLED";

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_oled_dev;
static bool                    s_oled_inited;
static uint8_t                 s_oled_buf[OLED_BUF_SIZE];

static void oled_draw_char_no_refresh(uint8_t line, uint8_t column, char ch);

//我这版是 5x7 字模放大到约 10x5，然后放在一个 16 高、8 宽的字符格 里显示。
/* 5x7 ASCII 字模，按 8x16 字符格显示，未列出的字符显示为空格。 */
static const uint8_t s_font_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t s_font_digits[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};
static const uint8_t s_font_letters[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t *oled_get_font(char ch)
{
    static const uint8_t minus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t plus[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
    static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
    static const uint8_t slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};

    if (ch >= '0' && ch <= '9') {
        return s_font_digits[ch - '0'];
    }
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }
    if (ch >= 'A' && ch <= 'Z') {
        return s_font_letters[ch - 'A'];
    }
    switch (ch) {
    case '-':
        return minus;
    case '+':
        return plus;
    case ':':
        return colon;
    case '.':
        return dot;
    case '/':
        return slash;
    default:
        return s_font_space;
    }
}

static esp_err_t oled_write(uint8_t control, const uint8_t *data, size_t len)
{
    uint8_t packet[33];

    if (len > sizeof(packet) - 1) {
        return ESP_ERR_INVALID_SIZE;
    }

    packet[0] = control;
    if (data != NULL && len > 0) {
        memcpy(&packet[1], data, len);
    }

    return i2c_master_transmit(s_oled_dev, packet, len + 1, OLED_I2C_TIMEOUT_MS);
}

static esp_err_t oled_cmd(uint8_t cmd)
{
    return oled_write(0x00, &cmd, 1);
}

static esp_err_t oled_cmd2(uint8_t cmd, uint8_t value)
{
    uint8_t data[] = {cmd, value};

    return oled_write(0x00, data, sizeof(data));
}

static esp_err_t oled_data(const uint8_t *data, size_t len)
{
    while (len > 0) {
        size_t chunk = len > 32 ? 32 : len;
        ESP_RETURN_ON_ERROR(oled_write(0x40, data, chunk), TAG, "write OLED data failed");
        data += chunk;
        len -= chunk;
    }

    return ESP_OK;
}

static void oled_i2c_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = OLED_I2C_PORT,
        .sda_io_num = OLED_I2C_SDA_GPIO,
        .scl_io_num = OLED_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_i2c_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = OLED_I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_oled_dev));
}

static void oled_controller_init(void)
{
    ESP_ERROR_CHECK(oled_cmd(0xAE));
    ESP_ERROR_CHECK(oled_cmd2(0xD5, 0x80));
    ESP_ERROR_CHECK(oled_cmd2(0xA8, 0x3F));
    ESP_ERROR_CHECK(oled_cmd2(0xD3, 0x00));
    ESP_ERROR_CHECK(oled_cmd(0x40));
    ESP_ERROR_CHECK(oled_cmd2(0x8D, 0x14));
    ESP_ERROR_CHECK(oled_cmd2(0x20, 0x00));
    ESP_ERROR_CHECK(oled_cmd(0xA1));
    ESP_ERROR_CHECK(oled_cmd(0xC8));
    ESP_ERROR_CHECK(oled_cmd2(0xDA, 0x12));
    ESP_ERROR_CHECK(oled_cmd2(0x81, 0xCF));
    ESP_ERROR_CHECK(oled_cmd2(0xD9, 0xF1));
    ESP_ERROR_CHECK(oled_cmd2(0xDB, 0x40));
    ESP_ERROR_CHECK(oled_cmd(0xA4));
    ESP_ERROR_CHECK(oled_cmd(0xA6));
    ESP_ERROR_CHECK(oled_cmd(0xAF));
}

static void oled_draw_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }

    uint16_t index = x + (y / 8) * OLED_WIDTH;
    uint8_t mask = 1U << (y % 8);

    if (on) {
        s_oled_buf[index] |= mask;
    } else {
        s_oled_buf[index] &= (uint8_t)~mask;
    }
}

static void oled_draw_hline(int x0, int x1, int y, bool on)
{
    if (x0 > x1) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }

    for (int x = x0; x <= x1; x++) {
        oled_draw_pixel(x, y, on);
    }
}

static void oled_draw_circle_ring(int cx, int cy, int r, int inner_r, bool on)
{
    int r2 = r * r;
    int inner_r2 = inner_r * inner_r;

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 <= r2 && d2 >= inner_r2) {
                oled_draw_pixel(cx + dx, cy + dy, on);
            }
        }
    }
}

static void oled_fill_circle(int cx, int cy, int r, bool on)
{
    int r2 = r * r;

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r2) {
                oled_draw_pixel(cx + dx, cy + dy, on);
            }
        }
    }
}

typedef enum {
    EYE_STYLE_OPEN = 0,
    EYE_STYLE_BLINK,
    EYE_STYLE_WINK,
} eye_style_t;

typedef struct {
    int8_t      cx;
    int8_t      cy;
    int8_t      pupil_dx;
    int8_t      pupil_dy;
    uint8_t     outer_r;
    uint8_t     inner_r;
    uint8_t     pupil_r;
    eye_style_t style;
} eye_pose_t;

static void oled_draw_eye_pose(const eye_pose_t *eye)
{
    switch (eye->style) {
    case EYE_STYLE_OPEN:
        oled_draw_circle_ring(eye->cx, eye->cy, eye->outer_r, eye->inner_r, true);
        oled_fill_circle(eye->cx + eye->pupil_dx, eye->cy + eye->pupil_dy, eye->pupil_r, true);
        break;
    case EYE_STYLE_BLINK:
        /* 两只眼睛一起眨一下，用短弧表示，别画成横线。 */
        oled_draw_hline(eye->cx - 10, eye->cx - 6, eye->cy + 2, true);
        oled_draw_hline(eye->cx - 8, eye->cx - 3, eye->cy + 1, true);
        oled_draw_hline(eye->cx - 5, eye->cx + 5, eye->cy, true);
        oled_draw_hline(eye->cx + 3, eye->cx + 8, eye->cy + 1, true);
        oled_draw_hline(eye->cx + 6, eye->cx + 10, eye->cy + 2, true);
        break;
    case EYE_STYLE_WINK:
        /* 右眼 wink 画成上扬粗弧，避免像普通闭眼横线。 */
        oled_draw_hline(eye->cx - 14, eye->cx - 9, eye->cy + 4, true);
        oled_draw_hline(eye->cx - 13, eye->cx - 7, eye->cy + 3, true);
        oled_draw_hline(eye->cx - 10, eye->cx - 4, eye->cy + 1, true);
        oled_draw_hline(eye->cx - 7, eye->cx + 1, eye->cy - 1, true);
        oled_draw_hline(eye->cx - 2, eye->cx + 6, eye->cy - 2, true);
        oled_draw_hline(eye->cx + 4, eye->cx + 11, eye->cy - 1, true);
        oled_draw_hline(eye->cx + 9, eye->cx + 14, eye->cy + 1, true);
        oled_draw_hline(eye->cx + 10, eye->cx + 15, eye->cy + 2, true);
        break;
    default:
        break;
    }
}

static void oled_draw_smile_mouth(void)
{
    /* 固定浅笑，不随动画开合。 */
    oled_draw_hline(49, 54, 51, true);
    oled_draw_hline(53, 59, 53, true);
    oled_draw_hline(58, 70, 55, true);
    oled_draw_hline(69, 75, 53, true);
    oled_draw_hline(74, 79, 51, true);
}

static void oled_draw_text(uint8_t line, uint8_t column, const char *text)
{
    if (text == NULL || line < 1 || line > 4 || column < 1 || column > 16) {
        return;
    }

    for (uint8_t i = 0; text[i] != '\0' && column + i <= 16; i++) {
        oled_draw_char_no_refresh(line, column + i, text[i]);
    }
}

static void oled_draw_char_no_refresh(uint8_t line, uint8_t column, char ch)
{
    if (line < 1 || line > 4 || column < 1 || column > 16) {
        return;
    }

    int x = (column - 1) * 8;
    int y = (line - 1) * 16 + 1;
    const uint8_t *glyph = oled_get_font(ch);

    /* 写新字符前先清空当前位置，避免旧字符像素残留。 */
    for (int dx = 0; dx < 8; dx++) {
        for (int dy = 0; dy < 16; dy++) {
            oled_draw_pixel(x + dx, (line - 1) * 16 + dy, false);
        }
    }

    /* 字符高度放大 2 倍，适配 16 像素行高。 */
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            bool on = (glyph[col] & (1U << row)) != 0;
            oled_draw_pixel(x + col, y + row * 2, on);
            oled_draw_pixel(x + col, y + row * 2 + 1, on);
        }
    }
}

static void oled_refresh(void)
{
    ESP_ERROR_CHECK(oled_cmd(0x21));
    ESP_ERROR_CHECK(oled_cmd(0));
    ESP_ERROR_CHECK(oled_cmd(OLED_WIDTH - 1));
    ESP_ERROR_CHECK(oled_cmd(0x22));
    ESP_ERROR_CHECK(oled_cmd(0));
    ESP_ERROR_CHECK(oled_cmd(OLED_PAGES - 1));
    ESP_ERROR_CHECK(oled_data(s_oled_buf, sizeof(s_oled_buf)));
}

void OLED_ShowMusicAnimation(void)
{
    static uint8_t frame_index;
    static const eye_pose_t frames[][2] = {
        /* 正常睁眼。 */
        {
            {40, 24, 0, 0, 20, 17, 9, EYE_STYLE_OPEN},
            {88, 24, 0, 0, 20, 17, 9, EYE_STYLE_OPEN},
        },
        /* 两只眼睛同时眨一下。 */
        {
            {40, 24, 0, 0, 20, 17, 9, EYE_STYLE_BLINK},
            {88, 24, 0, 0, 20, 17, 9, EYE_STYLE_BLINK},
        },
        /* 眨完恢复，准备 wink。 */
        {
            {40, 24, 0, 0, 20, 17, 9, EYE_STYLE_OPEN},
            {88, 24, 0, 0, 20, 17, 9, EYE_STYLE_OPEN},
        },
        /* 右眼开始 wink。 */
        {
            {40, 24, 0, 0, 20, 17, 9, EYE_STYLE_OPEN},
            {88, 24, 0, 0, 0, 0, 0, EYE_STYLE_WINK},
        },
        /* wink 维持一下。 */
        {
            {40, 24, 0, 0, 20, 17, 9, EYE_STYLE_OPEN},
            {88, 24, 0, 0, 0, 0, 0, EYE_STYLE_WINK},
        },
        /* 恢复正常。 */
        {
            {40, 24, 0, 0, 20, 17, 9, EYE_STYLE_OPEN},
            {88, 24, 0, 0, 20, 17, 9, EYE_STYLE_OPEN},
        },
    };
    uint8_t frame_count = (uint8_t)(sizeof(frames) / sizeof(frames[0]));
    uint8_t frame = frame_index % frame_count;

    memset(s_oled_buf, 0, sizeof(s_oled_buf));

    oled_draw_eye_pose(&frames[frame][0]);
    oled_draw_eye_pose(&frames[frame][1]);
    oled_draw_smile_mouth();

    oled_refresh();
    frame_index = (uint8_t)((frame_index + 1) % frame_count);
}

void OLED_ShowMusicInfo(const char *track_name,
                        uint8_t track_index,
                        uint8_t track_total,
                        uint8_t volume,
                        bool is_playing,
                        bool is_auto)
{
    char line2[17];
    char line3[17];
    char line4[17];
    const char *mode_text = is_auto ? "AUTO" : "MANUAL";
    const char *state_text = is_playing ? "PLAY" : "PAUSE";

    memset(s_oled_buf, 0, sizeof(s_oled_buf));

    oled_draw_text(1, 1, track_name);
    if (track_total == 0) {
        snprintf(line2, sizeof(line2), "IDX:--/--");
    } else {
        snprintf(line2, sizeof(line2), "IDX:%02u/%02u", (unsigned)(track_index + 1), (unsigned)track_total);
    }
    snprintf(line3, sizeof(line3), "MODE:%s", mode_text);
    snprintf(line4, sizeof(line4), "%s VOL:%02u", state_text, (unsigned)volume);

    oled_draw_text(2, 1, line2);
    oled_draw_text(3, 1, line3);
    oled_draw_text(4, 1, line4);

    oled_refresh();
}

static uint32_t oled_pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1;

    while (y-- > 0) {
        result *= x;
    }

    return result;
}

void OLED_Init(void)
{
    if (s_oled_inited) {
        OLED_Clear();
        return;
    }

    oled_i2c_init();
    oled_controller_init();
    s_oled_inited = true;
    OLED_Clear();
    ESP_LOGI(TAG, "OLED initialized: addr 0x%02X, SCL GPIO%d, SDA GPIO%d",
             OLED_I2C_ADDR, OLED_I2C_SCL_GPIO, OLED_I2C_SDA_GPIO);
}

void OLED_Clear(void)
{
    memset(s_oled_buf, 0, sizeof(s_oled_buf));
    oled_refresh();
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    oled_draw_char_no_refresh(Line, Column, Char);
    oled_refresh();
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    if (String == NULL) {
        return;
    }

    for (uint8_t i = 0; String[i] != '\0' && Column + i <= 16; i++) {
        oled_draw_char_no_refresh(Line, Column + i, String[i]);
    }
    oled_refresh();
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    for (uint8_t i = 0; i < Length; i++) {
        char ch = (char)(Number / oled_pow(10, Length - i - 1) % 10 + '0');
        oled_draw_char_no_refresh(Line, Column + i, ch);
    }
    oled_refresh();
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint32_t number_abs;

    if (Number >= 0) {
        oled_draw_char_no_refresh(Line, Column, '+');
        number_abs = (uint32_t)Number;
    } else {
        oled_draw_char_no_refresh(Line, Column, '-');
        number_abs = (uint32_t)(-(Number + 1)) + 1;
    }

    for (uint8_t i = 0; i < Length; i++) {
        char ch = (char)(number_abs / oled_pow(10, Length - i - 1) % 10 + '0');
        oled_draw_char_no_refresh(Line, Column + i + 1, ch);
    }
    oled_refresh();
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    for (uint8_t i = 0; i < Length; i++) {
        uint8_t digit = Number / oled_pow(16, Length - i - 1) % 16;
        char ch = digit < 10 ? (char)(digit + '0') : (char)(digit - 10 + 'A');
        oled_draw_char_no_refresh(Line, Column + i, ch);
    }
    oled_refresh();
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    for (uint8_t i = 0; i < Length; i++) {
        char ch = (char)(Number / oled_pow(2, Length - i - 1) % 2 + '0');
        oled_draw_char_no_refresh(Line, Column + i, ch);
    }
    oled_refresh();
}
