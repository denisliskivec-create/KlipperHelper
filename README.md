# KlipperDial

**KlipperDial** is a compact, standalone round IPS mini-monitor for 3D printers running **Klipper / Moonraker**, built on **ESP8266** and a **GC9A01 (240×240)** circular display.

**KlipperDial** — компактный автономный мини-монитор с круглым IPS-экраном для 3D-принтеров на базе **Klipper / Moonraker**, собранный на микроконтроллере **ESP8266** и дисплее **GC9A01 (240×240)**.

---

## ✨ Features / Возможности

* **Real-time Klipper Telemetry / Телеметрия в реальном времени:** Print progress (%), nozzle & bed temperatures (current/target), part cooling fan speed, print status, and estimated remaining time via Moonraker REST API (`port 7125`).
* **Customizable Progress Bar / Настраиваемая шкала прогресса:** Switch between a classic 240° arc (8 to 4 o'clock) or a full 360° circle.
* **Automatic NTP Desk Clock / Автоматические настольные часы:** Automatically switches to a large digital NTP clock with date and standby temperatures when the printer is idle or cooling down.
* **Captive Portal Wi-Fi Setup / Удобная настройка Wi-Fi:** First-boot Access Point (`KlipperDial_Setup`) via `WiFiManager` — no hardcoded credentials needed.
* **Built-in Web Panel (EN / RU) / Встроенная веб-панель (EN / RU):** Configure printer IP, screen rotation, progress style, polling interval, UTC timezone, clock preview (10s test), and interface language directly from your browser.
* **Persistent Storage / Энергонезависимая память:** All settings are saved in `LittleFS`.

---

## 🛠 Hardware & Wiring / Железо и распиновка

### Components / Компоненты
* **MCU:** ESP8266 (NodeMCU v2/v3, Wemos D1 Mini, etc.)
* **Display:** 1.28" Round IPS TFT LCD 240×240 (**GC9A01**, SPI interface)

### Pinout / Схема подключения

| GC9A01 Pin | ESP8266 (NodeMCU) | GPIO | Description / Назначение |
| :--- | :--- | :--- | :--- |
| **VCC** | `3V3` | — | Power 3.3V / Питание 3.3В |
| **GND** | `GND` | — | Ground / Земля |
| **SCL** | `D5` | `GPIO14` | SPI Clock (SCLK) |
| **SDA** | `D7` | `GPIO13` | SPI Data (MOSI) |
| **RES** | `D3` | `GPIO0` | Display Reset |
| **DC** | `D4` | `GPIO2` | Data / Command |
| **CS** | `D8` | `GPIO15` | Chip Select |
| **BLK** | `3V3` *(or NC)* | — | Backlight (always on) / Подсветка |

---

## 💻 Build & Flash (PlatformIO) / Сборка и прошивка

1. Open the project folder in **VS Code** with the **PlatformIO** extension installed.
2. Ensure your `platformio.ini` includes the required libraries and `TFT_eSPI` build flags:

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs

lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    bblanchon/ArduinoJson@^7.2.0
    tzapu/WiFiManager@^2.0.17

build_flags =
    -D USER_SETUP_LOADED=1
    -D GC9A01_DRIVER=1
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=240
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=0
    -D LOAD_GLCD=1
    -D LOAD_FONT2=1
    -D LOAD_FONT4=1
    -D LOAD_FONT6=1
    -D LOAD_GFXFF=1
    -D SMOOTH_FONT=1
    -D SPI_FREQUENCY=27000000