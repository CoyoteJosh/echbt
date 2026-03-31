# ECHBT — Deep Research Report

## Prompt

> Read this folder in depth. Understand how it works deeply — what it does and all the intricate details of how it works. When that is done, write a detailed report including this exact prompt (with any typos corrected and formatted so it's easy to read) and your learning and findings into a file named `research.md`. Be sure to include:
>
> - Stack and key dependencies
> - Architecture and data flow
> - Existing patterns to follow
> - Entry points
> - Gotchas and known issues
> - Glossary of domain terms

---

## Project Overview

**ECHBT** is an Arduino-based Bluetooth Low Energy (BLE) client for the Echelon Sport stationary bike. It runs on an ESP32 microcontroller with an integrated OLED display and shows real-time cycling metrics — cadence, resistance, power, and elapsed runtime — during a workout. A companion Android app called "EchBT Stats" can overlay the stats on screen.

**License:** Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International

---

## Stack and Key Dependencies

| Layer | Technology |
|---|---|
| Language | C++ (Arduino dialect) |
| Platform | ESP32 (MakerFocus board — Amazon ASIN B076KJZ5QM) |
| Display | OLED (via Heltec ESP32 board library) |
| Wireless | Bluetooth Low Energy (BLE) |
| Build system | Arduino IDE |
| Core libraries | `Arduino.h`, `heltec.h`, `BLEDevice.h`, `BLEClient.h`, `BLEScan.h`, `BLEAdvertisedDevice.h`, `BLERemoteCharacteristic.h`, `BLERemoteService.h` |
| Font rendering | `OLEDDisplayFonts.h` — `ArialMT_Plain_10`, `ArialMT_Plain_24` |
| Icon format | XBM (X11 monochrome bitmaps) stored in `PROGMEM` |

The `heltec.h` library is not included in the repo — it is provided by the Heltec ESP32 board support package in Arduino IDE. It wraps the OLED display and exposes `Heltec.display`.

---

## Repository Structure

```
echbt/
├── echbt.ino        # Main firmware — entry point, BLE client, display rendering
├── device.h         # BLE device discovery, deduplication, user selection UI
├── power.h          # Power output and resistance conversion algorithms
├── icons.h          # XBM icon assets (encoded as byte arrays in PROGMEM)
├── README.md        # Project documentation
└── icons/
    ├── README.md        # Icon conversion instructions
    ├── screenshot.png   # Photo of the running display
    ├── clock.png        # Source for clock_icon
    ├── cadence.png      # Source for cadence_icon
    ├── power.png        # Source for power_icon
    ├── resistance.png   # Source for resistance_icon
    ├── echelon.png      # Source for echelon_icon
    └── mountain.png     # Source for mountain_icon (splash/scanning screen)
```

---

## Architecture and Data Flow

### High-Level Architecture

```
┌──────────────────────────────────────────┐
│  ESP32 + OLED Display                    │
│  ┌────────────┐  ┌────────────────────┐  │
│  │  echbt.ino │  │     icons.h        │  │
│  │  (main)    │  │  (XBM bitmaps)     │  │
│  └─────┬──────┘  └────────────────────┘  │
│        │ uses                            │
│  ┌─────▼──────┐  ┌────────────────────┐  │
│  │  device.h  │  │     power.h        │  │
│  │ (discovery │  │  (calculations)    │  │
│  │ & select)  │  │                    │  │
│  └────────────┘  └────────────────────┘  │
└──────────────────────────────────────────┘
              BLE
┌──────────────────────────────────────────┐
│  Echelon Sport Bike                      │
│  ├─ Cadence sensor → 0xD1 notifications  │
│  └─ Resistance sensor → 0xD2 notifications│
└──────────────────────────────────────────┘
```

### Data Flow — Step by Step

**1. Discovery Phase** (runs in `loop()` while disconnected)

- ESP32 starts a 6-second BLE scan with `BLEScan`
- Scan parameters: interval 1349ms, window 449ms (~33% duty cycle), active scan enabled
- `AdvertisedDeviceCallbacks::onResult()` fires for each nearby device
- Devices that advertise the Echelon service UUID and have a name are added to a dedup list (max 20 devices) via `addDevice()`

**2. Device Selection** (`selectDevice()` in `device.h`)

- If exactly 1 device found: returns it immediately
- If multiple: shows a scrollable on-screen menu
  - Short button press (< 400ms): advance to next device (cycles)
  - Long button press (≥ 400ms): confirm selection

**3. Connection Phase** (`connectToServer()` in `echbt.ino`)

- Creates a `BLEClient` with `ClientCallback` installed
- Connects to the selected device
- Waits 200ms, verifies connection
- Retrieves the main service (`connectUUID`)
- Subscribes to the sensor characteristic (`sensorUUID`) with `notifyCallback` as the notification handler
- Sends a 5-byte activation command to `writeUUID`: `{0xF0, 0xB0, 0x01, 0x01, 0xA2}`

**4. Data Reception** (async, via `notifyCallback()`)

- Bike sends BLE notifications on the sensor characteristic
- Notification type is identified by `data[1]`:
  - `0xD1` — Cadence: extracted from `data[9:10]` as a 16-bit big-endian int
  - `0xD2` — Resistance: extracted from `data[3]` as a single byte (0–32)
- After each update, power is recalculated: `power = getPower(cadence, resistance)`
- `last_cadence` timestamp is updated on every cadence notification

**5. Display Update** (`updateDisplay()`, called every 200ms in `loop()`)

- Screen layout on 128×64 OLED:

```
┌─────────────────────────────────────┐
│ [clock]  MM:SS           [echelon]  │
│ [cadence] RPM   [power] WWW W       │
│ [resist] ██████████████░░░░ XX      │
└─────────────────────────────────────┘
```

- Runtime (top-left): increments only while `cadence > 0` (pauses at rest)
- Resistance progress bar: 78px wide, fills proportionally to `resistance / maxResistance`
- Screen blanks after 120 seconds of no cadence activity

**6. Disconnection**

- `ClientCallback::onDisconnect()` fires
- `connected = false`, LED turns off, client object is `delete`d and set to `nullptr`
- `loop()` re-enters the discovery phase on next iteration

---

## Entry Points

| Entry Point | File | Description |
|---|---|---|
| `setup()` | `echbt.ino:199` | Arduino setup — initializes serial, display, GPIO, BLE |
| `loop()` | `echbt.ino:221` | Arduino main loop — scan/connect/display state machine (200ms tick) |
| `notifyCallback()` | `echbt.ino:29` | BLE notification handler — parses cadence and resistance data |
| `ClientCallback` | `echbt.ino:61` | BLE connect/disconnect event handlers |
| `AdvertisedDeviceCallbacks` | `echbt.ino:75` | BLE scan result handler |
| `connectToServer()` | `echbt.ino:138` | Establishes BLE connection and subscribes to notifications |
| `updateDisplay()` | `echbt.ino:87` | Renders current metrics to OLED |
| `selectDevice()` | `device.h:29` | Button-driven device selection UI |
| `getPower()` | `power.h:67` | Calculates watts from cadence and resistance |
| `getPeletonResistance()` | `power.h:72` | Converts Echelon resistance to Peloton-equivalent scale |

---

## BLE UUIDs (Echelon Proprietary Protocol)

| Constant | UUID | Purpose |
|---|---|---|
| `deviceUUID` | `0bf669f0-45f2-11e7-9598-0800200c9a66` | Service UUID advertised by bike — used for discovery |
| `connectUUID` | `0bf669f1-45f2-11e7-9598-0800200c9a66` | Main service UUID — retrieved after connecting |
| `writeUUID` | `0bf669f2-45f2-11e7-9598-0800200c9a66` | Write characteristic — used to send activation command |
| `sensorUUID` | `0bf669f4-45f2-11e7-9598-0800200c9a66` | Notify characteristic — source of cadence/resistance data |

---

## Existing Patterns to Follow

**1. BLE Callback Pattern**
New BLE event handling should follow the existing pattern: subclass `BLEClientCallbacks` or `BLEAdvertisedDeviceCallbacks`, override the relevant virtual methods, and register via the appropriate setter. Keep callbacks short — set flags/values and return; do not do display work or complex logic inside callbacks.

**2. Header File Modularity**
Each concern lives in its own `.h` file. New features (e.g., heart rate sensor support) should get their own header. The `.ino` file orchestrates but does not contain algorithms.

**3. PROGMEM for Constants**
All read-only data (icons, lookup tables) belongs in PROGMEM. The existing icons use:
```c
const unsigned char icon_name[] PROGMEM = { ... };
```

**4. XBM Icons**
To add a new icon: create a PNG, convert to XBM using the `convery` tool referenced in `icons/README.md`, then format with `xxd -i`. The output byte array goes in `icons.h`.

**5. Empirical Model Coefficients**
The power and resistance calculations are derived from real-world data fitted with R. If new data is collected, the same nonlinear model structure (`a * b1^resistance * b2^cadence`) should be refit to get updated coefficients.

**6. Debug Mode via Compile Flag**
Diagnostic output uses a compile-time `#define debug 0` flag pattern. New debug output should follow this pattern rather than using always-on `Serial.print`.

**7. Runtime Tracking — Local, Not From Bike**
Always use local `millis()` for elapsed time. The bike's reported runtime drifts significantly (see Gotchas).

---

## Gotchas and Known Issues

**1. Bike Runtime Drifts (Hard Coded Workaround)**
The bike sends its own runtime value in the cadence notification at `data[7:8]`, but this value has "massive drift" per the code comment. The code intentionally ignores it and tracks elapsed time locally using `millis()`. The original code is left commented out at `echbt.ino:33`:
```c
//runtime = int((data[7] << 8) + data[8]); // This runtime has massive drift
```

**2. Magic Activation Bytes**
The activation command `{0xF0, 0xB0, 0x01, 0x01, 0xA2}` is a proprietary Echelon protocol sequence. Without sending it, the bike does not emit notifications. The meaning of individual bytes is undocumented in the repo.

**3. `heltec.h` Not in Repo**
The display library is an external Arduino board package, not a vendored dependency. Building requires installing the Heltec ESP32 board support in Arduino IDE separately.

**4. Fixed Device Array Size**
`device.h` uses a static array of 20 device pointers. If more than 20 Echelon bikes are nearby (unusual, but possible in a gym), additional devices are silently dropped.

**5. No Reconnection Logic**
On disconnect, the firmware falls back to the scan loop from scratch. There is no auto-reconnect to the previously paired device — the user must re-run the scan and selection flow after each disconnection.

**6. `BLEAdvertisedDevice*` Pointer Lifetime**
The `devices[]` array in `device.h` stores raw `BLEAdvertisedDevice*` pointers from the scan. These are owned by the BLE library and may be invalidated between scans. The current code starts a fresh scan each connection attempt, which avoids the issue, but any caching across scans would be unsafe.

**7. Screen Timeout Is Cadence-Gated**
The 120-second timeout is based on `last_cadence` (last time a `0xD1` notification arrived), not the last display update. This means even high resistance with zero pedaling will blank the screen — it is not truly an idle timeout.

**8. Power Formula is Echelon-Specific**
The `getPower()` coefficients were fitted specifically to Echelon Sport data. They will not produce accurate results for other bike models.

**9. Resistance Display Uses Native Scale**
The progress bar and numeric resistance shown on the OLED are Echelon native (0–32), not the Peloton-equivalent percentage. `getPeletonResistance()` is calculated and available but is not currently displayed. It would need to be added to `updateDisplay()` if desired.

---

## Glossary of Domain Terms

| Term | Definition |
|---|---|
| **Cadence** | Pedaling speed measured in revolutions per minute (RPM). Updates via `0xD1` BLE notifications. |
| **Resistance** | The mechanical difficulty/load level on the bike, 0–32 on Echelon Sport. Controlled by a physical knob on the bike. Updates via `0xD2` BLE notifications. |
| **Power** | Estimated rider output in watts. Calculated from cadence and resistance using an empirically fitted nonlinear model. |
| **Runtime** | Elapsed active workout time in milliseconds, tracked only while cadence > 0 (pauses at rest). |
| **Peloton Resistance** | A scaled equivalent of Echelon resistance expressed as a percentage (0–100+) compatible with Peloton ecosystem metrics. Derived via cubic polynomial. |
| **BLE Notification** | An asynchronous push of data from a BLE peripheral (the bike) to a central (the ESP32) when sensor values change. No polling required. |
| **BLE Central** | The device that initiates connections and consumes data — in this project, the ESP32. |
| **BLE Peripheral** | The device that advertises and provides data — in this project, the Echelon bike. |
| **Service UUID** | A BLE identifier for a logical grouping of related characteristics. The Echelon bike advertises a proprietary service UUID. |
| **Characteristic** | A BLE data endpoint within a service. The bike exposes a write characteristic (for commands) and a sensor characteristic (for notifications). |
| **Activation Command** | The 5-byte sequence `{0xF0, 0xB0, 0x01, 0x01, 0xA2}` that must be written to the bike after connecting to enable sensor notifications. |
| **XBM** | X BitMap format — a monochrome 1-bit image format stored as a C byte array. Used for OLED icons. |
| **PROGMEM** | Arduino/AVR attribute that places data in flash (program memory) instead of RAM. Critical on memory-constrained microcontrollers. |
| **ESP32** | Espressif's dual-core microcontroller with integrated Wi-Fi and Bluetooth. Used here primarily for its BLE hardware. |
| **Heltec** | Board manufacturer that makes ESP32 modules with integrated OLED displays. The `heltec.h` library wraps display and board initialization. |
| **Active Scan** | A BLE scan mode that requests additional advertising data (scan response) from devices beyond the initial advertisement packet. |
| **0xD1 / 0xD2** | Proprietary notification type identifiers in the Echelon BLE protocol. `0xD1` = cadence data, `0xD2` = resistance data. |
| **maxResistance** | Compile-time constant set to `32`, the maximum resistance level on the Echelon Sport bike. Used to calculate progress bar fill percentage. |
| **Screen Timeout** | Feature added in commit `892d981` — blanks the OLED display after 120 seconds without a cadence notification to conserve power. |
| **My Cadence for Arduino** | A fork of this project adapted for generic Bluetooth cadence sensors (not Echelon-specific). |
| **EchBT Stats** | A companion Android app that displays the bike metrics as an overlay on the phone screen. |
