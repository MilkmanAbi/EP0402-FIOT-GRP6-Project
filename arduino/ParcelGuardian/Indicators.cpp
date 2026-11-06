#include "Indicators.h"
#include "Config.h"
#include <avr/pgmspace.h>

/* ------------------------------------------------------------------ *
 *  Pattern tables. All of these live in flash, not SRAM.
 *  A step with freq 0 is a rest; a step with ms 0 terminates the list.
 * ------------------------------------------------------------------ */
struct ToneStep { uint16_t freq; uint16_t ms; };
struct LedStep  { uint8_t  on;   uint16_t ms; };

static const ToneStep PROGMEM P_BOOT[] = {
  { 880, 80 }, { 0, 40 }, { 1175, 80 }, { 0, 40 }, { 1568, 130 }, { 0, 0 }
};
static const ToneStep PROGMEM P_OK[] = {
  { 1568, 90 }, { 0, 0 }
};
static const ToneStep PROGMEM P_FIX[] = {
  { 2093, 55 }, { 0, 60 }, { 2093, 55 }, { 0, 0 }
};
static const ToneStep PROGMEM P_ROUGH[] = {
  { 2200, 70 }, { 0, 90 }, { 2200, 70 }, { 0, 0 }
};
/* Deliberately obnoxious: the point is that a handler standing next to
 * the parcel hears it and knows the box noticed. */
static const ToneStep PROGMEM P_SEVERE[] = {
  { 2600, 130 }, { 1900, 130 }, { 2600, 130 }, { 1900, 130 },
  { 2600, 130 }, { 1900, 130 }, { 2600, 200 }, { 0, 0 }
};
static const ToneStep PROGMEM P_UPLOAD[] = {
  { 3000, 25 }, { 0, 0 }
};
static const ToneStep PROGMEM P_ERROR[] = {
  { 300, 240 }, { 0, 120 }, { 300, 240 }, { 0, 120 }, { 300, 240 }, { 0, 0 }
};

static const LedStep PROGMEM L_SOLID[]     = { { 1, 500 }, { 0, 0 } };
static const LedStep PROGMEM L_SLOW[]      = { { 1, 900 }, { 0, 900 }, { 0, 0 } };
static const LedStep PROGMEM L_FAST[]      = { { 1, 90 },  { 0, 90 },  { 0, 0 } };
static const LedStep PROGMEM L_HEARTBEAT[] = { { 1, 60 }, { 0, 170 }, { 1, 60 }, { 0, 1300 }, { 0, 0 } };
static const LedStep PROGMEM L_TRIPLE[]    = { { 1, 80 }, { 0, 130 }, { 1, 80 }, { 0, 130 },
                                               { 1, 80 }, { 0, 900 }, { 0, 0 } };

/* ------------------------------------------------------------------ *
 *  Engine state
 * ------------------------------------------------------------------ */
static const ToneStep *buzzSeq_   = NULL;
static uint8_t         buzzIdx_   = 0;
static uint32_t        buzzNextMs_= 0;
static bool            buzzMuted_ = false;

static const LedStep  *ledSeq_    = NULL;
static uint8_t         ledIdx_    = 0;
static uint32_t        ledNextMs_ = 0;
static LedPattern      ledPat_    = LED_OFF;

static void toneOn(uint16_t freq) {
#if BUZZER_PASSIVE
  if (freq) tone(PIN_BUZZER, freq); else noTone(PIN_BUZZER);
#else
  /* Active buzzer: it generates its own tone, we just gate the supply. */
  digitalWrite(PIN_BUZZER, freq ? HIGH : LOW);
#endif
}

static void toneOff() {
#if BUZZER_PASSIVE
  noTone(PIN_BUZZER);
#endif
  digitalWrite(PIN_BUZZER, LOW);
}

void indicatorsBegin() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  toneOff();
  digitalWrite(PIN_LED, LOW);
  ledSet(LED_OFF);
}

void buzzPlay(BuzzPattern p) {
  if (buzzMuted_ || p == BUZZ_NONE) { buzzStop(); return; }

  switch (p) {
    case BUZZ_BOOT:   buzzSeq_ = P_BOOT;   break;
    case BUZZ_OK:     buzzSeq_ = P_OK;     break;
    case BUZZ_FIX:    buzzSeq_ = P_FIX;    break;
    case BUZZ_ROUGH:  buzzSeq_ = P_ROUGH;  break;
    case BUZZ_SEVERE: buzzSeq_ = P_SEVERE; break;
    case BUZZ_UPLOAD: buzzSeq_ = P_UPLOAD; break;
    case BUZZ_ERROR:  buzzSeq_ = P_ERROR;  break;
    default:          buzzSeq_ = NULL;     break;
  }
  buzzIdx_ = 0;
  buzzNextMs_ = millis();   /* fire the first step on the next update() */
}

void buzzStop() {
  buzzSeq_ = NULL;
  buzzIdx_ = 0;
  toneOff();
}

bool buzzBusy() { return buzzSeq_ != NULL; }

void buzzMute(bool mute) {
  buzzMuted_ = mute;
  if (mute) buzzStop();
}

bool buzzMuted() { return buzzMuted_; }

void ledSet(LedPattern p) {
  if (p == ledPat_) return;        /* don't restart an identical pattern */
  ledPat_ = p;
  switch (p) {
    case LED_SOLID:     ledSeq_ = L_SOLID;     break;
    case LED_SLOW:      ledSeq_ = L_SLOW;      break;
    case LED_FAST:      ledSeq_ = L_FAST;      break;
    case LED_HEARTBEAT: ledSeq_ = L_HEARTBEAT; break;
    case LED_TRIPLE:    ledSeq_ = L_TRIPLE;    break;
    default:            ledSeq_ = NULL;        break;
  }
  ledIdx_ = 0;
  ledNextMs_ = millis();
  if (!ledSeq_) digitalWrite(PIN_LED, LOW);
}

LedPattern ledCurrent() { return ledPat_; }

void indicatorsUpdate() {
  uint32_t now = millis();

  /* ---- buzzer: one-shot, stops at the terminator ---- */
  if (buzzSeq_ && (int32_t)(now - buzzNextMs_) >= 0) {
    uint16_t freq = pgm_read_word(&buzzSeq_[buzzIdx_].freq);
    uint16_t ms   = pgm_read_word(&buzzSeq_[buzzIdx_].ms);

    if (ms == 0) {              /* end of pattern */
      buzzStop();
    } else {
      toneOn(freq);
      buzzNextMs_ = now + ms;
      buzzIdx_++;
    }
  }

  /* ---- LED: loops forever ---- */
  if (ledSeq_ && (int32_t)(now - ledNextMs_) >= 0) {
    uint8_t  on = pgm_read_byte(&ledSeq_[ledIdx_].on);
    uint16_t ms = pgm_read_word(&ledSeq_[ledIdx_].ms);

    if (ms == 0) {              /* wrap around */
      ledIdx_ = 0;
      on = pgm_read_byte(&ledSeq_[0].on);
      ms = pgm_read_word(&ledSeq_[0].ms);
    }
    digitalWrite(PIN_LED, on ? HIGH : LOW);
    ledNextMs_ = now + ms;
    ledIdx_++;
  }
}
