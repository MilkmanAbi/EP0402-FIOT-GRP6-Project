/*
 * Config.h — ParcelGuardian build configuration
 *
 * EP0402 Fundamentals of IoT — project idea #8, Wireless Tracker (People / Asset) shield.
 * Everything tunable lives here so the .cpp files never hold magic numbers.
 *
 * TARGET: Arduino Uno R3 (ATmega328P) — 32 KB flash, 2 KB SRAM, 1 KB EEPROM.
 * The 2 KB SRAM budget drives every design decision in this firmware.
 */
#ifndef PG_CONFIG_H
#define PG_CONFIG_H

#include <Arduino.h>
#ifndef UPLINK_PHONE
#define UPLINK_PHONE 1
#endif

/* ------------------------------------------------------------------ *
 *  PIN MAP — taken from the supplied shield documentation.
 *  Do not reassign these without checking the board silkscreen.
 * ------------------------------------------------------------------ */
#define PIN_SD_MISO     12   /* SPI — fixed by hardware        */
#define PIN_SD_MOSI     11   /* SPI — fixed by hardware        */
#define PIN_SD_SCK      13   /* SPI — also the on-board LED pin */
#define PIN_SD_CS       10   /* SPI chip select                 */
#define PIN_BUZZER       9
#define PIN_LED          8   /* shield LED, NOT the D13 one     */
#define PIN_DIP_1        7
#define PIN_DIP_2        6
#define PIN_GPS_TX       5   /* Uno transmits -> NEO-6M RX      */
#define PIN_GPS_RX       4   /* Uno receives  <- NEO-6M TX      */
#define PIN_ESP_TX       3   /* Uno transmits -> ESP-01 RX      */
#define PIN_ESP_RX       2   /* Uno receives  <- ESP-01 TX      */
/* ADXL345 sits on A4 (SDA) / A5 (SCL) and is reached through the bounded TWI driver. */

/* SoftwareSerial is constructed as (rxPin, txPin) from the UNO's point of
 * view, so the ports are gps(4,5) and esp(2,3). Getting this backwards is
 * the single most common wiring mistake on this shield. */

/* ------------------------------------------------------------------ *
 *  FEATURE FLAGS — set a module to 0 to compile it out entirely.
 *  Useful if SRAM gets tight, or to bench-test one subsystem alone.
 * ------------------------------------------------------------------ */
#define ENABLE_SD         1   /* microSD black-box logging            */
#define ENABLE_ESP        1   /* ESP-01 WiFi uplink to ThingSpeak     */
#define ENABLE_GPS        1   /* NEO-6M position                      */
#define ENABLE_WATCHDOG   1   /* reboot if a transaction wedges       */
#define ENABLE_CONSOLE    1   /* serial command console (see README)  */

/* Passive buzzers need a driven frequency (tone()); active buzzers just
 * need DC. The shield's SG1 is small and cylindrical — if you hear nothing
 * with 1, set this to 0 and try again. */
#define BUZZER_PASSIVE    1

/* DIP switches: 1 = closed switch pulls the pin to GND (INPUT_PULLUP).
 * The shield's polarity is not documented, so verify with the 'm' console
 * command and flip this if the reported mode is inverted. */
#define DIP_ACTIVE_LOW    1

/* ------------------------------------------------------------------ *
 *  SERIAL / MODULE BAUD RATES
 * ------------------------------------------------------------------ */
#define BAUD_CONSOLE   9600UL
#define BAUD_GPS       9600UL
#define BAUD_ESP       9600UL   /* SoftwareSerial is unreliable above this */

/* ------------------------------------------------------------------ *
 *  IMPACT DETECTION
 *
 *  We compare the magnitude of the acceleration vector against a baseline
 *  captured at power-on, so the threshold is orientation-independent — a
 *  parcel's attitude inside a truck is unknown and changes constantly.
 *
 *  These tier boundaries are STARTING POINTS. Calibrate them with the drop
 *  test in docs/CALIBRATION.md before claiming any accuracy.
 * ------------------------------------------------------------------ */
#define IMPACT_ROUGH_G      0.40f   /* below this = normal handling      */
#define IMPACT_SEVERE_G     1.20f   /* at/above this = drop or throw     */
#define IMPACT_DEBOUNCE_MS  2000UL  /* one drop logs once, not forty     */
#define ACCEL_PERIOD_MS     50UL    /* 20 Hz sampling                    */
#define CALIBRATION_SAMPLES 64      /* averaged while the box sits still */

/* Motion above this (but below ROUGH) counts as "the parcel is moving",
 * which is what promotes PREP -> TRANSIT. */
#define MOTION_G            0.12f

/* ------------------------------------------------------------------ *
 *  TIMING — normal operation
 * ------------------------------------------------------------------ */
#define GPS_HEARTBEAT_MS    30000UL  /* periodic position row to the SD card */
#define UPLOAD_PERIOD_MS    20000UL  /* ThingSpeak free tier documents 15 s  */
#define PREP_TIMEOUT_MS     120000UL /* leave PREP even if never picked up   */
#define ESP_RETRY_MS        30000UL  /* backoff after a failed join          */

/* ------------------------------------------------------------------ *
 *  TIMING — DIP demo mode (everything accelerated so an examiner sees
 *  the whole lifecycle inside a minute).
 * ------------------------------------------------------------------ */
#define DEMO_HEARTBEAT_MS    5000UL
#if UPLINK_PHONE
#define DEMO_UPLOAD_MS      10000UL
#else
#define DEMO_UPLOAD_MS      20000UL
#endif
#define DEMO_PREP_TIMEOUT_MS 8000UL

/* ------------------------------------------------------------------ *
 *  ESP-01 / ThingSpeak
 *
 *  Field mapping (keep in step with the pitch deck appendix):
 *    field1 = latitude        field4 = status code
 *    field2 = longitude       field5 = cumulative event count
 *    field3 = peak delta-g since the previous upload
 *
 *  Status codes: 0 normal | 1 rough handling | 2 severe impact | 3 no GPS fix
 * ------------------------------------------------------------------ */
#define THINGSPEAK_HOST  "api.thingspeak.com"
#define THINGSPEAK_PORT  80

/* Default: the Android phone is both hotspot and HTTP receiver.
 * Set UPLINK_PHONE=0 for the legacy direct ThingSpeak build. */
#define PHONE_SERVER_PORT 8080
#define ESP_CMD_TIMEOUT    4000UL
#define ESP_JOIN_TIMEOUT  15000UL
#define ESP_TCP_TIMEOUT    8000UL
#define ESP_REPLY_BUF        48   /* bytes — keep small, SRAM is precious */

/* ------------------------------------------------------------------ *
 *  microSD
 * ------------------------------------------------------------------ */
#define LOG_FILENAME   "PGLOG.CSV"   /* 8.3 name — FAT short-name limit */
#define SD_LINE_BUF            96    /* one CSV row, built in place     */

/* ------------------------------------------------------------------ *
 *  EEPROM
 * ------------------------------------------------------------------ */
#define PERSIST_ADDR    0
#define PERSIST_MAGIC   0x5047       /* 'PG' — marks a valid record     */

#endif /* PG_CONFIG_H */
