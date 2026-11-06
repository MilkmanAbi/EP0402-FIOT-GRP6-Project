/*
 * Indicators.h — non-blocking buzzer and LED pattern engines.
 *
 * Nothing in here ever calls delay(). Both engines are step sequencers
 * driven from millis(), because the main loop has to keep servicing the
 * GPS and the accelerometer while the box is screaming about being dropped.
 *
 * Buzzer patterns are one-shot. LED patterns loop until changed.
 *
 * Timing note: tone() drives an interrupt at twice the output frequency,
 * which steals cycles from SoftwareSerial's bit-banged receive. A few NMEA
 * characters are lost while the alarm sounds. That is an accepted trade —
 * the alert matters more than one position fix, and the GPS re-acquires.
 */
#ifndef PG_INDICATORS_H
#define PG_INDICATORS_H

#include <Arduino.h>

enum BuzzPattern : uint8_t {
  BUZZ_NONE = 0,
  BUZZ_BOOT,     /* ascending triad at power-on                      */
  BUZZ_OK,       /* single confident blip — self-test passed         */
  BUZZ_FIX,      /* two high blips — GPS acquired a fix              */
  BUZZ_ROUGH,    /* two chirps — rough handling logged               */
  BUZZ_SEVERE,   /* warbling alarm — severe impact, someone hears it */
  BUZZ_UPLOAD,   /* soft tick — a row reached the cloud              */
  BUZZ_ERROR     /* three low tones — a subsystem failed             */
};

enum LedPattern : uint8_t {
  LED_OFF = 0,
  LED_SOLID,      /* GPS fix held, everything nominal   */
  LED_SLOW,       /* acquiring — no fix yet             */
  LED_FAST,       /* impact alert active                */
  LED_HEARTBEAT,  /* in transit, running, no fix        */
  LED_TRIPLE      /* fault: SD or accelerometer missing */
};

void indicatorsBegin();

/* Start a buzzer pattern. Interrupts whatever was playing.
 * Severity ordering is NOT enforced — the caller decides. */
void buzzPlay(BuzzPattern p);
void buzzStop();
bool buzzBusy();

/* Silence the buzzer entirely (DIP silent mode). The LED still works. */
void buzzMute(bool mute);
bool buzzMuted();

void ledSet(LedPattern p);
LedPattern ledCurrent();

/* Call every pass through loop(). Cheap: a couple of comparisons
 * unless a step boundary has been crossed. */
void indicatorsUpdate();

#endif /* PG_INDICATORS_H */
