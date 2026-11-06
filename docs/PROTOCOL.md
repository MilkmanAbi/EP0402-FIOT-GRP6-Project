# Phone receiver protocol

The phone owns a 2.4 GHz WPA2 hotspot and listens on TCP 8080. The Uno uses
the ESP-01's AT firmware as a TCP client. No PC server, router, account or
internet connection is required for local reception.

## Request

`GET /update` with URL query parameters:

| Parameter | Meaning |
|---|---|
| api_key | 16–32 URL-safe characters, copied from app Settings |
| id | Shipment ID, 1–65535 |
| boot | EEPROM boot/session count, 0–65535 |
| ms | Device uptime at the start of this upload, uint32 |
| field1 | Latitude, -90..90, or empty |
| field2 | Longitude, -180..180, or empty |
| field3 | Peak delta from resting acceleration, g, 0..40 |
| field4 | 0 normal, 1 rough, 2 severe, 3 no GPS fix |
| field5 | Cumulative event count, uint32 |

The five original README fields keep their meanings. GPS absence is also
represented by empty coordinate fields, so rough/severe reports can carry
no fix without suppressing the impact status. A status-3 row must not carry
coordinates. Zero latitude/longitude is valid and is never treated as missing.

Only GET is accepted. Duplicate query keys, malformed or non-finite numbers,
wrong shipment IDs, partial coordinates and impossible coordinate ranges are
rejected. `/health` returns a nonsensitive receiver banner without authentication.
There is no unauthenticated history/download endpoint.

## Acknowledgement

The response is HTTP 200, `Content-Length` set, `Connection: close`, with a
positive decimal entry ID followed by LF. The journal append is flushed
**before** that response. The receiver timestamps the row using phone UTC.

Error responses contain `0\n`: 400 invalid telemetry, 401 wrong key, 404 unknown
path, 405 wrong method, 414 excessive URI, or 503 storage failure. A failed
write does not enter in-memory history or update the contact clock.

The firmware decodes ESP `+IPD,<length>:` payloads, checks HTTP 200 and the
numeric body, and never trusts `SEND OK` alone. HTTP framing can span multiple
IPD frames. Close and error text outside IPD payloads is not interpreted as data.

The tuple `(shipment, boot, ms)` identifies a check-in. Repeating the identical
tuple and payload returns the original entry ID and receipt time. A conflicting
duplicate is rejected. A later firmware upload uses a new uptime and is a new
observation, even if its cumulative counter is unchanged. The device is not
implementing an SD replay queue.

## Data sources and clocks

* Phone check-ins are delivery summaries, timestamped when received.
* SD CSV is the recorded black-box history, including offline events.
* Importing SD data never resets the phone contact clock.
* Unavailable GPS UTC is `no-time`; device uptime/session still identifies a row.
* QUIET and LOST are computed by the app, with inclusive boundaries at T1/T2.
* A valid no-fix check-in restores LIVE contact. Position validity is separate.
* Map lines connect sampled fixes and do not establish the route between them.

## Optional cloud path

The phone can mirror durable receiver rows to ThingSpeak using validated HTTPS,
at most one attempted write every 20 seconds while open. Its cursor persists
after a positive cloud entry ID. Local ACKs do not depend on cloud availability.
The original five-field schema is preserved. Cloud `created_at` is relay time;
the original receipt timestamp is included in the ThingSpeak status text.
Cloud rows may be duplicated if the app is killed after a cloud write but before
the cursor is persisted. The cloud remains a cache, not the authoritative log.

## Security and lifecycle

The pairing key is kept using Android secure storage. Local HTTP is not encrypted
and evidence is not signed. Use a private demonstration hotspot. The app's main
manifest disables Android backup of its local evidence/key material. Export
records before uninstalling; uninstall removes them.

Reception is an explicit **foreground session**. Keep the app open; its screen
stays awake while receiving. It does not promise background or screen-off
reception. Hotspot configuration is done by the user in Android Settings.

The firmware's older direct ThingSpeak route remains available with
`UPLINK_PHONE=0`; it has no per-device identity fields and its demo upload cadence
is clamped to 20 seconds. It retains the original cleartext cloud transport.
