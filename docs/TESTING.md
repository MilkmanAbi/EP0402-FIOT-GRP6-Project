# Verification record

6 November 2026, Windows host. Tests are software evidence, not physical
calibration or proof that a particular shield/handset combination works.

## Toolchain

* Arduino CLI 1.3.1, Arduino AVR Boards 1.8.8, SD 1.3.0.
* Target `arduino:avr:uno`, `--warnings all`.
* Flutter upgraded from 3.44.6 to **3.47.9 stable** in `C:\flutter`.
* Bundled **Dart 3.13.7** is installed and available through `dart` on PATH.
* Android APK: package `sg.edu.sp.grp6.parcel_guardian`, version 1.0.0 (1),
  minimum API 24, target/compile API 36. Debug certificate signs release mode
  for classroom sideloading. `apksigner verify` passed.
* The pre-existing modified Flutter SDK pubspec.lock was backed up before
  restoring that file for the supported Flutter upgrade.

## Firmware

Default phone-server build: **31,790 / 32,256 bytes flash (98%)**,
**1,274 / 2,048 bytes globals**, leaving 774 bytes for stack/locals. Zero
Arduino compiler warnings. Only 466 bytes of flash remain: longer network
credentials/configuration strings or additional features can exceed the limit.
Recompile the real Secrets.h rather than assuming the example's size carries over.

Retained legacy direct-ThingSpeak build (`UPLINK_PHONE=0`) also compiled with
zero warnings: 31,604 bytes flash and 1,274 bytes global RAM.

| Sketch | Flash | Global RAM | Result |
|---|---:|---:|---|
| AdxlDropTest | 5,666 | 213 | Compiled |
| EspAtTest | 3,758 | 305 | Compiled |
| GpsRawTest | 3,992 | 315 | Compiled |
| SdCardTest | 12,360 | 839 | Compiled |

Native harness `tests/firmware/run.cmd` compiled and ran the actual Persist.cpp,
GpsModule.cpp, Fmt.cpp and EspClient.cpp using mocked hardware and AVR struct
packing. Passed:

* EEPROM interruption at each of 24 tested write-cut positions, generation
  rollover, shipment change and migration of a valid legacy record.
* Valid RMC coordinates; immediate invalidation for RMC void/GGA no fix;
  invalid hemisphere, malformed digits and out-of-range coordinates; stale
  fix/time handling and valid UTC formatting.
* Positive framed HTTP acknowledgement, every byte split across two IPD frames,
  wrong HTTP status, zero entry, HTML body, missing frame and truncated ACK.

MSVC emits host-specific packing/narrowing/deprecation warnings in the harness;
these are not Arduino target warnings. Native floating point and timing differ
from the Uno and do not validate real serial reception or sensor sampling gaps.

## Flutter app

`flutter analyze`: **No issues found.**

`flutter test`: **11 tests passed**, including real loopback HTTP requests:

* Exact WAITING/LIVE/QUIET/LOST boundaries and future-clock handling.
* No-fix check-ins, no-fix severe impact and valid zero coordinates.
* Malformed/non-finite/out-of-range telemetry rejection.
* Authentication, shipment and method/path rejection without changing contact.
* Flushed journal acknowledgement and restoration from disk.
* Idempotent identical retries, original receipt preservation, conflict rejection.
* Concurrent request entry IDs and journal restoration.
* Torn final append recovery; disk-write failure returns 503 without memory ACK.
* SD import sessions, missing GPS and offline events.
* Narrow phone layout at 1.25× text scaling, dashboard/settings/evidence views.

`flutter build apk --release`: **passed**, universal APK approximately 53 MB.
The final incremental release rebuild also passed. The APK installed and launched
in an Android API-35 emulator. Actual Android receiver checks passed: `/health`,
positive ACK, duplicate ACK without another row, wrong-key 401, accepted no-fix
packet and restored coordinates. The evidence timeline displayed all three
synthetic test rows. OpenStreetMap tiles loaded with the route/impact overlays
and attribution. The emulator initially suffered Pixel Launcher/System UI ANRs;
after reboot/startup settled these functional checks completed. These are not
tests of the user's physical phone or ESP radio.

The native Android file picker successfully imported the six-row synthetic SD
fixture. The app then still showed LOST based on the unchanged original receipt,
confirming that SD import does not fabricate new device contact.
Receiver CSV export opened Android's share sheet with `parcel-1001.csv`.
No share destination was selected and no records were sent to another service.

## Physical acceptance tests still required

1. Verify 3.3 V ESP supply and level shifting on the actual shield.
2. Verify AT firmware at 9600 and each bench sketch on hardware.
3. Obtain outdoor GPS fix; remove sky view and confirm current fix invalidates.
4. Run the calibration table with the tracker mounted in the actual parcel.
5. Configure the CMF Phone 2 Pro hotspot and prove `/health` reachability from
   a client. Pair matching keys/ID/address and verify positive Uno upload ACKs.
6. Disable/restore hotspot and observe reconnection and 20s/60s contact transitions.
7. Test no-card/full-card/read-only/failed-card handling and abrupt power cuts.
8. Import the actual SD CSV; compare against observed impacts and phone summaries.
9. Configure a real private ThingSpeak channel, enable HTTPS relay, and verify
   field values and timestamps. No cloud account or credentials were supplied.
10. Measure free SRAM under load with console `r`; static global size is not a
    measurement of maximum stack use. Check alarm and sampling behaviour during
    SD writes, Wi-Fi joins and failed uploads.
