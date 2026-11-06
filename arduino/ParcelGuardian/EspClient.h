/*
 * EspClient.h — ESP-01 AT-command driver and phone/ThingSpeak uplink.
 *
 * Assumes the ESP-01 is running ESP-AT / AT-command firmware at 9600 baud,
 * NOT a standalone ESP8266 Arduino sketch. Modules often ship at 115200;
 * SoftwareSerial is not reliable that fast on an Uno, so reconfigure the
 * module with AT+UART_DEF=9600,8,1,0,0 before using this firmware. See
 * arduino/tools/EspAtTest for a passthrough that lets you do exactly that.
 *
 * THE UPLINK IS BEST-EFFORT BY DESIGN. Every call here can fail and the
 * system carries on: the microSD card is the system of record, the cloud
 * is a cache of whatever got through. Nothing in this file is allowed to
 * block the main loop for longer than its stated timeout.
 *
 * SECURITY: local HTTP on port 8080 by default; legacy ThingSpeak uses 80.
 * The pairing/write key and coordinates are cleartext. See docs/SECURITY.md.
 */
#ifndef PG_ESP_CLIENT_H
#define PG_ESP_CLIENT_H

#include <Arduino.h>

enum EspState : uint8_t {
  ESP_DOWN = 0,   /* no AT response — module missing or wrong baud */
  ESP_IDLE,       /* module answers, not joined to an AP           */
  ESP_JOINED      /* joined, ready to open a socket                */
};

/* Probe the module and put it in station mode. Safe to call repeatedly. */
bool espBegin();

/* Join the AP from Secrets.h. Blocking up to ESP_JOIN_TIMEOUT. */
bool espJoin();

EspState espState();

/* Take the SoftwareSerial listener slot. Remember to call gpsListen()
 * afterwards or the GPS stays deaf. espUpload() does this for you. */
void espListen();

/* GET one record to the configured receiver. Returns true only when the server
 * accepted the write (entry id != 0), not merely when the ESP said SEND OK —
 * those are very different things and conflating them is the classic
 * "why is my channel empty" bug.
 *
 * Pass hasFix=false to send blanks for lat/lon rather than stale values. */
bool espUpload(bool hasFix, double lat, double lon,
               float peakDeltaG, uint8_t statusCode, uint32_t eventCount);

/* Diagnostics for the console. */
uint16_t espUploadsOk();
uint16_t espUploadsFailed();
uint32_t espLastOkMs();

#endif /* PG_ESP_CLIENT_H */
