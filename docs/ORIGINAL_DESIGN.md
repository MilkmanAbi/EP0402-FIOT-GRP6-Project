# ParcelGuardian — EP0402 FIOT Group 6

Firmware for a **wireless mishandling-prevention and loss-detection tracker** that rides inside a parcel.

EP0402 Fundamentals of IoT — project idea **#8, Wireless Tracker (People / Asset) shield**.
Arduino Uno R3 + the supplied Singapore Poly EEE tracker shield.

---

## What it does

| | | |
|---|---|---|
| **DETECT** | ADXL345 | Orientation-independent impact detection — drops, throws, hard jolts, the moment they happen |
| **DETER** | Buzzer + LED | Fire locally at the point of mishandling. A box that complains is a box that gets handled more carefully |
| **RECORD** | microSD | Every event and heartbeat appended to the black box, online or not |
| **REPORT** | ESP-01 → ThingSpeak | Whatever reaches the cloud drives the phone dashboard; the full log is read out at handover |

### The one idea that shapes everything

**The SD card is the system of record. The cloud is a cache of whatever got through.**

A parcel spends almost its entire journey out of WiFi range. A cloud-first design would fail to observe the exact window it exists to observe. So every event is written locally first, and the uplink is strictly best-effort.

### What the device deliberately does *not* do

It never decides it is **lost**. Silence is only observable from the other end, so that judgement belongs to the phone app, which compares time-since-last-check-in against a threshold. The device just reports; the app infers.

---

## Build status — measured, not estimated

Compiled with `arduino-cli 1.5.2` for `arduino:avr:uno`, `--warnings all`, **zero warnings**:

```
Sketch uses 30800 bytes (95%) of program storage space. Maximum is 32256 bytes.
Global variables use 1396 bytes (68%) of dynamic memory,
                     leaving 652 bytes for local variables. Maximum is 2048 bytes.
```

Getting there was not free. The first complete build came to **111% of flash** and did not fit. Three changes brought it down:

| Change | Flash saved | Why |
|---|---:|---|
| Replaced TinyGPSPlus with a purpose-built NMEA parser | ~2.3 KB | The library carries course/distance/speed maths this project never uses |
| Removed `snprintf_P` and `dtostrf`, unified on one integer-maths formatter (`Fmt.cpp`) | ~0.9 KB | Two separate float-to-text engines were being linked in at once |
| Replaced the Wire library with a direct TWI driver (`Adxl345.cpp`) | ~1.9 KB | We need three I2C transaction shapes; Wire brings ring buffers we never use |

That last one also returned **216 bytes of SRAM**.

The four bench-test tools build at 11–38% of flash.

---

## Pin map

Straight from the shield documentation. Do not reassign without checking the silkscreen.

| Function | Pin | Notes |
|---|---|---|
| ADXL345 SDA / SCL | A4 / A5 | I2C, driven by our own TWI code |
| microSD MISO / MOSI / SCK / CS | D12 / D11 / D13 / D10 | **D13 is SPI clock *and* the on-board LED** — you cannot use the built-in LED |
| Buzzer | D9 | |
| LED | D8 | The shield LED, not D13 |
| DIP switches | D7, D6 | `INPUT_PULLUP`; polarity undocumented, see below |
| NEO-6M GPS | D5 (Uno TX) / D4 (Uno RX) | `SoftwareSerial gps(4, 5)` |
| ESP-01 WiFi | D3 (Uno TX) / D2 (Uno RX) | `SoftwareSerial esp(2, 3)` |

`SoftwareSerial` is constructed `(rxPin, txPin)` **from the Uno's point of view**. Getting this backwards is the most common wiring mistake on this shield.

---

## Getting started

### 1. Run the bench tests first

Do not skip this. Each tool isolates one subsystem, and debugging four at once is how afternoons disappear.

```
firmware/tools/EspAtTest      ESP-01 responds to AT? (and how to fix the baud rate)
firmware/tools/GpsRawTest     NEO-6M emitting NMEA?
firmware/tools/SdCardTest     Card initialises, writes and reads back?
firmware/tools/AdxlDropTest   Accelerometer alive — and your drop-test calibration rig
```

### 2. Create your secrets file

```bash
cd firmware/ParcelGuardian
cp Secrets.example.h Secrets.h      # or duplicate it in the IDE
```

Fill in your WiFi SSID, password and ThingSpeak **write** key. `Secrets.h` is gitignored — **this repo is public, so a key committed here is a key you have to rotate.**

### 3. Install the libraries

Only one external library is needed now: **SD** (by Arduino, Library Manager). Everything else is in the AVR core.

### 4. Upload

Board **Arduino Uno**, Serial Monitor at **9600**. Send `?` for the command list.

---

## Modules

```
ParcelGuardian.ino   state machine, event handling, serial console
Config.h             every pin, threshold, timing and feature flag
Secrets.h            your credentials (gitignored)
Adxl345.*            accelerometer + bare-metal TWI driver
ImpactDetector.*     magnitude, baseline, severity tiers, debounce
Indicators.*         non-blocking buzzer and LED pattern engines
GpsModule.*          NMEA parser with checksum validation
EspClient.*          AT-command driver and ThingSpeak uplink
SdLogger.*           the black box
Modes.*              DIP switch mode selection
Persist.*            EEPROM counters that survive a power cut
Fmt.*                the one float formatter
Diagnostics.*        free-SRAM reporting
```

Every feature can be compiled out with a flag in `Config.h` (`ENABLE_SD`, `ENABLE_ESP`, `ENABLE_GPS`, `ENABLE_WATCHDOG`, `ENABLE_CONSOLE`) if you need the space back.

---

## How impact detection works

```
1. Sample x, y, z from the ADXL345 at 20 Hz
2. magnitude = sqrt(x² + y² + z²)        <- orientation independent
3. delta     = |magnitude - baseline|    <- baseline captured at power-on
4. Classify delta against the severity tiers
5. Hold a 2 s debounce before re-arming
```

| Tier | Delta | Response |
|---|---|---|
| Normal | < 0.40 g | Ignored — no log, no alert |
| Rough | 0.40 – 1.20 g | Logged with time + GPS, two chirps, LED pulse |
| Severe | ≥ 1.20 g | Logged, warbling alarm, fast LED, flagged for priority upload |

**Why magnitude and not per-axis:** a parcel's orientation in a truck is unknown and changes constantly. A per-axis threshold would miss a drop landing on the "wrong" face.

**These thresholds are starting points.** Calibrate them with `AdxlDropTest` and record your numbers in [docs/CALIBRATION.md](docs/CALIBRATION.md). We claim *validated impact detection* — **not** fall detection.

---

## DIP switch modes

Two switches, four modes, all of them earning their place:

| D7 | D6 | Mode | Behaviour |
|---|---|---|---|
| 0 | 0 | **NORMAL** | Real-world timings: 30 s heartbeat, 20 s upload |
| 0 | 1 | **DEMO** | Accelerated: 5 s heartbeat, 10 s upload — the whole lifecycle inside a minute |
| 1 | 0 | **SILENT** | Buzzer muted for bench work. LED still active |
| 1 | 1 | **READOUT** | Handover mode: stop tracking, dump the SD black box as the chain-of-custody record |

The shield's switch polarity is undocumented. Send `m` on the console; if the mode is inverted, flip `DIP_ACTIVE_LOW` in `Config.h`.

---

## Serial console

9600 baud. Send `?` for help.

| Key | Action | | Key | Action |
|---|---|---|---|---|
| `?` | help | | `u` | force an upload now |
| `s` | full status dump | | `g` | GPS diagnostics |
| `r` | free SRAM | | `a` | mark ARRIVED |
| `m` | DIP / mode | | `d` | dump the SD log |
| `c` | recalibrate baseline | | `w` | wipe the SD log |
| `b` | buzzer test | | `z` | reset EEPROM counters |
| `l` | LED test | | | |

`g` is deliberately diagnostic: *no chars* means wiring or power, *chars but no valid sentences* means baud rate, *valid sentences but no fix* means no sky view.

---

## State machine

```
CALIBRATE ──> PREP ──picked up / timeout──> TRANSIT <──> SYNC
                                               │
                                               └──console 'a'──> ARRIVED

  DIP 11 ──> READOUT  (from any state; returns to TRANSIT when cleared)
  Impact events overlay any state and return after the 2 s debounce.
```

`QUIET` and `LOST` are **not** device states — the phone app derives them from the time since the last successful check-in.

---

## ThingSpeak field schema

| Field | Contents | Notes |
|---|---|---|
| 1 | Latitude | Blank when there is no fix — never back-filled with a stale value |
| 2 | Longitude | Same rule |
| 3 | Peak Δg | Highest impact magnitude since the previous upload |
| 4 | Status code | `0` normal · `1` rough · `2` severe · `3` no GPS fix |
| 5 | Cumulative event count | Survives power cuts via EEPROM |

Upload cadence is 20 s (the free tier documents a 15 s minimum).

An upload counts as successful **only when ThingSpeak returns a non-zero entry id**. `SEND OK` just means the ESP-01 pushed bytes out of the door — trusting it alone is the classic "why is my channel empty" bug.

---

## Known limits — designed around, not hidden

| Limit | How the firmware copes |
|---|---|
| **Only one SoftwareSerial can receive at a time** | GPS holds the listener; the ESP-01 borrows it per upload. NMEA characters are lost in that window, so every sentence is checksum-verified and fix *age* is surfaced rather than hidden |
| **2 KB SRAM** | No `String` anywhere, every constant in `PROGMEM`, one shared 128-byte path buffer, free-RAM reporting at boot and on demand |
| **SD writes block for milliseconds** | Rows are short, infrequent, and opened/closed per write so a power cut loses at most one row |
| **No GPS fix indoors or in a cargo hold** | Logged honestly as no-fix; the LED shows fix state; stale coordinates are never uploaded as live |
| **ESP-01 is 3.3 V and draws burst current** | Verify the shield's regulation before powering. Never wire a bare ESP-01 to Uno 5 V |
| **A wedged AT transaction** | 8-second watchdog, armed after the slow boot-time join. Long operations (log dumps, AT waits) kick it explicitly |
| **AVR `double` is 32-bit** | ~7 significant digits, so the 6th decimal of a longitude is already below the noise floor — about a metre. We print it because it is the conventional format, not because it is meaningful |

---

## Security

Plain **HTTP on port 80**. The write key and coordinates travel in clear text, because validated TLS is not practical on ESP-01 AT firmware.

**Demo data only. Do not track a real person or a real shipment with this build.** See [docs/SECURITY.md](docs/SECURITY.md).

---

## Next

The Android **Control Centre** app — reads the channel, shows status and last position, hands off to Google Maps, and derives `QUIET` / `LOST` from the check-in gap. Not in this repo yet.
