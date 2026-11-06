/*
 * Fmt.h — one small fixed-point float formatter, used everywhere.
 *
 * WHY THIS EXISTS
 *   Two separate float-to-text engines were being linked into this sketch:
 *   avr-libc's dtostrf (via __ftoa_engine) and the Arduino core's
 *   Print::printFloat, reached through Serial.print(x, 3). Together they
 *   cost well over a kilobyte of flash on a part that only has 32 KB, and
 *   this firmware needed every byte of it back.
 *
 *   fmtFixed does the job with integer maths and no library support. Use
 *   it for every float that has to become text — SD rows, the ThingSpeak
 *   query string and the serial console all go through here.
 *
 * PRECISION NOTE
 *   On AVR, double is the same 32-bit float as float. That gives roughly
 *   seven significant digits, so for a longitude near 103 degrees the
 *   sixth decimal place is already below the noise floor — about a metre.
 *   We still print six places because that is the conventional format and
 *   ThingSpeak expects it, but do not read precision into the last digit.
 */
#ifndef PG_FMT_H
#define PG_FMT_H

#include <Arduino.h>

/* Write v to out with exactly dp decimal places, NUL-terminated.
 * Returns a pointer to the terminator so calls can be chained.
 * out needs 1 (sign) + 11 (integer) + 1 (point) + dp + 1 bytes. */
char *fmtFixed(char *out, float v, uint8_t dp);

/* Convenience: format into a scratch buffer and push it to a Stream. */
void fmtPrint(Print &p, float v, uint8_t dp);

#endif /* PG_FMT_H */
