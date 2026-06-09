#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 常量
 * ================================================================ */
#define APP_MUSIC_VOLUME_MIN    0
#define APP_MUSIC_VOLUME_MAX    10
#define APP_MUSIC_VOLUME_DEFAULT 4

#define APP_MUSIC_MAX_TRACKS    16

/* ================================================================
 * 播放模式
 * ================================================================ */
typedef enum {
    APP_MUSIC_MODE_SEQUENTIAL,     /* 顺序播放全部 */
    APP_MUSIC_MODE_SINGLE_REPEAT,  /* 单曲循环 */
    APP_MUSIC_MODE_SHUFFLE,        /* 随机播放 */
    APP_MUSIC_MODE_MAX,
} app_music_play_mode_t;

/* ================================================================
 * 运行时状态（供 OLED 同事读取）
 * ================================================================ */
typedef struct {
    char                  track_name[24];   /* 当前歌曲显示名 */
    uint8_t               track_index;      /* 当前曲目索引 (0-based) */
    uint8_t               track_total;      /* 总曲目数 */
    bool                  is_playing;       /* true=播放中, false=暂停 */
    uint8_t               volume;           /* 当前音量 0-10 */
    app_music_play_mode_t play_mode;        /* 当前播放模式 */
} app_music_state_t;

/* ================================================================
 * 函数声明
 * ================================================================ */

#include "dev_init.h"

/**
 * @brief 初始化音乐播放器
 *        挂载 SPIFFS → 扫描歌曲 → 创建按键 → 开始播放第一首
 *
 * @param handles 由 dev_init_all() 返回的设备句柄
 */
esp_err_t app_music_player_init(const dev_handles_t *handles);

/**
 * @brief 主循环 tick（每 100ms 调用一次）
 *        处理：播放结束自动切歌、LED 效果、光敏轮询
 */
void app_music_player_tick(void);

/**
 * @brief 获取当前播放器状态（OLED 显示用）
 */
const app_music_state_t *app_music_player_get_state(void);

#ifdef __cplusplus
}
#endif
