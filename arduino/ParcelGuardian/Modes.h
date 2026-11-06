/*
 * Modes.h — the two DIP switches on D7 and D6.
 *
 * Two switches give four modes, and all four earn their place:
 *
 *   NORMAL   full operation at real-world timings
 *   DEMO     everything accelerated so an examiner sees the whole
 *            lifecycle inside a minute (see the demo-scaling slide)
 *   SILENT   buzzer muted — for bench work and for testing in a room
 *            where a shrieking parcel is unwelcome
 *   READOUT  the handover mode: stop tracking, dump the SD black box to
 *            the serial console as the chain-of-custody record
 *
 * The shield's switch polarity is not documented. We drive the pins with
 * INPUT_PULLUP and assume a closed switch reads LOW; if the 'm' console
 * command reports the wrong mode, flip DIP_ACTIVE_LOW in Config.h.
 */
#ifndef PG_MODES_H
#define PG_MODES_H

#include <Arduino.h>

enum RunMode : uint8_t {
  MODE_NORMAL  = 0,
  MODE_DEMO    = 1,
  MODE_SILENT  = 2,
  MODE_READOUT = 3
};

void modesBegin();

/* Live read — the switches can be changed while running. */
RunMode modeRead();

/* Raw two-bit value, for the console so you can see what the pins do. */
uint8_t dipRaw();

const __FlashStringHelper *modeName(RunMode m);

#endif /* PG_MODES_H */
