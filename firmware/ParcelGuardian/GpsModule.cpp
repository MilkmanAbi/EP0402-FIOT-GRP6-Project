#include "GpsModule.h"
#include <SoftwareSerial.h>

/* SoftwareSerial takes (rxPin, txPin) from the Uno's point of view. */
static SoftwareSerial gpsPort(PIN_GPS_RX, PIN_GPS_TX);

/* An NMEA sentence is capped at 82 characters including CRLF. Anything
 * longer is corruption, and we drop it rather than overrun. */
#define NMEA_MAX 84
static char    line_[NMEA_MAX];
static uint8_t lineLen_ = 0;
static bool    collecting_ = false;

static double   lat_ = 0.0, lon_ = 0.0;
static bool     haveFix_ = false;
static uint32_t fixMs_ = 0;
static uint8_t  sats_ = 0;

static uint16_t year_ = 0;
static uint8_t  month_ = 0, day_ = 0, hour_ = 0, minute_ = 0, second_ = 0;
static bool     haveTime_ = false;

static uint32_t chars_ = 0;
static uint16_t good_  = 0;
static uint16_t csErr_ = 0;

/* ------------------------------------------------------------------ *
 *  Field helpers — all integer maths, no strtod. Pulling in the
 *  floating-point scanner would cost well over a kilobyte of flash.
 * ------------------------------------------------------------------ */

/* Return a pointer to comma-separated field n, or NULL. Destructive-free:
 * we scan rather than tokenise, so the buffer stays intact. */
static const char *field(const char *s, uint8_t n) {
  uint8_t f = 0;
  while (*s) {
    if (f == n) return s;
    if (*s == ',') f++;
    s++;
  }
  return (f == n) ? s : NULL;
}

static uint8_t fieldLen(const char *f) {
  uint8_t n = 0;
  while (f[n] && f[n] != ',' && f[n] != '*') n++;
  return n;
}

static uint32_t digits(const char *s, uint8_t n) {
  uint32_t v = 0;
  while (n--) {
    if (*s < '0' || *s > '9') return v;
    v = v * 10 + (uint32_t)(*s++ - '0');
  }
  return v;
}

/* NMEA coordinates are ddmm.mmmm / dddmm.mmmm — degrees and decimal
 * minutes glued together, which is why they are never simply atof'd. */
static bool parseCoord(const char *f, double &out) {
  uint8_t len = fieldLen(f);
  if (len < 4) return false;

  uint8_t dot = 0;
  while (dot < len && f[dot] != '.') dot++;
  if (dot < 3 || dot == len) return false;

  uint8_t degDigits = dot - 2;
  double deg = (double)digits(f, degDigits);
  double min = (double)digits(f + degDigits, 2);

  double frac = 0.0, scale = 0.1;
  for (uint8_t i = dot + 1; i < len; i++) {
    if (f[i] < '0' || f[i] > '9') break;
    frac += (f[i] - '0') * scale;
    scale *= 0.1;
  }

  out = deg + (min + frac) / 60.0;
  return true;
}

static bool checksumOk(const char *s, uint8_t len) {
  /* s starts just after '$'. Checksum covers everything up to '*'. */
  uint8_t sum = 0, i = 0;
  while (i < len && s[i] != '*') sum ^= (uint8_t)s[i++];

  /* Both hex digits must exist INSIDE the buffer. Getting this bound
   * wrong reads past the end on a sentence truncated right at the '*',
   * which is exactly the kind of sentence this hardware produces when
   * the ESP-01 borrows the listener mid-transmission. */
  if (i >= len || s[i] != '*' || (uint8_t)(i + 2) >= len) return false;

  auto hex = [](char c) -> int8_t {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  };
  int8_t hi = hex(s[i + 1]), lo = hex(s[i + 2]);
  if (hi < 0 || lo < 0) return false;
  return sum == (uint8_t)((hi << 4) | lo);
}

static void applyTime(const char *f) {
  if (fieldLen(f) < 6) return;
  hour_   = (uint8_t)digits(f,     2);
  minute_ = (uint8_t)digits(f + 2, 2);
  second_ = (uint8_t)digits(f + 4, 2);
}

static void applyDate(const char *f) {
  if (fieldLen(f) < 6) return;
  day_   = (uint8_t)digits(f,     2);
  month_ = (uint8_t)digits(f + 2, 2);
  year_  = (uint16_t)(2000 + digits(f + 4, 2));
  haveTime_ = true;
}

/* ------------------------------------------------------------------ *
 *  Sentence handlers
 * ------------------------------------------------------------------ */

/* $xxGGA,time,lat,NS,lon,EW,quality,sats,hdop,... */
static void handleGGA() {
  const char *f;

  f = field(line_, 1); if (f) applyTime(f);

  f = field(line_, 6);
  uint8_t quality = (f && fieldLen(f)) ? (uint8_t)digits(f, 1) : 0;

  f = field(line_, 7);
  if (f && fieldLen(f)) sats_ = (uint8_t)digits(f, fieldLen(f) > 2 ? 2 : fieldLen(f));

  if (quality == 0) return;          /* no fix — leave the old one to age out */

  double la, lo;
  const char *fLat = field(line_, 2);
  const char *fNS  = field(line_, 3);
  const char *fLon = field(line_, 4);
  const char *fEW  = field(line_, 5);
  if (!fLat || !fNS || !fLon || !fEW) return;
  if (!parseCoord(fLat, la) || !parseCoord(fLon, lo)) return;

  if (*fNS == 'S') la = -la;
  if (*fEW == 'W') lo = -lo;

  lat_ = la; lon_ = lo;
  haveFix_ = true;
  fixMs_ = millis();
}

/* $xxRMC,time,status,lat,NS,lon,EW,speed,course,date,... */
static void handleRMC() {
  const char *f;

  f = field(line_, 1); if (f) applyTime(f);
  f = field(line_, 9); if (f) applyDate(f);

  f = field(line_, 2);
  if (!f || *f != 'A') return;        /* V = void */

  double la, lo;
  const char *fLat = field(line_, 3);
  const char *fNS  = field(line_, 4);
  const char *fLon = field(line_, 5);
  const char *fEW  = field(line_, 6);
  if (!fLat || !fNS || !fLon || !fEW) return;
  if (!parseCoord(fLat, la) || !parseCoord(fLon, lo)) return;

  if (*fNS == 'S') la = -la;
  if (*fEW == 'W') lo = -lo;

  lat_ = la; lon_ = lo;
  haveFix_ = true;
  fixMs_ = millis();
}

static void parseLine() {
  if (lineLen_ < 9) return;

  if (!checksumOk(line_, lineLen_)) { csErr_++; return; }
  good_++;

  /* Talker id varies (GP, GN, GL...), so match on the last three only. */
  if      (line_[2] == 'G' && line_[3] == 'G' && line_[4] == 'A') handleGGA();
  else if (line_[2] == 'R' && line_[3] == 'M' && line_[4] == 'C') handleRMC();
}

/* ------------------------------------------------------------------ *
 *  Public API
 * ------------------------------------------------------------------ */

void gpsBegin() {
  gpsPort.begin(BAUD_GPS);
  gpsPort.listen();
  lineLen_ = 0;
  collecting_ = false;
}

void gpsListen() {
  if (!gpsPort.isListening()) {
    gpsPort.listen();
    /* Whatever was mid-flight while we were deaf is now a fragment. */
    lineLen_ = 0;
    collecting_ = false;
  }
}

void gpsService() {
  /* Bounded drain so a chatty receiver cannot starve the main loop. */
  uint8_t guard = 96;
  while (gpsPort.available() && guard--) {
    char c = (char)gpsPort.read();
    chars_++;

    if (c == '$') {                 /* start of sentence */
      collecting_ = true;
      lineLen_ = 0;
      continue;
    }
    if (!collecting_) continue;

    if (c == '\r' || c == '\n') {
      line_[lineLen_] = '\0';
      parseLine();
      collecting_ = false;
      lineLen_ = 0;
      continue;
    }

    if (lineLen_ < NMEA_MAX - 1) {
      line_[lineLen_++] = c;
    } else {
      collecting_ = false;          /* overlong: corrupt, drop it */
      lineLen_ = 0;
    }
  }
}

bool gpsHasFix(uint32_t maxAgeMs) {
  return haveFix_ && ((uint32_t)(millis() - fixMs_) < maxAgeMs);
}

double   gpsLat()            { return lat_; }
double   gpsLon()            { return lon_; }
uint8_t  gpsSats()           { return sats_; }
uint32_t gpsFixAgeMs()       { return haveFix_ ? (millis() - fixMs_) : 0xFFFFFFFFUL; }
uint32_t gpsCharsProcessed() { return chars_; }
uint16_t gpsGoodSentences()  { return good_; }
uint16_t gpsChecksumErrors() { return csErr_; }

/* Built by hand rather than with snprintf_P: the formatted-output
 * machinery costs more flash than this whole module. */
static char *put2(char *d, uint8_t v) {
  *d++ = '0' + (v / 10) % 10;
  *d++ = '0' + v % 10;
  return d;
}

bool gpsTimestamp(char *out, uint8_t len) {
  if (len < 20) { if (len) out[0] = '\0'; return false; }

  if (!haveTime_) { strcpy_P(out, PSTR("no-time")); return false; }

  char *d = out;
  *d++ = '0' + (year_ / 1000) % 10;
  *d++ = '0' + (year_ / 100)  % 10;
  *d++ = '0' + (year_ / 10)   % 10;
  *d++ = '0' +  year_         % 10;
  *d++ = '-'; d = put2(d, month_);
  *d++ = '-'; d = put2(d, day_);
  *d++ = ' '; d = put2(d, hour_);
  *d++ = ':'; d = put2(d, minute_);
  *d++ = ':'; d = put2(d, second_);
  *d = '\0';
  return true;
}
