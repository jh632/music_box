# 本次工作状态汇报

## 当前状态

本次已按最新接口要求调整 `components/music_box_hal`：

- HAL 头文件只保留 `ops` 操作表和 `_get_ops()`
- HAL 具体实现函数已收回到 `.c` 内部，改为 `static`
- HAL 不再对外暴露自定义 handle

## 本次主要变更

### 1. HAL 统一改为 ops 模式

以下模块当前都通过操作表对外提供能力：

- `hal_gpio`
- `hal_i2c`
- `hal_i2s`
- `hal_rmt`
- `hal_timer`

头文件对外只保留：

- `xxx_ops_t`
- `xxx_get_ops()`

不再同时暴露普通函数声明。

### 2. handle 已收回模块内部

以下 HAL 模块不再对外暴露 handle：

- `hal_i2c`
- `hal_i2s`
- `hal_rmt`
- `hal_timer`

相关状态和底层句柄已改为模块内静态保存，由 HAL 自己管理。

### 3. `.c` 内实现已隐藏

HAL 各模块内部实现函数已改为 `static`，调用方只能通过 `ops->method(...)` 使用。

## 涉及文件

```text
components/music_box_hal/include/hal_gpio.h
components/music_box_hal/include/hal_i2c.h
components/music_box_hal/include/hal_i2s.h
components/music_box_hal/include/hal_rmt.h
components/music_box_hal/include/hal_timer.h
components/music_box_hal/src/hal_gpio.c
components/music_box_hal/src/hal_i2c.c
components/music_box_hal/src/hal_i2s.c
components/music_box_hal/src/hal_rmt.c
components/music_box_hal/src/hal_timer.c
```

## 当前未做

- 未新增 Device 层代码
- 未修改 `main.c`
- 未执行编译 / 烧录 / 运行
- 未处理 Device 层对新接口的对接

---

## 按键硬件规划补充

本次按最新原理图规划补充 4 个独立按键，GPIO0 的 Boot 按键仅保留为下载/启动相关用途，不再复用为业务按键。

| GPIO | 按键 | 功能 | 接法 |
|------|------|------|------|
| GPIO11 | KEY_MODE | 自动/手动模式 | GPIO → 按键 → GND，内部上拉，低有效 |
| GPIO12 | KEY_NEXT | 切歌 | GPIO → 按键 → GND，内部上拉，低有效 |
| GPIO13 | KEY_VOL | 音量控制 | GPIO → 按键 → GND，内部上拉，低有效 |
| GPIO14 | KEY_PLAY | 播放/暂停 | GPIO → 按键 → GND，内部上拉，低有效 |

说明：当前不预留外部上拉电阻，后续软件初始化按键 GPIO 时需要开启 ESP32-S3 内部上拉，并按低电平表示按下处理。
