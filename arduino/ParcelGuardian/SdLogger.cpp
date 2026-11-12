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

static Sd2Card card_;
static SdVolume volume_;
static SdFile root_;

static bool     ready_   = false;
static bool     writeFault_ = false;
static uint32_t rows_    = 0;
static uint32_t errors_  = 0;

static const char HEADER_P[] PROGMEM =
  "ms,utc,state,event,delta_g,peak_g,events,lat,lon,sats,fix,uploaded";

bool sdOk()              { return ready_; }
uint32_t sdRowsWritten() { return rows_; }
uint32_t sdWriteErrors() { return errors_; }

static bool writeFailed() {
  ++errors_;
  writeFault_ = true;
  ready_ = false;
  return false;
}

static bool finishWrite(SdFile &file) {
  // SdFile exposes the result of the final sync through close().
  const bool written = !file.getWriteError();
  const bool synced = file.close();
  return (written && synced) || writeFailed();
}

bool sdBegin(bool readOnly) {
  ready_ = false;
  // A failed write can leave a dirty shared FAT cache. Never remount it
  // against a replacement card. A reset discards that cache.
  if (writeFault_) return false;
  if (root_.isOpen() && !root_.close()) return writeFailed();
  SdVolume::cacheClear();
  pinMode(PIN_SD_CS, OUTPUT);
  if (!card_.init(SPI_HALF_SPEED, PIN_SD_CS) ||
      !volume_.init(&card_) || !root_.openRoot(&volume_)) return false;
  if (readOnly) {
    ready_ = true;
    return true;
  }

  SdFile file;
  if (!file.open(&root_, LOG_FILENAME, O_WRITE | O_CREAT | O_APPEND)) return writeFailed();
  if (!file.fileSize()) {
    for (PGM_P p = HEADER_P; pgm_read_byte(p); ++p) file.write(pgm_read_byte(p));
    file.write('\n');
  }
  ready_ = finishWrite(file);
  return ready_;
}

bool sdLogBoot(uint16_t bootCount, uint16_t shipmentId) {
  if (!ready_) return false;
  SdFile f;
  if (!f.open(&root_, LOG_FILENAME, O_WRITE | O_APPEND)) return writeFailed();

  f.print(F("# session "));
  f.print(bootCount);
  f.print(F(" shipment "));
  f.print(shipmentId);
  f.print(F(" firmware ParcelGuardian\n"));
  return finishWrite(f);
}

bool sdLogRow(const char *utc, const char *state, const char *event,
              float deltaG, float peakG, uint32_t eventCount,
              bool hasFix, double lat, double lon, uint8_t sats,
              bool uploaded) {

  if (!ready_) return false;

  SdFile f;
  if (!f.open(&root_, LOG_FILENAME, O_WRITE | O_APPEND)) return writeFailed();

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

  const bool ok = finishWrite(f);
  if (ok) ++rows_;
  return ok;
}

bool sdDump(Stream &out) {
  if (!ready_) return false;

  SdFile f;
  if (!f.open(&root_, LOG_FILENAME, O_READ)) return false;

  /* A full log at 9600 baud takes far longer than the 8 second watchdog
   * window, so the dog gets kicked after each byte. Without this the handover
   * read-out reboots the tracker halfway through the evidence. */
  for (uint32_t remaining = f.fileSize(); remaining; --remaining) {
    const int c = f.read();
    if (c < 0 || out.write((uint8_t)c) != 1) { f.close(); return false; }
    PG_KICK();
  }
  f.close();
  return true;
}

bool sdWipe() {
  if (!ready_) return false;
  if (!SdFile::remove(&root_, LOG_FILENAME)) return writeFailed();
  rows_ = 0;
  errors_ = 0;
  return sdBegin();   /* recreate with a fresh header */
}

#else  /* ENABLE_SD == 0 — stubs so the rest of the firmware still builds */

bool sdBegin(bool)         { return false; }
bool sdOk()                { return false; }
bool sdLogBoot(uint16_t, uint16_t) { return false; }
bool sdLogRow(const char *, const char *, const char *, float, float,
              uint32_t, bool, double, double, uint8_t, bool) { return false; }
bool sdDump(Stream &)      { return false; }
bool sdWipe()              { return false; }
uint32_t sdRowsWritten()   { return 0; }
uint32_t sdWriteErrors()   { return 0; }

#endif /* ENABLE_SD */
