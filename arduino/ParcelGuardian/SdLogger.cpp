#include "SdLogger.h"
#include "Config.h"
#include "Fmt.h"

#if ENABLE_SD

#include <SPI.h>
#include <SD.h>
#include <avr/pgmspace.h>
#if ENABLE_WATCHDOG
  #include <avr/wdt.h>
  #define PG_KICK() wdt_reset()
#else
  #define PG_KICK() do {} while (0)
#endif

static bool     ready_   = false;
static uint32_t rows_    = 0;
static uint32_t errors_  = 0;

static const char HEADER_P[] PROGMEM =
  "ms,utc,state,event,delta_g,peak_g,events,lat,lon,sats,fix,uploaded";

bool sdOk()              { return ready_; }
uint32_t sdRowsWritten() { return rows_; }
uint32_t sdWriteErrors() { return errors_; }

bool sdBegin() {
  ready_ = false;

  /* The shield wires CS to D10. D10 must also be an output for the AVR's
   * SPI hardware to stay in master mode, which SD.begin() handles, but
   * being explicit costs nothing and has saved hours before now. */
  pinMode(PIN_SD_CS, OUTPUT);

  if (!SD.begin(PIN_SD_CS)) return false;
  ready_ = true;

  /* Write the CSV header exactly once, when the file is first created. */
  if (!SD.exists(LOG_FILENAME)) {
    File f = SD.open(LOG_FILENAME, FILE_WRITE);
    if (!f) { ready_ = false; return false; }
    char c;
    PGM_P p = HEADER_P;
    while ((c = pgm_read_byte(p++)) != 0) f.write(c);
    f.write('\n');
    f.close();
  }
  return true;
}

bool sdLogBoot(uint16_t bootCount, uint16_t shipmentId) {
  if (!ready_) return false;
  File f = SD.open(LOG_FILENAME, FILE_WRITE);
  if (!f) { errors_++; return false; }

  f.print(F("# session "));
  f.print(bootCount);
  f.print(F(" shipment "));
  f.print(shipmentId);
  f.print(F(" firmware ParcelGuardian\n"));
  f.flush();
  bool ok = !f.getWriteError();
  f.close();
  if (!ok) errors_++;
  return ok;
}

bool sdLogRow(const char *utc, const char *state, const char *event,
              float deltaG, float peakG, uint32_t eventCount,
              bool hasFix, double lat, double lon, uint8_t sats,
              bool uploaded) {

  if (!ready_) return false;

  File f = SD.open(LOG_FILENAME, FILE_WRITE);
  if (!f) { errors_++; return false; }

  char num[20];

  f.print(millis());            f.write(',');
  f.print(utc);                 f.write(',');
  f.print(state);               f.write(',');
  f.print(event);               f.write(',');

  fmtFixed(num, deltaG, 3); f.print(num); f.write(',');
  fmtFixed(num, peakG, 3);  f.print(num); f.write(',');

  f.print(eventCount);          f.write(',');

  /* No fix means empty columns. A stale coordinate written as if it were
   * current is worse than no coordinate at all — it is evidence that
   * quietly lies. */
  if (hasFix) {
    fmtFixed(num, (float)lat, 6); f.print(num); f.write(',');
    fmtFixed(num, (float)lon, 6); f.print(num); f.write(',');
  } else {
    f.print(F(",,"));
  }

  f.print(sats);                f.write(',');
  f.print(hasFix ? 1 : 0);      f.write(',');
  f.print(uploaded ? 1 : 0);
  f.write('\n');

  f.flush();
  bool ok = !f.getWriteError();
  f.close();
  if (ok) rows_++; else errors_++;
  return ok;
}

bool sdDump(Stream &out) {
  if (!ready_) return false;

  File f = SD.open(LOG_FILENAME, FILE_READ);
  if (!f) return false;

  /* A full log at 9600 baud takes far longer than the 8 second watchdog
   * window, so the dog gets kicked every line. Without this the handover
   * read-out reboots the tracker halfway through the evidence. */
  while (f.available()) {
    char c = (char)f.read();
    out.write(c);
    PG_KICK();
  }
  f.close();
  return true;
}

bool sdWipe() {
  if (!ready_) return false;
  if (SD.exists(LOG_FILENAME) && !SD.remove(LOG_FILENAME)) { errors_++; return false; }
  rows_ = 0;
  errors_ = 0;
  return sdBegin();   /* recreate with a fresh header */
}

#else  /* ENABLE_SD == 0 — stubs so the rest of the firmware still builds */

bool sdBegin()             { return false; }
bool sdOk()                { return false; }
bool sdLogBoot(uint16_t, uint16_t) { return false; }
bool sdLogRow(const char *, const char *, const char *, float, float,
              uint32_t, bool, double, double, uint8_t, bool) { return false; }
bool sdDump(Stream &)      { return false; }
bool sdWipe()              { return false; }
uint32_t sdRowsWritten()   { return 0; }
uint32_t sdWriteErrors()   { return 0; }

#endif /* ENABLE_SD */
