# Photo Frame CL01 (Spectra/Spectre Color E-ink)

E-ink digital picture frame with WiFi-enabled maintenance mode, color rendering and deep sleep operation with RTC backup for extended battery life.

> Display target in this repository is the color e-paper path (`EPD_7IN3E`, 800x480), also commonly referred to as Spectra/Spectre class display.

> **📌 Note:** This project is designed for the newer **LilyGo T5 4.7" E-Paper Plus** (ESP32-S3). For the older **LilyGo T5 4.7" E-Paper** (WROVER-E) version, check out [PhotoFrameGS01](https://github.com/tokosattila/PhotoFrameGS01.git).

## 📸 Gallery

| <img src="docs/images/pic01.jpg" width="240px" alt="Photo Frame Display" /> | <img src="docs/images/pic02.jpg" width="240px" alt="Photo Frame Hardware" /> | <img src="docs/images/pic03.jpg" width="240px" alt="Photo Frame Back Side" /> |
|:---:|:---:|:---:|
| *Photo Frame with Image* | *Hardware backside* | *Backside covered* |

## 🔧 Hardware

<table width="100%">
<tr>
<td align="center" style="padding:10px!important">

| Component | Specification |
|-----------|--------------|
| **Board** | LilyGo T5 4.7" E-Paper Plus |
| **MCU** | ESP32-S3 |
| **Display** | EPD_7IN3E class color panel (800×480) |
| **Flash** | 16MB |
| **PSRAM** | 8MB |
| **RTC** | PCF8563 (I2C, battery backup) |
| **Storage** | SD Card (SPI) + LittleFS (internal) |
| **Battery** | Li-Ion 18650 (optional) |

</td>
<td align="center"> 
<img src="docs/LilyGoT54.7E-PaperPlus-Pins.png" width="370px" alt="PIN" />
</td>
</tr>
</table>

## 🔄 Operating Modes

| Mode | Description |
|------|-------------|
| **Photo Frame Mode** | JPEG slideshow from SD Card or LittleFS with color rendering, deep sleep between wake intervals (10sec to monthly) |
| **Maintenance Mode** | Button-triggered mode for configuration & remote management |
| **Low Battery Mode** | Auto shutdown with battery icon display |

## ✨ Features

- **Dual Storage Support** — SD Card (primary) + LittleFS (internal flash)
- **Smart Storage Fallback** — Auto-switch to secondary storage if images folder is empty
- **Cross-Storage File Operations** — Copy files between SD Card and LittleFS, delete with glob/batch support
- **RTC Backup** — PCF8563 maintains time during deep sleep
- **WiFi Connectivity** — AP mode for setup, STA mode for network access
- **NTP Time Sync** — Automatic time synchronization with RTC backup
- **Firmware OTA (Dual Slot)** — Update firmware via `/firmware/` (ota_0/ota_1) with boot-slot control
- **Battery Monitoring** — Auto low-power mode with voltage display
- **Color Rendering** — Palette mapping for the active e-paper color set with brightness/contrast/gamma control
- **Scheduled Wake-up** — Timer-based deep sleep with configurable wake-up hour (0–23)
- **Deep Sleep Wake-up** — Timer-based or button-triggered (EXT1)
- **mDNS Support** — Access device via hostname.local

## 📁 Project Structure

```
src/
├── Main.cpp                    # Application entry point
├── App/
│   ├── Button.cpp/h            # Debounced button handling
│   ├── Configuration.cpp/h     # INI config management
│   ├── Connection.cpp/h        # WiFi management (AP/STA)
│   ├── Display.cpp/h           # E-Paper driver wrapper
│   ├── Firmware.cpp/h          # Firmware manager
│   ├── Global.h                # Global definitions & macros
│   ├── LittleFS.cpp/h          # LittleFS operations
│   ├── NTP.cpp/h               # NTP time sync
│   ├── RTCTime.cpp/h           # PCF8563 RTC driver
│   ├── SDCard.cpp/h            # SD Card operations (SPI)
│   ├── Storage.cpp/h           # Storage manager with fallback
│   └── Utils.cpp/h             # System utilities, file ops, glob matching
├── Fonts/                      # OpenSans bitmap fonts (6-26pt)
│   └── opensans*.h             # 26 font variants
└── Images/
    └── DefaultImage.h          # Default fallback image

lib/
├── ArduinoHttpClient/          # HTTP client for image fetch
├── JPEGDEC/                    # JPEG decoder
├── LilyGoEPD47/                # E-Paper driver
└── Unity/                      # Unit testing framework

test/
├── mocks/                      # Mock classes for testing
│   ├── MockString.h
│   └── MockWiFiClient.h
├── test_Button/                # Button unit tests
├── test_ConfigCommand/         # Config command parsing tests
├── test_Configuration/         # Configuration parser tests
├── test_DateCommand/           # Date/RTC command parsing tests
├── test_ESP32/                 # Hardware-specific ESP32 tests
├── test_NTP/                   # NTP time utility tests
├── test_RTCTime/               # RTC time functions tests
├── test_SDCard/                # SD Card path/file utilities
├── test_Storage/               # Storage fallback logic tests
├── test_Utils/                 # Utility function tests
└── test_Wrappers/              # Type wrapper tests
```

## 🛠️ Build

### Requirements
- [PlatformIO](https://platformio.org/)
- ESP32-S3 toolchain (arduino-esp32 >= 2.0.3)

### Commands
```bash
# Build
pio run

# Upload firmware
pio run -t upload

# Upload filesystem (LittleFS)
pio run -t uploadfs

# Run tests (native)
pio test -e native

# Monitor serial
pio device monitor
```

## ⚙️ Configuration

Place `config.ini` in SD Card or LittleFS root (`/config.ini`):

```ini
[device]
appname = PHOTO FRAME CL01
version = v1.0

[display]
jpg_brightness = 25         ; 0-100%
jpg_contrast = 75           ; 0-100%
jpg_gamma = 125             ; gamma correction
image_file =                ; current image file

[ntp]
ntp_server = pool.ntp.org
ntp_port = 123
ntp_gmt_offset = 7200       ;GMT+2 in seconds
ntp_update = 60000          ; update interval ms

[ap mode]
ap_enable = true
ap_ssid = PhotoFrameCL01
ap_password = 123456789
ap_ip = 192.168.4.1
ap_gateway = 192.168.4.1
ap_subnet = 255.255.255.0

[sta mode]
sta_ssid = YourNetwork
sta_password = YourPassword

[static ip]
sta_enable = false
sta_ip = 192.168.0.83
sta_gateway = 192.168.0.1
sta_subnet = 255.255.255.0
sta_dns1 = 192.168.0.1
sta_dns2 = 8.8.8.8

[mdns]
mdns_enable = false
mdns_hostname = photoframecl01

[timer]
wake_up = 5                 ; 1=10sec, 2=1min, 3=1hour, 4=12hour, 5=Daily, 6=Weekly, 7=Monthly
wake_up_hour = 6            ; target hour (0-23) for Daily/Weekly/Monthly wake-up

[storage]
default_file_system = sdcard ; sdcard | littlefs
fallback_enabled = true      ; smart fallback if images empty
```

> `image_updated_at` is an internal metadata value stored in NVS (`dsp.file.upd`).
> It is intentionally not part of `config.ini` and cannot be queried or modified via `config`.

### Wake-up Schedule

The `wake_up_hour` setting (0–23) controls when the device wakes from deep sleep for the **Daily**, **Weekly** and **Monthly** timer modes. The device calculates the exact seconds remaining until the target hour using the RTC clock.

| Timer Mode | Behavior |
|------------|----------|
| 10sec / 1min / 1hour / 12hour | Fixed interval, `wake_up_hour` ignored |
| **Daily** | Wakes at the configured hour every day |
| **Weekly** | Wakes at the configured hour + 6 days |
| **Monthly** | Wakes at the configured hour + 29 days |

```
config wake_up_hour 8       # Set wake-up to 08:00
config wake_up 5            # Set timer to Daily mode
```

## 🧩 Firmware (OTA)

This project uses a dual-slot OTA layout (`ota_0` + `ota_1`) controlled by the `otadata` partition.

- **Applying update**: upload `firmware.bin` and `firmware.sha256` into `/firmware/` on the active storage, then run `fwupdate verify` and `fwupdate run`.
- **Which slot is running**: use `bootpart status` (shows Running/Boot partitions).
- **Force boot slot**: use `bootpart ota0` or `bootpart ota1`, then `reboot`.
- **USB upload note**: a plain USB upload typically writes the firmware at `0x10000` (often `ota_0`). If the device still boots the other slot, set it explicitly with `bootpart`.

## 🔌 Pin Configuration

| Pin | Function | Description |
|-----|----------|-------------|
| GPIO21 | Button 1 | Wake from deep sleep, Enter maintenance mode |
| GPIO48 | Button 2 | Factory reset (hold 30 sec) |
| GPIO14 | Battery ADC | Battery voltage measurement |
| GPIO18 | RTC SDA (I2C) | PCF8563 real-time clock data line |
| GPIO17 | RTC SCL (I2C) | PCF8563 real-time clock clock line |
| GPIO40 | SD MISO | SD Card data out (Master In Slave Out) |
| GPIO41 | SD MOSI | SD Card data in (Master Out Slave In) |
| GPIO39 | SD SCK | SD Card serial clock |
| GPIO38 | SD CS | SD Card chip select |

## 📦 Dependencies

- [LilyGoEPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) — E-Paper driver
- [JPEGDEC](https://github.com/bitbank2/JPEGDEC) — JPEG decoder
- [ArduinoHttpClient](https://github.com/arduino-libraries/ArduinoHttpClient) — HTTP client
- [Unity](https://github.com/ThrowTheSwitch/Unity) — Unit testing

## 🔋 Power Management

- **Photo Frame Mode**: Display image → deep sleep → wake by timer or button
- **Deep Sleep Current**: ~10µA (with RTC backup)
- **Wake-up Sources**: 
  - Timer (configurable: 10sec to monthly)
  - Scheduled hour (0–23) for Daily, Weekly and Monthly modes
  - Button press (GPIO21, EXT1 wakeup)
- **RTC Backup**: PCF8563 maintains accurate time during sleep
- **Low Battery**: Auto-shutdown at configurable voltage threshold

## 📄 License

MIT License

Copyright (c) 2025-2026 Szeklerman
