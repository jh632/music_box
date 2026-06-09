#include "app_music_player.h"

#include "dev_init.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_spiffs.h"

#include <dirent.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "APP_MUSIC";

/* ================================================================
 * 常量
 * ================================================================ */
#define SPIFFS_MOUNT_POINT   "/spiffs"
#define SPIFFS_PART_LABEL    "storage"

#define VOL_INDICATE_TICKS   20             /* 调音量时 LED 显示时长 (20*100ms=2s) */

#define VOL_INTERNAL(ext)    ((ext) * 10)   /* 外部 0-10 → 内部 0-100 */

#define STATE_IS_PLAYING(s)  ((s) == DEV_AUDIO_STATE_PLAYING)
#define STATE_IS_IDLE(s)     ((s) == DEV_AUDIO_STATE_IDLE)

/* ================================================================
 * 播放列表
 * ================================================================ */
typedef struct {
    char name[24];
    char path[264];  /* "/spiffs/" + 255 chars + null */
} track_t;

static track_t   s_tracks[APP_MUSIC_MAX_TRACKS];

/* ================================================================
 * 运行时状态
 * ================================================================ */
static app_music_state_t s_state;
static dev_audio_state_t s_prev_audio_state;

static uint8_t s_vol_indicate_remain;   /* >0 时 LED 显示音量条 */

static bool s_spiffs_mounted;
static bool s_was_dark;

/* 保存由 init() 传入的设备句柄 */
static const dev_handles_t *s_hd;

/* ================================================================
 * LED 跑马灯控制
 * ================================================================ */
static void led_chase(void)
{
    if (s_hd->led_strip) {
        dev_led_strip_get_ops()->chase_step(s_hd->led_strip);
    }
}

static void led_all_off(void)
{
    if (s_hd->led_strip) {
        dev_led_strip_get_ops()->all_off(s_hd->led_strip);
    }
}

/* 根据音量亮起对应数量的 LED */
static void led_show_volume(uint8_t vol)
{
    const dev_led_strip_ops_t *ops = dev_led_strip_get_ops();
    if (!s_hd->led_strip || ops == NULL) {
        return;
    }

    /* vol 范围 0-10, 按比例映射到 0-8 个 LED */
    uint8_t lit = (vol * DEV_LED_MAX + APP_MUSIC_VOLUME_MAX / 2) / APP_MUSIC_VOLUME_MAX;

    for (uint8_t i = 0; i < DEV_LED_MAX; i++) {
        ops->set(s_hd->led_strip, i, i < lit);
    }
}

/* ================================================================
 * 曲目管理
 * ================================================================ */
static void play_track(uint8_t index)
{
    if (index >= s_state.track_total) {
        return;
    }

    const dev_audio_ops_t *audio = dev_audio_get_ops();
    if (s_hd->audio == NULL) {
        return;
    }

    /* 停止当前播放 */
    audio->stop(s_hd->audio);

    s_prev_audio_state  = DEV_AUDIO_STATE_IDLE;

    /* 更新显示状态 */
    strncpy(s_state.track_name, s_tracks[index].name, sizeof(s_state.track_name) - 1);
    s_state.track_name[sizeof(s_state.track_name) - 1] = '\0';
    s_state.track_index = index;
    s_state.is_playing  = true;

    esp_err_t ret = audio->play(s_hd->audio, s_tracks[index].path);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "play %s failed: %s", s_tracks[index].path, esp_err_to_name(ret));
        s_state.is_playing = false;
        return;
    }
    ESP_LOGI(TAG, "playing [%d/%d] %s", index + 1, s_state.track_total, s_tracks[index].name);
}

static void play_next(void)
{
    if (s_state.track_total == 0) {
        return;
    }

    switch (s_state.play_mode) {
    case APP_MUSIC_MODE_SINGLE_REPEAT:
        /* 继续播同一首 */
        break;

    case APP_MUSIC_MODE_SHUFFLE: {
        uint8_t next;
        if (s_state.track_total > 1) {
            do {
                next = esp_random() % s_state.track_total;
            } while (next == s_state.track_index);
        } else {
            next = s_state.track_index;
        }
        s_state.track_index = next;
        break;
    }

    case APP_MUSIC_MODE_SEQUENTIAL:
    default:
        s_state.track_index = (s_state.track_index + 1) % s_state.track_total;
        break;
    }

    play_track(s_state.track_index);
}

static void play_prev(void)
{
    if (s_state.track_total == 0) {
        return;
    }

    /* 上一首只在顺序模式下有意义；随机模式回退到上一曲目索引或重选 */
    switch (s_state.play_mode) {
    case APP_MUSIC_MODE_SHUFFLE:
        s_state.track_index = esp_random() % s_state.track_total;
        break;

    case APP_MUSIC_MODE_SINGLE_REPEAT:
    case APP_MUSIC_MODE_SEQUENTIAL:
    default:
        s_state.track_index = (s_state.track_index == 0) ? s_state.track_total - 1 : s_state.track_index - 1;
        break;
    }

    play_track(s_state.track_index);
}

/* ================================================================
 * 音量
 * ================================================================ */
static void volume_add(int8_t delta)
{
    uint8_t new_vol = s_state.volume + delta;
    if (new_vol < APP_MUSIC_VOLUME_MIN || new_vol > APP_MUSIC_VOLUME_MAX) {
        return;
    }
    s_state.volume = new_vol;
    dev_audio_get_ops()->set_volume(s_hd->audio, VOL_INTERNAL(s_state.volume));
    s_vol_indicate_remain = VOL_INDICATE_TICKS;
    ESP_LOGI(TAG, "volume: %d", s_state.volume);
}

/* ================================================================
 * 播放模式
 * ================================================================ */
static const char *s_mode_names[APP_MUSIC_MODE_MAX] = {
    "顺序", "单曲", "随机",
};

static void cycle_play_mode(void)
{
    s_state.play_mode = (app_music_play_mode_t)((s_state.play_mode + 1) % APP_MUSIC_MODE_MAX);
    ESP_LOGI(TAG, "play mode: %s", s_mode_names[s_state.play_mode]);
}

/* ================================================================
 * 播放/暂停
 * ================================================================ */
static void toggle_play(void)
{
    const dev_audio_ops_t *audio = dev_audio_get_ops();
    if (s_hd->audio == NULL || s_state.track_total == 0) {
        return;
    }

    if (s_state.is_playing) {
        audio->pause(s_hd->audio);
        s_state.is_playing = false;
        led_all_off();
        ESP_LOGI(TAG, "paused");
    } else {
        audio->resume(s_hd->audio);
        s_state.is_playing = true;
        ESP_LOGI(TAG, "resumed");
    }
}

/* ================================================================
 * 按键回调（统一分发）
 * ================================================================ */
static void button_callback(dev_button_handle_t handle,
                            dev_btn_event_t event,
                            void *user_data)
{
    (void)handle;
    uintptr_t btn_id = (uintptr_t)user_data;

    switch (btn_id) {
    case 0: /* KEY1 - 模式切换 */
        cycle_play_mode();
        break;

    case 1: /* KEY2 - 切歌 */
        if (event == DEV_BTN_EVT_SHORT_UP) {
            play_next();
        } else if (event == DEV_BTN_EVT_LONG_UP || event == DEV_BTN_EVT_VERY_LONG_UP) {
            play_prev();
        }
        break;

    case 2: /* KEY3 - 音量 */
        if (event == DEV_BTN_EVT_SHORT_UP) {
            volume_add(1);
        } else if (event == DEV_BTN_EVT_LONG_UP || event == DEV_BTN_EVT_VERY_LONG_UP) {
            volume_add(-1);
        }
        break;

    case 3: /* KEY4 - 播放/暂停 */
        if (event == DEV_BTN_EVT_SHORT_UP) {
            toggle_play();
        }
        break;

    default:
        break;
    }
}

/* ================================================================
 * SPIFFS
 * ================================================================ */
static esp_err_t mount_spiffs(void)
{
    if (s_spiffs_mounted) {
        return ESP_OK;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path              = SPIFFS_MOUNT_POINT,
        .partition_label        = SPIFFS_PART_LABEL,
        .max_files              = 8,
        .format_if_mount_failed = false,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_spiffs_mounted = true;

    size_t total = 0, used = 0;
    esp_spiffs_info(SPIFFS_PART_LABEL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS mounted: %u/%u bytes", (unsigned)used, (unsigned)total);
    return ESP_OK;
}

static void unmount_spiffs(void)
{
    if (s_spiffs_mounted) {
        esp_vfs_spiffs_unregister(SPIFFS_PART_LABEL);
        s_spiffs_mounted = false;
        ESP_LOGI(TAG, "SPIFFS unmounted");
    }
}

/* ================================================================
 * 歌曲扫描
 * ================================================================ */
static int sort_by_name(const void *a, const void *b)
{
    const track_t *ta = (const track_t *)a;
    const track_t *tb = (const track_t *)b;
    return strcasecmp(ta->name, tb->name);
}

static void scan_tracks(void)
{
    DIR *dir = opendir(SPIFFS_MOUNT_POINT);
    if (dir == NULL) {
        ESP_LOGE(TAG, "opendir(%s) failed", SPIFFS_MOUNT_POINT);
        return;
    }

    s_state.track_total = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s_state.track_total < APP_MUSIC_MAX_TRACKS) {
        const char *dot = strrchr(entry->d_name, '.');
        if (dot == NULL) {
            continue;
        }

        if (strcasecmp(dot, ".mp3") != 0) {
            continue;
        }

        /* 复制显示名（去掉扩展名） */
        size_t name_len = (size_t)(dot - entry->d_name);
        if (name_len > sizeof(s_tracks[s_state.track_total].name) - 1) {
            name_len = sizeof(s_tracks[s_state.track_total].name) - 1;
        }
        memcpy(s_tracks[s_state.track_total].name, entry->d_name, name_len);
        s_tracks[s_state.track_total].name[name_len] = '\0';

        snprintf(s_tracks[s_state.track_total].path, sizeof(s_tracks[s_state.track_total].path),
                 "%s/%s", SPIFFS_MOUNT_POINT, entry->d_name);

        s_state.track_total++;
    }

    closedir(dir);

    /* 按文件名排序 */
    qsort(s_tracks, s_state.track_total, sizeof(track_t), sort_by_name);

    ESP_LOGI(TAG, "found %d track(s)", s_state.track_total);
    for (uint8_t i = 0; i < s_state.track_total; i++) {
        ESP_LOGI(TAG, "  [%d] %s", i, s_tracks[i].name);
    }
}

/* ================================================================
 * 光敏传感器
 * ================================================================ */
static void light_sensor_poll(void)
{
    if (!s_hd->light_sensor) {
        return;
    }

    bool is_dark = false;

    esp_err_t ret = dev_light_sensor_get_ops()->get_status(s_hd->light_sensor, &is_dark);
    if (ret != ESP_OK) {
        return;
    }

    if (is_dark && !s_was_dark) {
        /* 变暗 → 关 LED */
        ESP_LOGD(TAG, "light: dark");
        led_all_off();
        s_was_dark = true;
    } else if (!is_dark && s_was_dark) {
        /* 变亮 → 恢复 LED */
        ESP_LOGD(TAG, "light: bright");
        s_was_dark = false;
    }
}

/* ================================================================
 * 创建按键
 * ================================================================ */
static const gpio_num_t s_btn_gpios[DEV_BUTTON_MAX] = {
    DEV_PIN_BTN_MODE,    /* ID 0 */
    DEV_PIN_BTN_TOGGLE,  /* ID 1 */
    DEV_PIN_BTN_VOLUME,  /* ID 2 */
    DEV_PIN_BTN_PLAY,    /* ID 3 */
};

static esp_err_t create_buttons(void)
{
    for (int i = 0; i < DEV_BUTTON_MAX; i++) {
        esp_err_t ret = dev_init_create_button(
            s_btn_gpios[i], 0, button_callback, (void *)(uintptr_t)i,
            &s_hd->buttons[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "button GPIO%d create failed: %s",
                     s_btn_gpios[i], esp_err_to_name(ret));
            return ret;
        }
    }
    return ESP_OK;
}

/* ================================================================
 * 公开 API
 * ================================================================ */
esp_err_t app_music_player_init(const dev_handles_t *handles)
{
    if (handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_hd = handles;

    esp_err_t ret;

    /* 1. 挂载 SPIFFS */
    ret = mount_spiffs();
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. 扫描歌曲 */
    scan_tracks();
    if (s_state.track_total == 0) {
        ESP_LOGW(TAG, "no tracks found");
    }

    /* 3. 初始化状态 */
    s_state.volume     = APP_MUSIC_VOLUME_DEFAULT;
    s_state.play_mode  = APP_MUSIC_MODE_SEQUENTIAL;
    s_state.is_playing = false;

    /* 4. 创建按键 */
    ret = create_buttons();
    if (ret != ESP_OK) {
        unmount_spiffs();
        return ret;
    }

    /* 5. LED 全灭 */
    led_all_off();

    /* 6. 播放第一首 */
    if (s_state.track_total > 0) {
        play_track(0);
    }

    ESP_LOGI(TAG, "music player initialized");
    return ESP_OK;
}

void app_music_player_tick(void)
{
    /* ==============================================================
     * 1. 检测音频是否结束 → 自动下一首
     * ============================================================== */
    dev_audio_state_t audio_state;
    if (s_hd->audio && s_state.is_playing &&
        dev_audio_get_ops()->get_state(s_hd->audio, &audio_state) == ESP_OK) {

        if (STATE_IS_PLAYING(s_prev_audio_state) && STATE_IS_IDLE(audio_state)) {
            /* 歌曲播放完毕 */
            ESP_LOGI(TAG, "track finished");
            play_next();
        }
        s_prev_audio_state = audio_state;
    }

    /* ==============================================================
     * 2. LED 指示
     * ============================================================== */
    if (s_vol_indicate_remain > 0) {
        /* 正在显示音量条 */
        s_vol_indicate_remain--;
        led_show_volume(s_state.volume);
    } else if (s_state.is_playing) {
        /* 播放中：跑马灯 */
        led_chase();
    } else {
        /* 暂停：全灭 */
        led_all_off();
    }

    /* ==============================================================
     * 3. 光敏传感器
     * ============================================================== */
    light_sensor_poll();
}

const app_music_state_t *app_music_player_get_state(void)
{
    return &s_state;
}
