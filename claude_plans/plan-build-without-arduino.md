# Plan: Build and Compile ECHBT Without the Arduino IDE

## Prompt

> Using `research.md` as context, create a plan to be able to build and compile this project without installing and using the Arduino IDE. Write a detailed `plan-build-without-arduino.md` in a `claude_plans` directory in this project outlining how to implement this. Nothing inconsistent or that could cause bugs. Include code snippets and this exact prompt (with typos corrected and formatted for best readability).

---

## Todo List

### Phase 1 — One-Time Environment Setup
- [x] Confirm VS Code is installed (`code --version` in terminal, or check Applications)
- [x] Open VS Code Extensions panel (`Cmd+Shift+X`)
- [x] Search for **PlatformIO IDE** and confirm it is the extension published by PlatformIO
- [x] Install the PlatformIO IDE extension
- [x] Wait for PlatformIO Core to finish installing (progress shown in the VS Code notification area — can take 2–3 minutes on first install)
- [x] Restart VS Code when prompted
- [x] Confirm the PlatformIO toolbar (alien head icon) appears in the VS Code left sidebar

### Phase 2 — Project Configuration
- [x] Create `platformio.ini` in the project root (same directory as `echbt.ino`)
- [x] Set `platform = espressif32`
- [x] Set `board = heltec_wifi_kit_32` (not a generic `esp32dev` — required for `LED` and `KEY_BUILTIN` pin constants)
- [x] Set `framework = arduino`
- [x] Set `src_dir = .` in the `[platformio]` section (critical — must be global, not under `[env]`, or PlatformIO ignores it)
- [x] Set `lib_deps = https://github.com/HelTecAutomation/Heltec_ESP32.git#1.1.1` (pinned to v1.x via git tag — v2 breaks the `Heltec.display->init()` API call in `echbt.ino:204`)
- [x] Set `board_build.partitions = min_spiffs.csv` (required — default partition is too small for BLE)
- [x] Set `monitor_speed = 115200` (matches `Serial.begin(115200)` in `setup()`)
- [x] Verify no existing source files (`echbt.ino`, `device.h`, `power.h`, `icons.h`) were modified

### Phase 3 — First Build
- [x] Open the `echbt` project root folder in VS Code (**File → Open Folder**)
- [x] Confirm PlatformIO detects `platformio.ini` (the env `heltec_wifi_kit_32` should appear in the bottom status bar)
- [x] Trigger a build via the **✓ checkmark** in the PlatformIO toolbar (or `pio run` in the VS Code terminal)
- [x] Wait for PlatformIO to download the `espressif32` platform and Xtensa toolchain into `~/.platformio/` (first build only — can take several minutes)
- [x] Wait for PlatformIO to fetch `Heltec ESP32 Dev-Boards @ ~1.1.1` into `.pio/libdeps/heltec_wifi_kit_32/`
- [x] Confirm the build completes with `[SUCCESS]`
- [x] Confirm flash usage is under the `min_spiffs` app partition limit (~1.9 MB / 1,966,080 bytes)
- [x] Confirm RAM usage is reported without overflow

### Phase 4 — Flash and Hardware Verification
- [x] Connect the Heltec WiFi Kit 32 board to the computer via USB
- [x] Confirm the board's USB-UART port appears (PlatformIO **Devices** tab, or `pio device list` in terminal)
- [x] If port is not detected: install CP2102 or CH340 USB-UART driver for the OS (see Troubleshooting section)
- [x] Flash the firmware via the **→ upload arrow** in the PlatformIO toolbar (or `pio run --target upload`)
- [x] Confirm upload completes with no errors (watch for "Hard resetting via RTS pin..." as the final line — this means success)
- [x] Open the serial monitor via the **⚡ plug icon** (or `pio device monitor`)
- [x] Confirm `Start Scan!` appears in the serial output — this means the board booted and the BLE scan loop is running
- [x] Confirm the OLED display shows the mountain icon and "Starting Scan.." text
- [ ] (If bike is available) Confirm a device name appears on the OLED after the scan completes
- [ ] (If bike is available) Confirm `Found device.` and `Activated status callbacks.` appear in serial output after connecting
- [ ] (If bike is available) Confirm cadence, resistance, and power values update on the display while riding

### Phase 5 — Housekeeping
- [x] Confirm `.pio/` directory was created in the project root (this is the build cache — it is safe to delete and regenerate)
- [x] Add `.pio/` to `.gitignore` to avoid committing build artifacts and downloaded libraries
- [x] Confirm `platformio.ini` is the only new file added to the repository

---

## Overview

The approach is **PlatformIO via the VS Code extension**. Adding a single `platformio.ini` file to the project root is all that's needed. Once that file is present, opening the project folder in VS Code with the PlatformIO IDE extension installed gives you one-click build, flash, and serial monitor — no Arduino IDE, no system-wide CLI tools, no manual dependency management.

**What gets installed automatically (user-local, not system-wide):**
- The PlatformIO VS Code extension installs PlatformIO Core into `~/.platformio/` — entirely user-local
- PlatformIO Core downloads the Espressif ESP32 toolchain into `~/.platformio/packages/` on first build
- The Heltec library is fetched and cached in `.pio/libdeps/` inside the project on first build
- Nothing is installed into `/usr/local`, `/usr/bin`, or any system path

**The only manual step is installing the VS Code extension once.**

---

## Critical Facts from `research.md`

These constraints directly shape the `platformio.ini` configuration:

- `heltec.h` is **not in the repo**. It must be declared as a library dependency so PlatformIO fetches it automatically.
- The code uses the **older Heltec library API**: `Heltec.display->init()` directly. The v2+ library replaced this with `Heltec.begin(true, true, true)`. Using the wrong version will produce compile errors. The library must be **pinned to v1.x**.
- `LED`, `LED_BUILTIN`, and `KEY_BUILTIN` are pin constants defined by the Heltec board package — not standard Arduino. The correct board identifier must be used so these resolve to the right GPIO pins (LED → GPIO 25, KEY_BUILTIN → GPIO 0).
- BLE on ESP32 is large (~1.5 MB). The **default flash partition scheme is too small** and will cause a linker overflow. A larger app partition must be specified in the config.
- All `.h` files (`device.h`, `power.h`, `icons.h`) live in the project root alongside `echbt.ino`. PlatformIO must be told to treat the project root as the source directory, not the default `src/` subdirectory.
- The BLE libraries (`BLEDevice.h`, `BLEClient.h`, `BLEScan.h`, `BLERemoteCharacteristic.h`, `BLERemoteService.h`) ship with the ESP32 Arduino core and do **not** need to be listed in `lib_deps`.

---

## Step 1 — Install the PlatformIO IDE Extension for VS Code

1. Open VS Code
2. Go to the Extensions panel (`Cmd+Shift+X` on macOS, `Ctrl+Shift+X` on Windows/Linux)
3. Search for **PlatformIO IDE**
4. Click **Install** on the result published by PlatformIO

On first launch, the extension automatically downloads and installs PlatformIO Core into `~/.platformio/`. This takes a minute or two and requires an internet connection. No `pip install`, `brew install`, or any other system command is needed.

---

## Step 2 — Create `platformio.ini`

Create this file in the **project root** (alongside `echbt.ino`). Do not modify any existing source files.

**File: `platformio.ini`**

```ini
[platformio]
src_dir = .

[env:heltec_wifi_kit_32]
platform               = espressif32
board                  = heltec_wifi_kit_32
framework              = arduino
lib_deps               = https://github.com/HelTecAutomation/Heltec_ESP32.git#1.1.1
board_build.partitions = min_spiffs.csv
monitor_speed          = 115200
```

### Why each setting matters

| Setting | Value | Reason |
|---|---|---|
| `platform` | `espressif32` | PlatformIO's Espressif ESP32 platform; includes the toolchain, BLE libs, and Arduino core |
| `board` | `heltec_wifi_kit_32` | Defines `LED`, `KEY_BUILTIN`, `LED_BUILTIN` pin constants; selects correct bootloader |
| `framework` | `arduino` | Enables `setup()` / `loop()` Arduino entry points |
| `src_dir` | `.` | **Must be in `[platformio]` section.** Finds `echbt.ino` and all `.h` files in the project root |
| `lib_deps` | GitHub URL pinned to `#V1.1.1` | Provides `heltec.h`, `OLEDDisplayFonts.h`, and the `Heltec.display` object |
| `board_build.partitions` | `min_spiffs.csv` | Expands app flash region from ~1.2 MB to ~1.9 MB; required for BLE |
| `monitor_speed` | `115200` | Matches `Serial.begin(115200)` in `setup()` |

---

## Step 3 — Open the Project in VS Code

1. In VS Code: **File → Open Folder** → select the `echbt` directory
2. PlatformIO detects `platformio.ini` and activates the project automatically
3. The PlatformIO toolbar appears at the bottom of the VS Code window

**PlatformIO toolbar buttons (bottom status bar):**

| Icon | Action |
|---|---|
| ✓ (checkmark) | Build / Compile |
| → (right arrow) | Upload / Flash |
| ⚡ (plug) | Serial Monitor |
| 🗑 (trash) | Clean build artifacts |

On the **first build**, PlatformIO will:
1. Download the `espressif32` platform and toolchain into `~/.platformio/packages/`
2. Fetch the `Heltec ESP32 Dev-Boards` library into `.pio/libdeps/heltec_wifi_kit_32/`

Subsequent builds are fast because everything is cached.

---

## Step 4 — Build

Click the **✓ checkmark** in the PlatformIO toolbar, or use the command palette:

```
PlatformIO: Build
```

Or from a terminal within VS Code:
```bash
pio run
```

**Expected successful output:**
```
Linking .pio/build/heltec_wifi_kit_32/firmware.elf
RAM:   [=         ]  11.4% (used 37,432 bytes from 327,680 bytes)
Flash: [=========]  92.1% (used 1,207,552 bytes from 1,310,720 bytes)
========== [SUCCESS] Took 23.45 seconds ==========
```

---

## Step 5 — Flash to the Board

1. Connect the Heltec WiFi Kit 32 to your computer via USB
2. Click the **→ upload arrow** in the PlatformIO toolbar

PlatformIO auto-detects the USB serial port. If multiple ports are present, or if auto-detection fails, add this to `platformio.ini`:

```ini
upload_port = /dev/cu.usbserial-0001   ; macOS example
; upload_port = /dev/ttyUSB0           ; Linux example
; upload_port = COM3                   ; Windows example
```

> **If upload fails with "Failed to connect to ESP32":** Hold the `BOOT` (PRG) button on the board while the upload begins. Some boards require manual boot mode entry if the auto-reset circuit is not functioning.

---

## Step 6 — Serial Monitor

Click the **⚡ plug icon** in the PlatformIO toolbar to open the serial monitor at 115200 baud. You will see connection logs and (if `#define debug 1` is set in `echbt.ino`) raw BLE notification hex dumps.

---

## What PlatformIO Fetches Automatically

Nothing below requires a manual install command. PlatformIO manages all of this:

| Dependency | Source | Installed to |
|---|---|---|
| ESP32 Arduino core | PlatformIO package registry | `~/.platformio/packages/framework-arduinoespressif32/` |
| Xtensa GCC toolchain | PlatformIO package registry | `~/.platformio/packages/toolchain-xtensa-esp32/` |
| esptool (flash uploader) | PlatformIO package registry | `~/.platformio/packages/tool-esptoolpy/` |
| Heltec ESP32 Dev-Boards | GitHub (HelTecAutomation/Heltec_ESP32 tag V1.1.1) | `.pio/libdeps/heltec_wifi_kit_32/Heltec ESP32 Dev-Boards/` |

The `.pio/` directory is local to the project and can be deleted and regenerated freely. Add it to `.gitignore` if needed.

---

## Partition Scheme Reference

The `board_build.partitions = min_spiffs.csv` setting in `platformio.ini` is non-negotiable for this project. For reference:

| Scheme file | App partition | OTA | SPIFFS | Notes |
|---|---|---|---|---|
| *(default)* | ~1.2 MB | Yes | Yes | **Too small — BLE will overflow** |
| `min_spiffs.csv` | ~1.9 MB | Yes | Minimal | **Use this — recommended** |
| `no_ota_2MB.csv` | ~2.0 MB | No | Yes | Slightly more room, no OTA |
| `huge_app.csv` | ~3.0 MB | No | No | Maximum app space |

`min_spiffs.csv` is the right choice: enough space for BLE, OTA capability is preserved, and it ships with the standard ESP32 Arduino core so no custom partition file is needed.

---

## Troubleshooting

### Build error: `'Heltec' was not declared in this scope`
The library was not fetched. Delete `.pio/libdeps/` and rebuild to force a fresh download from GitHub.

### Build error: `no member named 'init' in 'SSD1306Wire'` or similar
The wrong git tag was checked out. Ensure the `#V1.1.1` tag is appended to the URL in `lib_deps`. Delete `.pio/libdeps/` and rebuild.

### Build error: `'LED' was not declared in this scope`
The board identifier is wrong. `LED` is defined in the Heltec board package for `heltec_wifi_kit_32` (GPIO 25). Do not use a generic `esp32dev` board — it does not define this constant.

### Build error: `region 'iram0_0_seg' overflowed by XXXXX bytes`
`board_build.partitions` is missing or wrong. Ensure `min_spiffs.csv` is set. If still overflowing, change to `huge_app.csv`.

### PlatformIO does not detect the project
Ensure the folder opened in VS Code is the `echbt` root (where `platformio.ini` lives), not a parent directory.

### Upload fails — port not found (macOS)
The USB-UART driver for the onboard CP2102 or CH340 chip may not be installed. Download the appropriate driver:
- **CP2102:** Silicon Labs CP210x driver
- **CH340:** CH34x driver from the chip vendor

After installing, replug the board.

### Upload fails — port not found (Linux)
Add your user to the `dialout` group:
```bash
sudo usermod -aG dialout $USER
```
Log out and back in for the change to take effect.

---

## What This Plan Does NOT Require

- No Arduino IDE installed at any point
- No modification to `echbt.ino`, `device.h`, `power.h`, or `icons.h`
- No renaming or moving of any existing files
- No `pip install`, `brew install`, or any other system-wide CLI installation
- No manually downloaded toolchains or SDK zips
