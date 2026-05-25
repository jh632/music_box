# 本次工作报告：HAL 层基础构建

## 1. 工作背景

本次工作围绕 `music_box` 工程的 HAL 层进行基础构建。

项目当前使用：

- ESP-IDF：v5.5.4
- 不使用 LVGL

本次遵循 `DOC` 目录下的三份项目文档要求，重点保持三层架构边界：

```text
Application -> Device -> HAL -> ESP-IDF
```

因此，本次只实现 HAL 层的通用封装，不在 HAL 中写死项目具体 GPIO，也不创建 `dev_init_all()` 或 Application 层逻辑。

## 2. 已完成内容

### 2.1 GPIO HAL

保留并完善了已有 `hal_gpio` 模块。

主要能力：

- 单个 GPIO 初始化
- 输出电平设置
- 输入电平读取
- GPIO ISR 服务安装
- ISR 回调注册与移除
- 新增 `hal_gpio_ops_t` 和 `hal_gpio_get_ops()`，方便后续 Device 层按 ops 模式调用

涉及文件：

```text
components/music_box_hal/include/hal_gpio.h
components/music_box_hal/src/hal_gpio.c
```

### 2.2 I2C HAL

新增 `hal_i2c` 模块，使用 ESP-IDF v5.5.4 推荐的新 I2C master API：

```c
#include "driver/i2c_master.h"
```

主要能力：

- 初始化 I2C master bus
- 删除 I2C master bus
- 添加 I2C device
- 移除 I2C device
- I2C transmit
- I2C transmit_receive
- 寄存器读 `read_reg`
- 寄存器写 `write_reg`
- 提供 `hal_i2c_ops_t` / `hal_i2c_get_ops()`

注意：

- 没有在 HAL 中写 OLED 地址。
- 没有在 HAL 中写 GPIO41/GPIO42。
- OLED 地址、SDA/SCL、速率等应由后续 Device 层配置传入。

涉及文件：

```text
components/music_box_hal/include/hal_i2c.h
components/music_box_hal/src/hal_i2c.c
```

### 2.3 I2S HAL

新增 `hal_i2s` 模块，使用 ESP-IDF v5.5.4 推荐的新 I2S standard API：

```c
#include "driver/i2s_std.h"
```

本项目音频功放只需要 I2S TX 输出，因此本次只封装 standard TX。

主要能力：

- 初始化 I2S standard TX
- 删除 I2S channel
- enable
- disable
- write
- preload
- 提供 `hal_i2s_ops_t` / `hal_i2s_get_ops()`

注意：

- 没有在 HAL 中写 GPIO16/GPIO17/GPIO18。
- 没有在 I2S HAL 中处理功放 SD 关断脚。
- 功放 SD 脚应由后续 `dev_audio` 通过 `hal_gpio` 管理。

涉及文件：

```text
components/music_box_hal/include/hal_i2s.h
components/music_box_hal/src/hal_i2s.c
```

### 2.4 RMT HAL

新增 `hal_rmt` 模块，使用 ESP-IDF v5.5.4 推荐的新 RMT TX API：

```c
#include "driver/rmt_tx.h"
```

主要能力：

- 初始化 RMT TX channel
- 创建字节流编码器
- enable
- disable
- transmit
- wait_done
- deinit
- 提供 `hal_rmt_ops_t` / `hal_rmt_get_ops()`

注意：

- 头文件中没有出现具体灯珠型号。
- 没有在 HAL 中写 GPIO48。
- 单线 RGB 时序由 `hal_rmt_tx_config_t` 传入，后续 Device 层可按实际灯珠配置。

涉及文件：

```text
components/music_box_hal/include/hal_rmt.h
components/music_box_hal/src/hal_rmt.c
```

### 2.5 Timer HAL

新增 `hal_timer` 模块，基于 ESP-IDF 的 `esp_timer`。

主要能力：

- 创建定时器
- 删除定时器
- 启动单次定时器
- 启动周期定时器
- 停止定时器
- 提供 `hal_timer_ops_t` / `hal_timer_get_ops()`

后续可用于：

- 按键消抖
- 长按/双击判断
- 灯效节拍

涉及文件：

```text
components/music_box_hal/include/hal_timer.h
components/music_box_hal/src/hal_timer.c
```

## 3. CMake 与组件命名调整

最初自定义 HAL 组件目录为：

```text
components/hal
```

这个名字会和 ESP-IDF 自带的 `hal` 组件冲突。

实际编译时出现过类似错误：

```text
fatal error: hal/sha_types.h: No such file or directory
```

原因是 ESP-IDF 自己的组件需要包含：

```c
#include "hal/sha_types.h"
```

但工程中的 `components/hal` 抢占了 ESP-IDF 内置 `hal` 组件名，导致 IDF 内部头文件解析异常。

为避免冲突，已将自定义组件目录改为：

```text
components/music_box_hal
```

同时顶层 `CMakeLists.txt` 已恢复为 ESP-IDF 常规写法：

```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(music_box)
```

HAL 组件自身 CMake 已包含新增源码和依赖：

```text
components/music_box_hal/CMakeLists.txt
```

## 4. 当前 HAL 层边界

本次 HAL 层只做“通用硬件封装”，不做具体业务初始化。

因此，目前没有在 HAL 中固定以下项目引脚：

```text
GPIO0   Boot 按键
GPIO15  功放 SD
GPIO16  I2S BCLK
GPIO17  I2S LRC/WS
GPIO18  I2S DIN
GPIO41  OLED SDA
GPIO42  OLED SCL
GPIO48  RGB DATA
```

这些引脚应在后续 Device 层中集中配置，例如：

```text
components/device/src/dev_init.c
```

这符合项目文档中“配置参数集中在该层的 _init_all() 内”的要求。

## 5. 静态检查结果

本次做过的静态检查包括：

- `.c` 文件只包含自身对应的 `.h`
- HAL 头文件未出现具体芯片型号 `SSD1306`、`MAX98357A`、`WS2812B`
- HAL 源码未硬编码项目 GPIO 编号
- `main.c` 未修改
- 未创建 Device/Application 层
- 未主动烧录

项目后来已经能执行到 `idf_size.py` 输出体积统计，说明此前 `hal/sha_types.h` 的组件名冲突已经解决。

## 6. 后续协作建议

下一步建议其他协作者从 Device 层开始：

1. 新建 `components/device`
2. 添加 `dev_init`
3. 在 `dev_init_all()` 中集中配置本项目实际引脚
4. 由 Device 层调用 `hal_gpio`、`hal_i2c`、`hal_i2s`、`hal_rmt`、`hal_timer`
5. `main.c` 仍只负责调用 `dev_init_all()` 和后续 `app_init_all()`

建议优先顺序：

```text
dev_init -> dev_button -> dev_display -> dev_audio -> dev_led_strip
```

其中 I2C 总线建议在 `dev_init_all()` 开头初始化一次，再交给 OLED 相关 Device 模块使用。

## 7. 本次未做内容

本次没有做以下内容：

- 没有实现 `dev_init_all()`
- 没有初始化具体 GPIO 引脚
- 没有实现 OLED 驱动
- 没有实现音频播放逻辑
- 没有实现灯效逻辑
- 没有实现按键状态机
- 没有改动 Application 层
- 没有引入 LVGL

以上内容应在后续 Device/Application 阶段继续实现。
