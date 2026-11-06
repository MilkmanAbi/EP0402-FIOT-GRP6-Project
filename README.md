# ParcelGuardian — EP0402 FIOT Group 6

An Arduino Uno parcel black box and a Flutter Android control centre.
**The phone is both the Wi-Fi hotspot and the server.**

```text
ADXL345 + NEO-6M → Uno → microSD (full recorded evidence)
                    └→ ESP-01 → phone hotspot → Android app :8080
                                                  └→ optional HTTPS → ThingSpeak
```

The SD card is the system of record. Phone and cloud history contain what got
through. Impact alerts run locally; network success is not required to record
an event. The app computes QUIET/LOST from the last accepted check-in. LOST is
a contact timeout, not proof of theft, destruction or damage.

The original design is preserved in [ORIGINAL_DESIGN.md](docs/ORIGINAL_DESIGN.md).
The phone-server change follows the user's updated deployment requirement.

## Start here

1. Install the ParcelGuardian APK on your Android phone.
2. Turn on a **2.4 GHz WPA2 hotspot**. Leave the app open during reception.
3. In app Settings, refresh/select the hotspot IPv4, save settings and copy the
   generated firmware configuration. Do not assume a fixed Android gateway IP.
4. Copy `firmware/ParcelGuardian/Secrets.example.h` to `Secrets.h`. Set the
   hotspot SSID/password, phone address, matching receiver key and shipment ID.
   Keep `Secrets.h` private; it is gitignored.
5. Run the four bench tools first: EspAtTest, GpsRawTest, SdCardTest, AdxlDropTest.
6. Compile/upload ParcelGuardian for **Arduino Uno R3**, then press **Start** on
   the app's Overview screen. Serial console is **9600 baud**.

See [CMF Phone 2 Pro setup](docs/PHONE_SETUP.md) for the user's handset,
[protocol](docs/PROTOCOL.md), [audit](docs/AUDIT.md), [testing](docs/TESTING.md)
and [calibration](docs/CALIBRATION.md).

## Android app

* Overview: contact state, last receipt time, cumulative events, reported peak,
  local receiver start/stop, GPS status and pairing guidance.
* Map: OpenStreetMap basemap, sampled route, impact points, last recorded fix,
  separate SD/received sources, recenter and Google Maps handoff.
* Evidence: check-in timeline, severity filters, journey-leg labels, PGLOG.CSV
  import, receiver CSV export through Android's share sheet.
* Insights: GPS fix rate, peak plot, severity counts, longest observed check-in
  gap and labelled journey-leg summaries. Phone summaries are not exact counts
  of all offline events; SD event rows are analysed separately.
* Settings: shipment/name, pairing key, phone address, configurable silence
  windows, light/dark/system appearance and optional cloud relay.
* Sample journey: explicitly labelled, isolated from real reception and cloud.

Receiver data is saved to an append-only journal before acknowledgement. A
torn final append is repaired on startup; corruption inside completed rows is
surfaced. Replayed identical check-ins return the original receipt/entry ID.
Keys use Android secure storage. No account or phone GPS permission is needed.

Reception is an explicit foreground session: **keep the app open**. Its display
stays awake while receiving. Background/screen-off operation is not promised.
Android hotspot setup remains manual. Map tiles and optional cloud mirroring
need internet; local reception, stored coordinates and evidence do not.

## Build

Requirements: Arduino CLI with `arduino:avr` and Arduino **SD** library;
Flutter stable, its bundled Dart SDK, Android SDK and JDK 17+.

```powershell
arduino-cli core install arduino:avr
arduino-cli lib install SD
Copy-Item firmware/ParcelGuardian/Secrets.example.h firmware/ParcelGuardian/Secrets.h
# Edit Secrets.h before uploading; placeholders are sufficient only to compile.
arduino-cli compile --fqbn arduino:avr:uno --warnings all firmware/ParcelGuardian

cd app
flutter pub get
flutter analyze
flutter test
flutter build apk --release
```

APK: `app/build/app/outputs/flutter-apk/app-release.apk`. The classroom build is
signed with the machine's Android debug certificate for sideloading. Use a
dedicated signing key for published distribution.

`tools/build.ps1` runs the firmware builds, app checks and APK build. The native
firmware regression harness is `tests/firmware/run.cmd` (MSVC on Windows),
exercising actual GPS/uplink/EEPROM source with mocked hardware. It models AVR
packing but does not reproduce AVR timing, floating-point width or physical I/O.

## Shield pin map

| Function | Uno pins |
|---|---|
| ADXL345 SDA/SCL | A4/A5, bounded direct TWI |
| microSD MISO/MOSI/SCK/CS | D12/D11/D13/D10 |
| Buzzer / shield LED | D9 / D8 |
| DIP switches | D7 / D6 |
| NEO-6M Uno RX/TX | D4/D5, `SoftwareSerial(4,5)` |
| ESP-01 Uno RX/TX | D2/D3, `SoftwareSerial(2,3)` |

D13 is SPI clock, not a free LED. Verify shield power/level shifting: ESP-01 is
a 3.3 V device. ESP-01 must have AT firmware at 9600 baud; use a suitable USB-UART
adapter to configure a module initially at 115200 rather than assuming Uno
SoftwareSerial will handle it reliably.

## Detection, modes and console

20 Hz vector-magnitude sampling: `delta = abs(sqrt(x²+y²+z²) - baseline)`.
Default rough threshold 0.40 g, severe 1.20 g, debounce 2 seconds. These are
**unvalidated starting values**. Brief shocks can be missed; run and document
the physical calibration procedure before claiming detection performance.

| D7/D6 logical mode | Behaviour |
|---|---|
| 00 NORMAL | 30-second SD heartbeat, 20-second upload |
| 01 DEMO | 5-second heartbeat, 10-second phone upload |
| 10 SILENT | Normal timings, buzzer muted |
| 11 READOUT | Stop tracking and dump SD to serial |

Switch polarity is undocumented; inspect console `m` and adjust `DIP_ACTIVE_LOW`.

Console: `?` help, `s` status, `r` RAM, `m` mode, `c` calibrate while still,
`b` buzzer, `l` LED, `u` upload, `g` GPS, `a` arrived, `d` dump,
`w` erase SD log, `z` reset event counters. Wipe/reset commands are deliberate
destructive console actions; the app does not invoke them remotely.

EEPROM counters use two alternating checksum-protected slots, with a commit
marker written last and migration from the old layout. A new shipment ID starts
a new event count. Boot/session count continues; it eventually wraps at 65535.

## Optional ThingSpeak cloud

The phone relay keeps the course's cloud-computing path available while making
local reception independent of internet. Create a private channel with:

| Field | Meaning |
|---|---|
| 1 | Latitude, blank without fix |
| 2 | Longitude, blank without fix |
| 3 | Peak delta-g since the previous successful device upload |
| 4 | 0 normal, 1 rough, 2 severe, 3 no fix |
| 5 | Cumulative event count |

Enable the app's HTTPS relay and enter its write key. It mirrors one queued row
every 20 seconds and retains a persistent progress cursor. Verify actual cloud
entries before demonstrating the cloud assessment requirement.

For the legacy Uno-to-ThingSpeak path, set `UPLINK_PHONE=0`; demo uploads are
then 20 seconds. That older route is plain HTTP. Phone mode is the default.

## Limits and test status

Only one SoftwareSerial port receives at a time: GPS characters are lost during
ESP work. Sensor/indicator callbacks service AT waits, but SD, EEPROM and UART
operations still cause sampling gaps. SD close/flush cannot guarantee survival
of abrupt power loss or FAT corruption. A missing/broken card loses the local
record. No signed/tamper-proof evidence or automatic SD replay is claimed.

The supplied firmware binary uses placeholder network values. Recompile after
pairing the actual phone. Physical shield, GPS, impact calibration, battery,
power-cut and CMF hotspot tests require the user's hardware and remain separate
from software builds and emulator tests.

Local HTTP is a classroom transport and exposes the pairing key to an observer.
Use a private demonstration hotspot and data. See [security](docs/SECURITY.md).
