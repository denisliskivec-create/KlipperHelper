# KlipperDial

**English:**
**KlipperDial** is a compact, standalone mini-monitor for 3D printers running **Klipper and Moonraker**, inspired by circular Bambu Lab companion displays (like BambuHelper).
Built on an **ESP8266** microcontroller and a **1.28" round IPS display (GC9A01, 240×240)**, it shows real-time print progress, nozzle/bed temperatures, fan speed, and remaining time.
When the printer is idle or cooling down, the screen automatically turns into a minimalist digital **NTP desk clock**.
All settings—including Wi-Fi, printer IP, progress bar style, rotation, and language—are easily configured via the built-in **Web UI**.

**Русский:**
**KlipperDial** — компактный автономный мини-монитор для 3D-принтеров на базе **Klipper и Moonraker**, вдохновлённый круглыми вспомогательными дисплеями в стиле **Bambu Lab (BambuHelper)**.
Устройство собрано на микроконтроллере **ESP8266** и круглом **1.28" IPS-дисплее (GC9A01, 240×240)** и в реальном времени отображает прогресс печати, температуры сопла и стола, скорость обдува и оставшееся время.
Когда принтер простаивает или остывает, экран автоматически переключается в режим лаконичных настольных **NTP-часов** с датой.
Все параметры — от Wi-Fi и IP принтера до ориентации экрана, шкалы прогресса и языка — настраиваются через встроенную **веб-панель** без перепрошивки.

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

## 💻 Build & Flash / Сборка и прошивка

1. Download or clone this repository / Скачайте или клонируйте этот репозиторий.
2. Open the project folder in **VS Code** with the **PlatformIO** extension installed / Откройте папку проекта в **VS Code** с установленным расширением **PlatformIO**.
3. Connect your ESP8266 via USB and click **PlatformIO: Upload** (all libraries and `TFT_eSPI` flags are already configured in `platformio.ini`) / Подключите плату ESP8266 по USB и нажмите **PlatformIO: Upload** (все библиотеки и настройки дисплея уже прописаны в `platformio.ini`).

---

## 🚀 First Setup / Первый запуск

1. Power on the device. On first boot, it will create a Wi-Fi Access Point named **`KlipperDial_Setup`**.
   *(Включите питание устройства. При первом запуске оно создаст точку доступа Wi-Fi с именем **`KlipperDial_Setup`**).*
2. Connect to **`KlipperDial_Setup`** from your phone or PC and open `192.168.4.1` (if the captive portal does not pop up automatically).
   *(Подключитесь к **`KlipperDial_Setup`** со смартфона или ПК и перейдите по адресу `192.168.4.1`, если окно настройки не открылось само).*
3. Select your home Wi-Fi network, enter the password, and specify your **Klipper printer's local IP address**.
   *(Выберите домашнюю сеть Wi-Fi, введите пароль и укажите **локальный IP-адрес вашего принтера**).*
4. Once connected, the screen will display the monitor's own IP address for 3 seconds. Open this IP in any browser to access the **KlipperDial Web Settings**.
   *(После подключения экран на 3 секунды покажет собственный IP-адрес монитора. Откройте его в любом браузере для доступа к **веб-панели настроек KlipperDial**).*

---

## 🔧 Fan Speed Compatibility Note / Настройка отображения вентилятора

Different Klipper printers expose the part cooling fan under different Moonraker objects:
* **Creality K1 / K2 series:** Uses `output_pin fan0` *(enabled by default)*.
* **Standard Klipper (Voron, Ender, Sovol, etc.):** Uses the `[fan]` section (`fan.speed`).

If your printer always displays `FAN: 0%`, open `src/main.cpp`, locate `fetchPrinterData()`, and switch the commented lines from `output_pin fan0` to `fan` as described in the inline comments.

---

## 📄 License / Лицензия

MIT License — free to use, modify, and share.
