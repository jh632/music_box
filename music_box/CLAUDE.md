# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 music box: plays MP3 audio via I2S (MAX98357A amp), with button input, LED strip control, light sensor, and OLED display (SSD1306 128x64).

- **Target**: ESP32-S3 (ESP-IDF 5.5.4)
- **Flash**: 8MB, custom partition table (3M factory + 4M SPIFFS for audio files)
- **PSRAM**: Enabled

## Build Commands

```bash
idf.py build              # Build everything
idf.py flash              # Flash firmware
idf.py monitor            # Serial monitor
idf.py menuconfig         # Configure project
idf.py clean / fullclean  # Clean build artifacts
idf.py spiffs-flash       # Flash the SPIFFS partition containing MP3 files
```

## Architecture

Three-layer design with ops + handle pattern throughout:

### 1. HAL Layer (`components/music_hal/`)
Wraps ESP-IDF driver APIs behind ops tables. Each module exposes `const xxx_ops_t *hal_xxx_get_ops(void)`:

| Module | File | Wraps |
|--------|------|-------|
| `hal_gpio` | `hal_gpio.c/h` | ESP-IDF `gpio_config/set_level/get_level` + ISR |
| `hal_i2s` | `hal_i2s.c/h` | ESP-IDF `i2s_std` TX channel |
| `hal_i2c` | `hal_i2c.c/h` | ESP-IDF `i2c_master` bus + device |
| `hal_rmt` | `hal_rmt.c/h` | ESP-IDF `rmt_tx` with byte-stream encoder |
| `hal_timer` | `hal_timer.c/h` | ESP-IDF `esp_timer` (create/start/stop/delete) |
| `hal_adc` | `hal_adc.c/h` | ESP-IDF `adc_oneshot` + calibration |

### 2. Device Layer (`components/device/`)
Device drivers built on HAL ops tables. Same pattern: `const xxx_ops_t *dev_xxx_get_ops(void)`.

| Module | File | Hardware |
|--------|------|----------|
| `dev_audio` | `dev_audio.c/h` | MAX98357A I2S amplifier (BCLK=GPIO16, WS=GPIO17, DOUT=GPIO18, SD=GPIO15) |
| `dev_button` | `dev_button.c/h` | GPIO button with debounce & multi-press detection (SHORT/DOUBLE/LONG/VERY_LONG) |
| `dev_led_strip` | `dev_led_strip.c/h` | Individual GPIO-driven LEDs (not WS2812) |
| `dev_light_sensor` | `dev_light_sensor.c/h` | Digital light sensor (GPIO DO pin, DO low = dark) |
| `dev_display` | `dev_display.c` + `OLED.h` | SSD1306 OLED 128x64 via I2C (SCL=GPIO41, SDA=GPIO42, addr=0x3C) |
| `dev_init` | `dev_init.c/h` | Aggregator: inits all devices and returns a `dev_handles_t` struct |

### 3. Application Layer (`components/application/`)
- `app_audio_verify` — mounts SPIFFS, plays an MP3 file via `dev_audio`, waits for completion, cleans up.

### Entry Point (`main/main.c`)
Minimal: init NVS → `app_audio_verify_run()` → loop with memory usage logging.

## Architecture Patterns

- **ops + handle**: Every module exposes a const ops table (virtual methods) and an opaque handle type. Consumers get ops via `xxx_get_ops()` and pass the handle as first argument (C-style OOP).
- **Singleton handles**: `s_audio_handle`, `s_tx_chan`, `s_bus`, `s_rmt` — one instance per peripheral, enforced by state checks.
- **Invalid arg checks**: Every public function validates pointers and returns `ESP_ERR_INVALID_ARG` / `ESP_ERR_INVALID_STATE`.
- **Component dependencies**: `main` → `application` → `device` → `music_hal` + `esp-audio-player` + `esp_driver_*`.

## GPIO Pin Map

| Pin | Function |
|-----|----------|
| 15  | MAX98357A SD (shutdown) |
| 16  | I2S BCLK |
| 17  | I2S WS |
| 18  | I2S DOUT |
| 41  | OLED SCL |
| 42  | OLED SDA |

## External Dependencies (managed_components/)

- `chmorgan/esp-audio-player` — audio playback framework (MP3 via Helix, WAV)
- `chmorgan/esp-libhelix-mp3` — Helix MP3 decoder

## SPIFFS

Audio files live in `spiffs/` directory at project root. Built to a 4M `storage` partition. Current files: Balloon.mp3, Canon.mp3, For_Elise.mp3, gdstory.mp3, Your_name.mp3.
