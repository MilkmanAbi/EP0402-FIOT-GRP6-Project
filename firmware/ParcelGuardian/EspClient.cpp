#include "EspClient.h"
#include "Config.h"
#include "Secrets.h"
#include "GpsModule.h"
#include "Fmt.h"
#include <SoftwareSerial.h>
#include <avr/pgmspace.h>
#if ENABLE_WATCHDOG
  #include <avr/wdt.h>
  #define PG_KICK() wdt_reset()
#else
  #define PG_KICK() do {} while (0)
#endif

static SoftwareSerial espPort(PIN_ESP_RX, PIN_ESP_TX);

static EspState state_       = ESP_DOWN;
static uint16_t uploadsOk_   = 0;
static uint16_t uploadsFail_ = 0;
static uint32_t lastOkMs_    = 0;

/* One scratch buffer for the query string. The single biggest RAM
 * allocation in this module — do not grow it casually.
 *
 * The builders below are unbounded for speed and size, so the only thing
 * standing between a long write key and a buffer overrun is this check.
 * A ThingSpeak key is 16 characters; the assert leaves generous room. */
static char path_[128];

static_assert(sizeof(THINGSPEAK_WRITE_KEY) <= 48,
              "THINGSPEAK_WRITE_KEY too long for path_ buffer in EspClient.cpp");

/* Every constant string lives in flash. A bare "literal" on AVR is copied
 * into SRAM at startup, which on a 2 KB part is a real cost. */
static const char HOST_P[]  PROGMEM = THINGSPEAK_HOST;
static const char KEY_P[]   PROGMEM = THINGSPEAK_WRITE_KEY;
static const char HDR1_P[]  PROGMEM = " HTTP/1.1\r\nHost: ";
static const char HDR2_P[]  PROGMEM = "\r\nConnection: close\r\n\r\n";

/* ------------------------------------------------------------------ *
 *  Low-level AT plumbing
 * ------------------------------------------------------------------ */

static void printP(PGM_P p) {
  char c;
  while ((c = pgm_read_byte(p++)) != 0) espPort.write(c);
}

/* Tiny string builders used instead of snprintf. Each returns the new
 * end-of-string so calls chain. */
static char *appendP(char *d, PGM_P s) {
  char c;
  while ((c = pgm_read_byte(s++)) != 0) *d++ = c;
  *d = '\0';
  return d;
}
static char *appendS(char *d, const char *s) {
  while (*s) *d++ = *s++;
  *d = '\0';
  return d;
}
static char *appendU32(char *d, uint32_t v) {
  ultoa(v, d, 10);
  while (*d) d++;
  return d;
}

static void espFlushInput() {
  uint8_t guard = 120;
  while (espPort.available() && guard--) espPort.read();
}

/* Wait for one of two PROGMEM tokens in the incoming stream.
 * Returns 1 for tokA, 2 for tokB, 0 on timeout.
 *
 * A 16-character sliding window is compared instead of buffering the whole
 * reply, so this costs ~20 bytes of stack however chatty the module is.
 * Tokens must therefore be 16 characters or shorter. */
static uint8_t espWaitFor(PGM_P tokA, PGM_P tokB, uint32_t timeoutMs) {
  char a[17], b[17], win[17];
  strncpy_P(a, tokA, 16); a[16] = '\0';
  if (tokB) { strncpy_P(b, tokB, 16); b[16] = '\0'; } else b[0] = '\0';
  memset(win, 0, sizeof(win));

  uint32_t deadline = millis() + timeoutMs;
  while ((int32_t)(millis() - deadline) < 0) {
    PG_KICK();
    if (!espPort.available()) continue;

    char c = (char)espPort.read();
    memmove(win, win + 1, 15);
    win[15] = c;
    win[16] = '\0';

    if (a[0] && strstr(win, a)) return 1;
    if (b[0] && strstr(win, b)) return 2;
  }
  return 0;
}

/* Send a PROGMEM command with CRLF; 1 = OK, 2 = ERROR, 0 = timeout. */
static uint8_t espCmd(PGM_P cmd, uint32_t timeoutMs) {
  espFlushInput();
  printP(cmd);
  espPort.print(F("\r\n"));
  return espWaitFor(PSTR("OK"), PSTR("ERROR"), timeoutMs);
}

void espListen() {
  if (!espPort.isListening()) espPort.listen();
}

/* ------------------------------------------------------------------ *
 *  Public API
 * ------------------------------------------------------------------ */

bool espBegin() {
  espPort.begin(BAUD_ESP);
  espListen();

  /* Three attempts: a module that just powered up often misses the first. */
  uint8_t r = 0;
  for (uint8_t i = 0; i < 3 && r != 1; i++) {
    r = espCmd(PSTR("AT"), ESP_CMD_TIMEOUT);
  }
  if (r != 1) { state_ = ESP_DOWN; gpsListen(); return false; }

  espCmd(PSTR("AT+CWMODE=1"), ESP_CMD_TIMEOUT);   /* station mode      */
  espCmd(PSTR("AT+CIPMUX=0"), ESP_CMD_TIMEOUT);   /* single connection */
  state_ = ESP_IDLE;

  gpsListen();
  return true;
}

bool espJoin() {
  if (state_ == ESP_DOWN && !espBegin()) return false;

  espListen();
  espFlushInput();

  espPort.print(F("AT+CWJAP=\""));
  espPort.print(F(WIFI_SSID));
  espPort.print(F("\",\""));
  espPort.print(F(WIFI_PASSWORD));
  espPort.print(F("\"\r\n"));

  uint8_t r = espWaitFor(PSTR("OK"), PSTR("FAIL"), ESP_JOIN_TIMEOUT);
  state_ = (r == 1) ? ESP_JOINED : ESP_IDLE;

  gpsListen();
  return state_ == ESP_JOINED;
}

EspState espState()         { return state_; }
uint16_t espUploadsOk()     { return uploadsOk_; }
uint16_t espUploadsFailed() { return uploadsFail_; }
uint32_t espLastOkMs()      { return lastOkMs_; }

bool espUpload(bool hasFix, double lat, double lon,
               float peakDeltaG, uint8_t statusCode, uint32_t eventCount) {

  if (state_ != ESP_JOINED) { uploadsFail_++; return false; }

  espListen();

  /* ---- build the query string ---------------------------------- */
  char latS[16], lonS[16], gS[12];
  if (hasFix) {
    fmtFixed(latS, (float)lat, 6);
    fmtFixed(lonS, (float)lon, 6);
  } else {
    latS[0] = '\0';   /* blank, never a stale coordinate sold as live */
    lonS[0] = '\0';
  }
  fmtFixed(gS, peakDeltaG, 3);

  /* Built by hand. snprintf_P would drag in the float-capable vfprintf,
   * which on its own is most of a kilobyte of flash we do not have. */
  char *d = path_;
  d = appendP(d, PSTR("/update?api_key="));
  d = appendP(d, KEY_P);
  d = appendP(d, PSTR("&field1=")); d = appendS(d, latS);
  d = appendP(d, PSTR("&field2=")); d = appendS(d, lonS);
  d = appendP(d, PSTR("&field3=")); d = appendS(d, gS);
  d = appendP(d, PSTR("&field4=")); d = appendU32(d, statusCode);
  d = appendP(d, PSTR("&field5=")); d = appendU32(d, eventCount);

  /* ---- exact byte count for CIPSEND ----------------------------- */
  uint16_t len = 4                        /* "GET " */
               + strlen(path_)
               + strlen_P(HDR1_P)
               + strlen_P(HOST_P)
               + strlen_P(HDR2_P);

  /* ---- open the socket ------------------------------------------ */
  espFlushInput();
  espPort.print(F("AT+CIPSTART=\"TCP\",\""));
  printP(HOST_P);
  espPort.print(F("\","));
  espPort.print(THINGSPEAK_PORT);
  espPort.print(F("\r\n"));

  /* "ALREADY CONNECTED" is a usable state, so treat it as success. */
  if (espWaitFor(PSTR("OK"), PSTR("ALREADY"), ESP_TCP_TIMEOUT) == 0) {
    espCmd(PSTR("AT+CIPCLOSE"), ESP_CMD_TIMEOUT);
    uploadsFail_++; gpsListen(); return false;
  }

  /* ---- announce the length, wait for the '>' prompt -------------- */
  espFlushInput();
  espPort.print(F("AT+CIPSEND="));
  espPort.print(len);
  espPort.print(F("\r\n"));

  if (espWaitFor(PSTR(">"), NULL, ESP_CMD_TIMEOUT) != 1) {
    espCmd(PSTR("AT+CIPCLOSE"), ESP_CMD_TIMEOUT);
    uploadsFail_++; gpsListen(); return false;
  }

  /* ---- the request, streamed in pieces --------------------------- */
  espPort.print(F("GET "));
  espPort.print(path_);
  printP(HDR1_P);
  printP(HOST_P);
  printP(HDR2_P);

  if (espWaitFor(PSTR("SEND OK"), PSTR("ERROR"), ESP_TCP_TIMEOUT) != 1) {
    espCmd(PSTR("AT+CIPCLOSE"), ESP_CMD_TIMEOUT);
    uploadsFail_++; gpsListen(); return false;
  }

  /* ---- confirm the SERVER accepted it ----------------------------
   * ThingSpeak replies with the new entry id as the body. Zero means the
   * write was rejected — bad key, or faster than the rate limit. "SEND OK"
   * only means the ESP pushed bytes out of the door, so trusting it alone
   * reports success against a channel that is actually empty.          */
  bool accepted = false;
  bool inBody   = false;
  uint8_t newlines = 0;
  uint32_t deadline = millis() + ESP_TCP_TIMEOUT;

  while ((int32_t)(millis() - deadline) < 0) {
    PG_KICK();
    if (!espPort.available()) continue;
    char c = (char)espPort.read();

    if (!inBody) {
      /* headers finish at a blank line (\r\n\r\n) */
      if      (c == '\n') { if (++newlines >= 2) inBody = true; }
      else if (c != '\r') { newlines = 0; }
    } else {
      if      (c >= '1' && c <= '9') { accepted = true; break; }
      else if (c == '0')             { break; }   /* explicit rejection */
      else if (c == 'C' || c == 'E') { break; }   /* CLOSED / ERROR     */
    }
  }

  espCmd(PSTR("AT+CIPCLOSE"), ESP_CMD_TIMEOUT);

  if (accepted) { uploadsOk_++; lastOkMs_ = millis(); }
  else          { uploadsFail_++; }

  gpsListen();   /* hand the listener slot straight back to the GPS */
  return accepted;
}
