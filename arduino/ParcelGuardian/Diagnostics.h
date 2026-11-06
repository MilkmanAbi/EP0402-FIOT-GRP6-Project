/*
 * Diagnostics.h — know how much rope is left.
 *
 * The ATmega328P has 2048 bytes of SRAM and no memory protection. When it
 * runs out, the stack grows down into the globals and the symptom is not
 * a crash but *wrong behaviour*: garbled strings, a corrupted file name,
 * an upload that silently stops. Printing the headroom at boot and on
 * demand turns that class of bug from a mystery into a number.
 *
 * Anything below about 250 bytes free should be treated as a warning.
 */
#ifndef PG_DIAGNOSTICS_H
#define PG_DIAGNOSTICS_H

#include <Arduino.h>

/* Bytes between the top of the heap and the current stack pointer. */
int freeRam();

#endif /* PG_DIAGNOSTICS_H */
