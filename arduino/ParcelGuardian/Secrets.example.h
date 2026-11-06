/*
 * Secrets.example.h — copy this file to "Secrets.h" and fill it in.
 *
 *   cp Secrets.example.h Secrets.h      (or just duplicate it in the IDE)
 *
 * Secrets.h is listed in .gitignore and must never be committed. This repo
 * is public: a ThingSpeak write key pushed here is a key you have to rotate.
 *
 * SECURITY NOTE: the uplink in this firmware is plain HTTP on port 80,
 * because the ESP-01's older AT firmware makes validated TLS impractical.
 * That means the write key and the coordinates travel in clear text. Use
 * demo data only. Do not track a real person or a real shipment with this
 * build — see docs/SECURITY.md.
 */
#ifndef PG_SECRETS_H
#define PG_SECRETS_H

/* 2.4 GHz WPA2 network. A phone hotspot is the easiest option —
 * school WiFi with a captive portal will not work. */
#define WIFI_SSID      "YOUR_2G_WIFI_NAME"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

/* ThingSpeak channel WRITE key. The phone app gets the READ key —
 * they are different keys and the device only ever needs the write one. */
#define THINGSPEAK_WRITE_KEY "YOUR_THINGSPEAK_WRITE_KEY"

/* Identifies this tracker in the log and on the channel. Any 1..65535. */
#define SHIPMENT_ID 1001

/* In the app: Settings > Receiver lists the phone's actual IPv4 address.
 * Android hotspot addresses vary by manufacturer. Do not assume this
 * example is correct for your phone. Use a 16..32 character URL-safe key. */
#define PHONE_SERVER_HOST "192.168.43.1"
#define PHONE_API_KEY "CHANGE_ME_16_CHARS"


#endif /* PG_SECRETS_H */
