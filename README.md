# Photo Frame CL01 (Spectra6 Color E-Ink)

Photo Frame CL01 is an ESP32-S3 based, production-oriented color e-paper photo frame firmware for LilyGo T5 4.7" E-Paper Plus boards.

The project is designed around three goals:
1. Low-power autonomous image display with deep sleep.
2. Reliable maintenance workflows through a built-in web dashboard.
3. Robust field operation with NVS-backed configuration, dual OTA partitions, and storage fallback.

## 1. Scope and Hardware Target

- MCU: ESP32-S3
- Display: EPD_7IN3E class, 800x480 Spectra6 color e-paper
- Flash: 16 MB (dual OTA app slots + LittleFS)
- PSRAM: 8 MB OPI
- External RTC: PCF8563 (I2C)
- Storage: SD Card + LittleFS fallback
- Audio: I2S codec tone output

This firmware targets the LilyGo T5 4.7" E-Paper Plus hardware generation. For the older WROVER-E based variant, use PhotoFrameGS01.

## 2. System Architecture

The runtime is organized into focused modules under `src/App`:

- `Configuration_`: NVS persistence and defaults.
- `Storage_`: unified SD/LittleFS abstraction with fallback.
- `Display_`: e-paper rendering, JPEG tuning pipeline, text/graphics primitives.
- `Dashboard_`: async web server, authentication/session layer, API routing.
- `Connection_`: AP/STA networking, optional mDNS.
- `Firmware_`: OTA stream handling to inactive partition.
- `NTP_` + `RTC_`: time sync and clock persistence.
- `Battery_` + `Sound_` + `Led_` + `Button_`: device peripherals and UX signals.
- `Utils_`: sleep/wakeup logic, CPU frequency switching, diagnostics.

The application entrypoint in `src/Main.cpp` orchestrates initialization and mode routing.

## 3. Boot Sequence and Operating Modes

### 3.1 Boot Sequence

Startup flow (high level):

1. Init config (NVS), load defaults if first boot.
2. Init utility/peripheral stack (clock, RTC sync, battery, audio, LED).
3. Evaluate low battery condition.
4. Route into mode based on wake source and button state.

### 3.2 Photo Frame Mode

Purpose: autonomous slideshow operation with low power consumption.

Core behavior:

- Mount storage.
- Resolve current image from persisted config.
- Render JPEG using current display tuning.
- Persist next image pointer.
- Power down subsystems.
- Enter deep sleep with timer/button wake source.

If the current image is missing or unreadable, a built-in fallback image is shown.

### 3.3 Maintenance Mode

Purpose: online configuration and administration via local web dashboard.

Core behavior:

- Bring up WiFi (AP or STA according to config).
- Start async dashboard server.
- Show admin URL on display.
- Serve all configuration, media, and OTA endpoints.
- Track user activity for inactivity timeout handling.

Maintenance mode is intended for bounded interaction windows, then automatic return to photo-frame behavior.

### 3.4 Low Battery Mode

Purpose: protect battery and avoid unstable operation.

Core behavior:

- Play low-battery tone.
- Draw battery warning state to display.
- Enter low-power sleep path.

## 4. Configuration Model (NVS, No INI)

Configuration is fully NVS-backed via `Preferences`.

There is no runtime `config.ini` source of truth.

On first boot, defaults are created and persisted by `Configuration_::Init()`.

Configuration domains include:

- Device identity and pin assignment.
- Display rendering parameters (brightness/contrast/gamma/saturation/RGB gains/rotation).
- WiFi AP/STA + fallback + mDNS.
- NTP/RTC and wake scheduling.
- Storage defaults and fallback policy.
- Dashboard settings (language/theme/session-related options, dynamic CPU scaling).

Factory reset clears NVS config and reboots.

## 5. Power, Sleep, and Wake Mechanisms

### 5.1 Wake Scheduling

Timer modes are enum-based and support:

- Seconds
- Minutes
- Hourly
- Half-day
- Daily
- Weekly
- Monthly

For Daily/Weekly/Monthly, `wake_up_hour` is respected; shorter interval modes ignore hour targeting.

### 5.2 Deep Sleep Strategy

Photo Frame mode calculates sleep target and enters deep sleep after render completion.

Wake sources include timer and button-triggered wake logic.

### 5.3 CPU Frequency Control

Baseline in maintenance is 160 MHz.

Dynamic high-performance windows are activated on dashboard workloads:

- Page navigation: 6 s hold
- Media/image operations: 10 s hold
- OTA operations: 45 s hold

If no high-demand workload remains active, frequency returns to 160 MHz.

This mechanism can be toggled in dashboard settings (`dsh.cpu.dyn`).

## 6. Session and Security Model

Dashboard authentication is cookie-token based.

- Login endpoint validates user credentials.
- Session token is issued and stored in bounded session table.
- Protected endpoints require valid token.
- Expired/invalid sessions are rejected and redirected to login.
- Activity timestamps are updated on authenticated calls.

The activity timestamp is reused by maintenance inactivity logic to decide automatic restart.

Security notes:

- HTTP only (no TLS termination on device).
- Single-admin credential model.
- Password stored as SHA-256 hash in NVS.

## 7. Maintenance Inactivity Mechanism

Maintenance loop checks last authenticated dashboard activity once per second.

When inactivity reaches `MAINTENANCE_INACTIVITY_TIMEOUT_MS` (currently 5 minutes), the firmware:

1. Stops dashboard services.
2. Powers down display/storage/network cleanly.
3. Calls `esp_restart()`.
4. Returns to default boot path (Photo Frame mode).

This avoids leaving the device in permanent maintenance state.

## 8. Storage and Media Pipeline

`Storage_` unifies SD Card and LittleFS behind one interface.

Behavior:

- Select configured primary storage when available.
- If unavailable/empty and fallback enabled, switch to secondary storage.
- Ensure image directory availability.
- Provide list/read/write/delete/copy/move flows used by dashboard APIs.

Media operations exposed in dashboard include:

- Upload
- Import from URL
- Thumbnail serving
- Delete (including pattern-based operations)
- Cross-storage copy/swap
- Set current/default image

## 9. Display Rendering Pipeline

`Display_` wraps low-level e-paper driver operations and rendering policies.

Main responsibilities:

- JPEG decode and render to Spectra6-compatible output.
- Apply user-configurable tuning (brightness/contrast/gamma/saturation/channel gains).
- Text and shape primitives for status screens.
- Rotation support.
- Controlled update and power-off behavior for e-paper lifecycle.

## 10. Networking and Time Services

### 10.1 Connectivity

`Connection_` supports AP and STA operation with fallback behavior.

Capabilities:

- AP mode for local maintenance access.
- STA mode for network-integrated deployments.
- Optional mDNS hostname publishing.
- Client presence checks used by maintenance UX.

### 10.2 Time Management

`NTP_` and `RTC_` cooperate to keep time stable across deep sleep cycles.

- NTP sync updates system clock.
- RTC stores time persistently with backup source.
- Wake-hour scheduling uses RTC/system time computations.

## 11. Firmware Update (OTA) and Partitioning

Partition design (`partitions.csv`):

- `ota_0` and `ota_1` (equal app slots)
- `otadata` for active boot slot metadata
- `littlefs` data partition
- `nvs` config partition

OTA flow:

1. Upload new firmware through dashboard OTA endpoint.
2. Stream into inactive slot.
3. Validate and finalize image.
4. Flip boot target.
5. Reboot into new firmware.

This provides rollback-friendly dual-slot behavior for remote updates.

## 12. Dashboard (Detailed)

The dashboard is a first-class subsystem with embedded assets and API-driven interactions.

### 12.1 Pages

Implemented pages include:

- Login
- Gallery/Index
- Display
- Firmware
- Network
- NTP
- Date & Time
- Wake-up
- mDNS
- Language
- Settings
- User
- Stats
- Registry

These are served from compiled assets (`Dashboard/Pages`, `Dashboard/Assets`, `Dashboard/Languages`).

### 12.2 API Groups

API surface is organized around:

- Authentication (`/api/login`, `/api/logout`)
- Page and status (`/api/page`, `/api/status`, `/api/stats`)
- Media (`/api/images*`)
- Configuration saves (`/api/*/save` endpoints)
- Time sync (`/api/ntp/sync`, `/api/rtc/*`)
- OTA (`/api/ota/*`)
- System actions (`/api/reboot`, `/api/restart`, `/api/factory/reset`)

### 12.3 Runtime Behaviors

Dashboard runtime includes:

- Session purge and validation.
- Cached status/stats refresh intervals.
- Optional WebSocket updates.
- CPU demand marking for performance windows.
- Activity timestamp updates for inactivity timeout.

### 12.4 Full Endpoint Inventory

Authentication and navigation:

- `GET /`
- `GET /login`
- `GET /login.html`
- `POST /login`
- `POST /login.html`
- `GET /error`
- `GET /error.html`
- `GET /api/page`
- `POST /api/login`
- `POST /api/logout`

Images and media:

- `GET /api/images`
- `GET /api/images/<name>`
- `GET /api/images/thumb/<name>`
- `GET /api/images/thumbs/<name or pattern>`
- `POST /api/images/upload`
- `POST /api/images/copy`
- `POST /api/images/import-url`
- `POST /api/images/delete`
- `POST /api/images/swap`
- `POST /api/images/default`

Status and configuration:

- `GET /api/status`
- `GET /api/config`
- `POST /api/config/save`
- `POST /api/display/save`
- `POST /api/network/save`
- `POST /api/mdns/save`
- `POST /api/ntp/save`
- `POST /api/ntp/sync`
- `POST /api/datetime/save`
- `POST /api/language/save`
- `POST /api/user/save`
- `POST /api/user/restore`
- `POST /api/wakeup/save`
- `GET /api/stats`

OTA and system actions:

- `GET /api/ota/status`
- `POST /api/ota/upload`
- `POST /api/reboot`
- `POST /api/restart`
- `POST /api/factory/reset`
- `POST /api/rtc/sync`
- `GET /api/rtc/now`

WebSocket and static assets:

- `GET /ws` (WebSocket upgrade)
- Static pages, scripts, styles, SVG/images, and language assets served from embedded PROGMEM resources

## 13. Build and Deployment

Project uses PlatformIO (`platformio.ini`) with `photo_frame_cl_01` as default env.

Key build characteristics:

- C++17
- ESP32-S3 board config
- 16 MB flash partition layout
- 8 MB PSRAM OPI
- Post-build firmware export script: `scripts/firmware.py`

Typical commands:

```bash
pio run -e photo_frame_cl_01
pio run -e photo_frame_cl_01 -t upload
pio run -e photo_frame_cl_01 -t uploadfs
pio device monitor
```

## 14. Tooling and Utility Scripts

`scripts/` contains project tooling for asset and firmware workflows:

- `firmware.py`: post-build firmware artifact handling.
- `bmp6_to_hex.py`: bitmap conversion to project-friendly C representation.
- `fontconvert.py`: font asset conversion pipeline.
- `ConvertTo6C/`: palette conversion utilities for Spectra6 preparation.

These tools support repeatable asset preparation and deployment packaging.

## 15. Operational Constraints and Recommended Practices

Constraints to account for in production:

- HTTP-only dashboard (LAN-trust boundary required).
- Limited concurrent sessions.
- Embedded resource limits (RAM/flash/partition sizes).
- E-paper refresh characteristics (not suited for high-frame-rate interaction).
- Storage/media quality impacts on render time.

Recommended practices:

1. Change default admin credentials immediately.
2. Validate NTP/RTC and wake-hour settings after first boot.
3. Keep firmware artifact/version workflow explicit for OTA traceability.
4. Use stable power source to avoid brownout-adjacent issues.
5. Keep media assets resolution/format aligned with display target.

## License

MIT License

Copyright (c) 2025-2026 Szeklerman
