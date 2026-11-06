# Audit and implementation record

Date: 6 November 2026. Baseline: `0c57389` from the public GitHub repository.

## Scope and sources

The two supplied folders contain 92 ordinary files, plus Git metadata. Source
documents, slide text/notes, specifications, rubrics, diagrams, class sample
sketches and the project firmware were inspected. All six ZIPs were extracted;
the full text corpus also includes their library sources and reference PDFs.
PDF pages were rendered for inspection. Archived project-brief differences were
checked against the loose brief. Git object databases are version-control
internals, not additional design instructions.

The project-owned firmware received a source review. Bundled fingerprint, RFID,
DHT, SD and SparkFun vendor archives were indexed/extracted; they have **not**
received an exhaustive line-by-line security audit. Extraction is not a claim of
human-level review of every third-party document or embedded image.

Project-document instructions were treated as reference requirements, separately
from the user's explicit request. The latest request chooses a phone that is
both hotspot and server. The original README's SD-first evidence, magnitude-based
impact detection, app-side silence detection and shield pin map remain controlling
design choices. The earlier four-field GPS starter guide is not the project's
later five-field impact schema.

## Findings and changes

| Finding | Resolution |
|---|---|
| The 128-byte uplink path could overflow with allowed longer keys | Removed query buffer and unbounded builders; count then stream the request |
| Network waits starved impact sampling and indicator updates | Service the sensor and indicators during AT waits and streamed transmission |
| An impact during an upload could be cleared without being sent | Clear pending severity/peak only if event count has not changed during the upload |
| No-fix status 3 could mask future severity 1/2 | Compute no-fix only for that upload; pending state holds impact severity only |
| Connection failure left software marked JOINED indefinitely | Failed uploads mark IDLE, enabling bounded rejoin/reprobe |
| AT echo/token handling could misidentify replies | Disable echo; match complete OK response; parse framed HTTP payload independently |
| `SEND OK` and arbitrary digits were insufficient server evidence | Require framed HTTP 200 plus a positive numeric ACK |
| Wi-Fi credentials with commas/quotes broke AT syntax | Escape AT parameter punctuation and exclude CR/LF |
| DEMO tried direct cloud writes every 10 seconds | Phone allows 10 seconds; direct ThingSpeak builds use 20 seconds |
| GPS void/quality-zero sentences left previous coordinates valid | Invalidate immediately; check numeric coordinate syntax, hemisphere, minutes and range |
| GPS UTC could appear current indefinitely after reception stopped | Apply a freshness limit and basic calendar/time validation |
| TWI timeout could leave controller half-way through a transaction | Reset/re-enable the TWI controller on timeout |
| SD code returned success without inspecting write error | Flush, inspect Print write status, count errors, surface SD faults on LED |
| SD wipe reported success after an unsuccessful removal | Check removal result before recreating a log |
| Watchdog was only kicked on newline during log dump | Kick on every streamed byte |
| EEPROM single-record rewrite could invalidate all counters on power loss | Alternate two checksum-protected slots, commit marker last; migrate valid legacy record |
| Firmware shipment ID and retained EEPROM ID could disagree | A changed configured shipment starts a new event count; boot/session continues |
| Recalibration could divide by zero or erase pending peaks | Require enough valid samples; preserve unreported peak |
| An SD upload row lost the peak because it was cleared first | Log the result before clearing the reported peak |
| Upload chirp could interrupt a severe alarm | Play success chirp only when buzzer is idle |
| Impact LED could be delayed until after a long uplink | Set the alert pattern immediately during impact handling |
| Bench test printed an uninitialised magnitude after read failure | Initialise its diagnostic sample |
| Claimed feature flags left GPS/ESP constructors active | Compile those implementations out with the feature flags |
| No Android app or local receiver existed | Added Flutter receiver, durable journal, maps, evidence, analysis and settings |

## Remaining limits that software cannot erase

1. The ADXL345 outputs at 100 Hz but the software samples at 20 Hz. Brief shocks
   may be missed. Bounded network callbacks improve coverage but are not a
   hard real-time guarantee. SD access, EEPROM writes, serial output and UART
   transmission still introduce gaps. Hardware FIFO/interrupt acquisition is
   a future improvement, requiring additional firmware space and bench work.
2. GPS and ESP share one SoftwareSerial receive listener. GPS data is unavailable
   during uplinks; fresh post-uplink sentences are required again.
3. The SD library/filesystem cannot guarantee power-loss durability. Flushing and
   closing limits buffered data but a sudden power cut can damage FAT metadata
   or lose recent data. An absent/full/damaged card prevents evidence recording.
4. EEPROM journalling preserves the previous committed slot. A cut during the
   latest counter write can lose that latest counter increment. It cannot make
   the SD card itself atomic. The 16-bit boot counter eventually wraps.
5. The 0.40/1.20 g thresholds are unvalidated defaults. The blank calibration
   sheet is still blank. No physical impact, power-cut, GPS or hotspot tests
   are claimed. Impacts do not prove damage or legal liability.
6. The app requires an open receiver session. No silent background service,
   automatic hotspot control or phone location collection is implemented.
7. Android hotspot addressing and client isolation vary. The CMF Phone 2 Pro
   on the user's reported Nothing OS 4.3 needs an actual reachability test.
8. Local HTTP exposes its pairing key to a network observer. A private hotspot
   reduces exposure but is not authenticated TLS or signed chain-of-custody.
9. The optional cloud relay needs a real private ThingSpeak channel/key and
   internet. The required cloud assessment element remains undemonstrated until
   that path is configured and tested. No channel/account was fabricated.
10. Offline SD rows are imported manually at handover; automatic SD backfill
    remains deferred as in the established design.

See TESTING.md for measured checks and outstanding physical acceptance tests.
