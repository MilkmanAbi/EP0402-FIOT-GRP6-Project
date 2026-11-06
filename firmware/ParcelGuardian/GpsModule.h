/*
 * GpsModule.h — NEO-6M position, with honest fix reporting.
 *
 * WHY A HAND-WRITTEN NMEA PARSER INSTEAD OF TinyGPSPlus
 *   TinyGPSPlus is a fine library, but it carries course, distance and
 *   speed maths this project never uses, and the full build with it on
 *   board came to 111% of the Uno's flash. We need five values — latitude,
 *   longitude, satellite count, fix validity and UTC — so we parse the two
 *   sentences that carry them and nothing else. That bought back about
 *   3.5 KB of flash and removed a dependency the teammate has to install.
 *
 * WHY THE CHECKSUM CHECK IS NOT OPTIONAL HERE
 *   Only one SoftwareSerial port can receive at a time, so the GPS goes
 *   deaf for a few seconds during every upload and every SD write steals
 *   characters too. Truncated and spliced sentences are therefore NORMAL
 *   on this hardware, not an anomaly. Every sentence is checksum-verified
 *   before a single field is believed.
 *
 * RULE THIS MODULE ENFORCES: a position that is not fresh is reported as
 * "no fix". We never hand a stale coordinate to the cloud or the SD log
 * dressed up as a live one.
 */
#ifndef PG_GPS_MODULE_H
#define PG_GPS_MODULE_H

#include <Arduino.h>
#include "Config.h"

void gpsBegin();

/* Hand the single SoftwareSerial listener slot back to the GPS. */
void gpsListen();

/* Drain the UART into the parser. Call this constantly. */
void gpsService();

/* Valid only if we have parsed a fix AND it is younger than maxAgeMs. */
bool gpsHasFix(uint32_t maxAgeMs = 10000UL);

double   gpsLat();
double   gpsLon();
uint8_t  gpsSats();
uint32_t gpsFixAgeMs();

/* Diagnostics — these three together tell you exactly where a GPS
 * problem lives: no chars = wiring or power; chars but no good
 * sentences = baud rate; good sentences but no fix = no sky view. */
uint32_t gpsCharsProcessed();
uint16_t gpsGoodSentences();
uint16_t gpsChecksumErrors();

/* UTC as "YYYY-MM-DD HH:MM:SS" into out (needs 20 bytes).
 * Returns false and writes "no-time" if the receiver has no time yet. */
bool gpsTimestamp(char *out, uint8_t len);

#endif /* PG_GPS_MODULE_H */
