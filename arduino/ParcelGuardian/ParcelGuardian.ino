/*
 * ParcelGuardian — wireless mishandling-prevention and loss-detection tracker
 * EP0402 Fundamentals of IoT, project idea #8 (Wireless Tracker, People / Asset shield)
 *
 * WHAT THIS FIRMWARE DOES
 *   DETECT  orientation-independent impact detection on the ADXL345
 *   DETER   buzzer and LED fire at the moment of mishandling, locally
 *   RECORD  every event and heartbeat appended to the microSD black box,
 *           whether or not there is any network
 *   REPORT  whatever reaches ThingSpeak drives the phone dashboard; the
 *           full log is read out at handover
 *
 * THE ONE IDEA THAT SHAPES EVERYTHING
 *   The SD card is the system of record. The cloud is a cache of whatever
 *   got through. A parcel spends almost its entire journey out of WiFi
 *   range, so a cloud-first design would fail to observe the exact window
 *   it exists to observe.
 *
 * WHAT THE DEVICE DELIBERATELY DOES NOT DO
 *   It never decides it is "lost". Silence is only observable from the
 *   other end, so that judgement belongs to the phone app, which compares
 *   the time since the last successful check-in against a threshold.
 *
 * HARDWARE LIMITS THAT ARE DESIGNED AROUND, NOT FOUGHT
 *   - 2 KB SRAM. No String class anywhere, every constant in PROGMEM.
 *   - Only one SoftwareSerial port can receive at a time, so the GPS
 *     holds the listener and the ESP-01 borrows it for each upload.
 *     NMEA characters are lost during that window; fix age makes that
 *     visible instead of hiding it.
 *   - SD writes block for milliseconds and cost GPS characters too. Rows
 *     are short and infrequent for exactly that reason.
 *
 * Console: send '?' over serial at 9600 baud for the command list.
 */

#include "Config.h"
#include "Secrets.h"
#include "Adxl345.h"
#include "ImpactDetector.h"
#include "Indicators.h"
#include "GpsModule.h"
#include "EspClient.h"
#include "SdLogger.h"
#include "Modes.h"
#include "Persist.h"
#include "Diagnostics.h"
#include "Fmt.h"

#if ENABLE_WATCHDOG
  #include <avr/wdt.h>
  #define PG_KICK() wdt_reset()
#else
  #define PG_KICK() do {} while (0)
#endif

/* ------------------------------------------------------------------ *
 *  State
 * ------------------------------------------------------------------ */
enum SysState : uint8_t {
  ST_CALIBRATE = 0,
  ST_PREP,
  ST_TRANSIT,
  ST_SYNC,
  ST_ARRIVED,
  ST_READOUT
};

static Adxl345       accel;
static ImpactDetector impact;

static SysState  state_         = ST_CALIBRATE;
static uint32_t  stateEnteredMs = 0;
static uint32_t  lastHeartbeat  = 0;
static uint32_t  lastUpload     = 0;
static uint32_t  lastEspRetry   = 0;
static uint32_t  alertUntilMs   = 0;
static RunMode   mode_          = MODE_NORMAL;
static bool      accelOk        = false;
static bool      sdReady        = false;
static bool      readoutDone    = false;
static bool      sawFixOnce     = false;
static uint8_t   pendingStatus  = 0;   /* 0 ok, 1 rough, 2 severe, 3 no fix */

/* Shared scratch for short names. Two 10-byte buffers beat two dozen
 * string literals sitting in SRAM. */
static char nameBufA_[10];
static char nameBufB_[10];

/* ------------------------------------------------------------------ *
 *  Small helpers
 * ------------------------------------------------------------------ */

static const char *stateName(SysState s) {
  switch (s) {
    case ST_CALIBRATE: strcpy_P(nameBufA_, PSTR("CALIB"));   break;
    case ST_PREP:      strcpy_P(nameBufA_, PSTR("PREP"));    break;
    case ST_TRANSIT:   strcpy_P(nameBufA_, PSTR("TRANSIT")); break;
    case ST_SYNC:      strcpy_P(nameBufA_, PSTR("SYNC"));    break;
    case ST_ARRIVED:   strcpy_P(nameBufA_, PSTR("ARRIVED")); break;
    default:           strcpy_P(nameBufA_, PSTR("READOUT")); break;
  }
  return nameBufA_;
}

static const char *eventName(uint8_t ev) {
  switch (ev) {
    case 1:  strcpy_P(nameBufB_, PSTR("ROUGH"));  break;
    case 2:  strcpy_P(nameBufB_, PSTR("SEVERE")); break;
    case 3:  strcpy_P(nameBufB_, PSTR("BOOT"));   break;
    case 4:  strcpy_P(nameBufB_, PSTR("UPLOAD")); break;
    default: strcpy_P(nameBufB_, PSTR("HB"));     break;   /* heartbeat */
  }
  return nameBufB_;
}

static uint32_t heartbeatPeriod() {
  return (mode_ == MODE_DEMO) ? DEMO_HEARTBEAT_MS : GPS_HEARTBEAT_MS;
}
static uint32_t uploadPeriod() {
  return (mode_ == MODE_DEMO) ? DEMO_UPLOAD_MS : UPLOAD_PERIOD_MS;
}
static uint32_t prepTimeout() {
  return (mode_ == MODE_DEMO) ? DEMO_PREP_TIMEOUT_MS : PREP_TIMEOUT_MS;
}

static void enterState(SysState s) {
  state_ = s;
  stateEnteredMs = millis();
  Serial.print(F("[state] "));
  Serial.println(stateName(s));
}

/* One log row, written to the card and echoed to the console. */
static void logRow(uint8_t eventKind, bool uploaded) {
  char utc[20];
  gpsTimestamp(utc, sizeof(utc));

  bool fix = gpsHasFix();
  sdLogRow(utc, stateName(state_), eventName(eventKind),
           impact.lastDeltaG(), impact.peakDeltaG(), impact.eventCount(),
           fix, fix ? gpsLat() : 0.0, fix ? gpsLon() : 0.0,
           gpsSats(), uploaded);
}

/* ------------------------------------------------------------------ *
 *  Upload
 * ------------------------------------------------------------------ */
#if ENABLE_ESP
static void attemptUpload() {
  SysState prev = state_;
  enterState(ST_SYNC);

  bool fix = gpsHasFix();
  const uint8_t status = pendingStatus ? pendingStatus : (fix ? 0 : 3);
  const uint32_t sentCount = impact.eventCount();

  bool ok = espUpload(fix,
                      fix ? gpsLat() : 0.0,
                      fix ? gpsLon() : 0.0,
                      impact.peakDeltaG(),
                      status,
                      sentCount);

  Serial.print(F("[uplink] upload "));
  Serial.println(ok ? F("accepted") : F("FAILED"));

  logRow(4 /* UPLOAD */, ok);
  if (ok) {
    if (!buzzBusy()) buzzPlay(BUZZ_UPLOAD);
    /* Events detected while the AT transaction ran still need reporting. */
    if (impact.eventCount() == sentCount) {
      impact.clearPeak();
      pendingStatus = 0;
    }
  }

  enterState(prev == ST_SYNC ? ST_TRANSIT : prev);
}
#endif

/* ------------------------------------------------------------------ *
 *  Impact handling
 * ------------------------------------------------------------------ */
static void handleImpact(ImpactLevel lvl) {
  if (lvl == IMPACT_NONE) return;

  if (lvl == IMPACT_SEVERE) {
    buzzPlay(BUZZ_SEVERE);
    if (pendingStatus < 2) pendingStatus = 2;
  } else {
    buzzPlay(BUZZ_ROUGH);
    if (pendingStatus < 1) pendingStatus = 1;
  }

  alertUntilMs = millis() + 3000UL;
  ledSet(LED_FAST);
  /* Preserve the local evidence before EEPROM and any uplink. */
  logRow(lvl == IMPACT_SEVERE ? 2 : 1, false);
  persistSetEventCount(impact.eventCount());
  indicatorsUpdate();

  Serial.print(F("[IMPACT] "));
  Serial.print(lvl == IMPACT_SEVERE ? F("SEVERE") : F("rough"));
  Serial.print(F("  delta="));
  fmtPrint(Serial, impact.lastDeltaG(), 3);
  Serial.print(F("g  total="));
  Serial.println(impact.eventCount());

}

/* Called from AT waits, never recursively calls the network client.
 * GPS reception is unavailable while the ESP owns SoftwareSerial. */
void pgServiceUplink() {
  indicatorsUpdate();
  if (accelOk && mode_ != MODE_READOUT) handleImpact(impact.update());
}

/* ------------------------------------------------------------------ *
 *  LED reflects the single most important thing happening right now
 * ------------------------------------------------------------------ */
static void updateLed() {
  if (!accelOk || !sdOk() || sdWriteErrors()) { ledSet(LED_TRIPLE); return; }                 /* fault */
  if ((int32_t)(millis() - alertUntilMs) < 0) { ledSet(LED_FAST); return; }
  if (gpsHasFix())            { ledSet(LED_SOLID);     return; }
  if (state_ == ST_TRANSIT)   { ledSet(LED_HEARTBEAT); return; }
  ledSet(LED_SLOW);
}

/* ------------------------------------------------------------------ *
 *  Serial console
 * ------------------------------------------------------------------ */
#if ENABLE_CONSOLE
static void printStatus() {
  Serial.println(F("--- ParcelGuardian ---"));
  Serial.print(F("state    ")); Serial.println(stateName(state_));
  Serial.print(F("mode     ")); Serial.println(modeName(mode_));
  Serial.print(F("uptime_s ")); Serial.println(millis() / 1000UL);
  Serial.print(F("shipment ")); Serial.println(persistShipmentId());
  Serial.print(F("session  ")); Serial.println(persistBootCount());

  Serial.print(F("accel    "));
  if (accelOk) {
    Serial.print(F("ok @0x")); Serial.print(accel.address(), HEX);
    Serial.print(F("  base=")); fmtPrint(Serial, impact.baseline(), 3);
    Serial.print(F("g  last=")); fmtPrint(Serial, impact.lastDeltaG(), 3);
    Serial.print(F("g  peak=")); fmtPrint(Serial, impact.peakDeltaG(), 3);
    Serial.println(F("g"));
  } else {
    Serial.println(F("MISSING"));
  }

  Serial.print(F("events   "));
  Serial.print(impact.eventCount());
  Serial.print(F(" (rough ")); Serial.print(impact.roughCount());
  Serial.print(F(", severe ")); Serial.print(impact.severeCount());
  Serial.println(F(")"));

  Serial.print(F("gps      "));
  if (gpsHasFix()) {
    fmtPrint(Serial, gpsLat(), 6); Serial.print(F(", "));
    fmtPrint(Serial, gpsLon(), 6);
    Serial.print(F("  sats=")); Serial.print(gpsSats());
    Serial.print(F("  age=")); Serial.print(gpsFixAgeMs());
    Serial.println(F("ms"));
  } else {
    Serial.print(F("no fix  sats=")); Serial.print(gpsSats());
    Serial.print(F("  chars=")); Serial.println(gpsCharsProcessed());
  }

  Serial.print(F("sd       "));
  if (sdReady) {
    Serial.print(F("ok  rows=")); Serial.print(sdRowsWritten());
    Serial.print(F("  errors=")); Serial.println(sdWriteErrors());
  } else {
    Serial.println(F("not present"));
  }

#if ENABLE_ESP
  Serial.print(F("wifi     "));
  switch (espState()) {
    case ESP_JOINED: Serial.print(F("joined")); break;
    case ESP_IDLE:   Serial.print(F("idle (not joined)")); break;
    default:         Serial.print(F("no AT response")); break;
  }
  Serial.print(F("  ok=")); Serial.print(espUploadsOk());
  Serial.print(F("  fail=")); Serial.println(espUploadsFailed());
#endif

  Serial.print(F("freeRAM  ")); Serial.println(freeRam());
  Serial.println(F("------------------------"));
}

static void printHelp() {
  Serial.println(F("? help s status r RAM m mode"));
  Serial.println(F("c calibrate b buzzer l LED"));
  Serial.println(F("u upload g GPS a ARRIVED"));
  Serial.println(F("d dump w wipe z reset"));
}

static void handleConsole() {
  if (!Serial.available()) return;
  char c = (char)Serial.read();

  switch (c) {
    case '?': printHelp(); break;
    case 's': printStatus(); break;
    case 'r': Serial.print(F("freeRAM ")); Serial.println(freeRam()); break;

    case 'm':
      Serial.print(F("dip=0b"));
      Serial.print(dipRaw(), BIN);
      Serial.print(F("  mode="));
      Serial.println(modeName(modeRead()));
      break;

    case 'c':
      Serial.println(F("Hold still"));
      if (impact.calibrate(CALIBRATION_SAMPLES)) {
        Serial.print(F("baseline ")); fmtPrint(Serial, impact.baseline(), 4);
        Serial.println(F(" g"));
        buzzPlay(BUZZ_OK);
      } else {
        Serial.println(F("calibration FAILED"));
        buzzPlay(BUZZ_ERROR);
      }
      break;

    case 'b': buzzPlay(BUZZ_SEVERE); Serial.println(F("buzzer test")); break;
    case 'l': ledSet(LED_FAST); alertUntilMs = millis() + 3000UL;
              Serial.println(F("LED test")); break;

#if ENABLE_ESP
    case 'u':
      Serial.println(F("Upload"));
      if (espState() != ESP_JOINED) {
        Serial.println(F("Joining"));
        espJoin();
      }
      attemptUpload();
      lastUpload = millis();
      break;
#endif

    case 'g':
      Serial.print(F("chars=")); Serial.print(gpsCharsProcessed());
      Serial.print(F("  good=")); Serial.print(gpsGoodSentences());
      Serial.print(F("  csErr=")); Serial.print(gpsChecksumErrors());
      Serial.print(F("  sats=")); Serial.println(gpsSats());
      if (gpsCharsProcessed() < 10)      Serial.println(F("No NMEA: D4/D5/power?"));
      else if (gpsGoodSentences() == 0)  Serial.println(F("Bad NMEA: baud?"));
      break;

    case 'a':
      enterState(ST_ARRIVED);
      logRow(0, false);
      buzzPlay(BUZZ_OK);
      break;

    case 'd':
      Serial.println(F("---- BEGIN LOG ----"));
      if (!sdDump(Serial)) Serial.println(F("No SD/log"));
      Serial.println(F("---- END LOG ----"));
      break;

    case 'w':
      Serial.println(sdWipe() ? F("log wiped") : F("wipe failed"));
      break;

    case 'z':
      persistReset(SHIPMENT_ID);
      impact.setEventCount(0);
      Serial.println(F("Counters reset"));
      break;

    default: break;   /* ignore CR/LF and stray characters */
  }
}
#endif /* ENABLE_CONSOLE */

/* ------------------------------------------------------------------ *
 *  setup
 * ------------------------------------------------------------------ */
void setup() {
  Serial.begin(BAUD_CONSOLE);
  delay(50);

  Serial.println();
  Serial.println(F("ParcelGuardian EP0402"));
  Serial.print(F("RAM ")); Serial.println(freeRam());

  modesBegin();
  indicatorsBegin();
  mode_ = modeRead();
  buzzMute(mode_ == MODE_SILENT);
  buzzPlay(BUZZ_BOOT);

  Serial.print(F("mode: ")); Serial.println(modeName(mode_));

  persistBegin(SHIPMENT_ID);
  Serial.print(F("session ")); Serial.print(persistBootCount());
  Serial.print(F(", shipment ")); Serial.println(persistShipmentId());

  /* ---- accelerometer (TWI is brought up inside begin()) ---- */
  accelOk = accel.begin();
  if (accelOk) {
    Serial.print(F("ADXL345 ok at 0x")); Serial.println(accel.address(), HEX);
  } else {
    Serial.println(F("ADXL345 missing"));
    buzzPlay(BUZZ_ERROR);
  }

  impact.begin(&accel);
  impact.setEventCount(persistEventCount());

  if (accelOk) {
    Serial.println(F("Calibrating: hold still"));
    if (impact.calibrate(CALIBRATION_SAMPLES)) {
      Serial.print(F("baseline ")); fmtPrint(Serial, impact.baseline(), 4);
      Serial.println(F(" g"));
    } else {
      Serial.println(F("Calibration failed: default 1g"));
    }
  }

  /* ---- GPS ---- */
#if ENABLE_GPS
  gpsBegin();
  Serial.println(F("GPS D4/D5 9600"));
#endif

  /* ---- SD ---- */
#if ENABLE_SD
  sdReady = sdBegin();
  Serial.print(F("microSD: "));
  Serial.println(sdReady ? F("ready") : F("NOT present (continuing)"));
  if (sdReady) sdLogBoot(persistBootCount(), persistShipmentId());
#endif

  /* ---- WiFi ---- *
   * This is the PREP step from the lifecycle: the unit joins the depot
   * access point while it is still on the packing bench. It can take
   * fifteen seconds, which is why the watchdog is armed afterwards. */
#if ENABLE_ESP
  Serial.println(F("ESP-01: probing..."));
  if (espBegin()) {
    Serial.println(F("ESP ready; joining"));
    if (espJoin()) Serial.println(F("WiFi joined"));
    else           Serial.println(F("Join failed; retry later"));
  } else {
    Serial.println(F("ESP: check baud/power"));
  }
  gpsListen();
#endif

  Serial.print(F("RAM ")); Serial.println(freeRam());
  if (freeRam() < 250) Serial.println(F("Low RAM"));

  if (accelOk) buzzPlay(BUZZ_OK);
#if ENABLE_CONSOLE
  printHelp();
#endif

  logRow(3 /* BOOT */, false);
  lastHeartbeat = lastUpload = millis();
  enterState(ST_PREP);

#if ENABLE_WATCHDOG
  /* Armed only now: joining an access point can legitimately take longer
   * than the timeout, and a boot loop is worse than a wedge. */
  wdt_enable(WDTO_8S);
#endif
}

/* ------------------------------------------------------------------ *
 *  loop
 * ------------------------------------------------------------------ */
void loop() {
  PG_KICK();

  indicatorsUpdate();
#if ENABLE_GPS
  gpsService();
#endif
#if ENABLE_CONSOLE
  handleConsole();
#endif

  /* ---- DIP switches are live; react to a change mid-run ---- */
  RunMode m = modeRead();
  if (m != mode_) {
    mode_ = m;
    buzzMute(mode_ == MODE_SILENT);
    readoutDone = false;
    Serial.print(F("[mode] ")); Serial.println(modeName(mode_));
  }

  /* ---- READOUT: the handover mode. Stop tracking, print the log. ---- */
  if (mode_ == MODE_READOUT) {
    if (state_ != ST_READOUT) enterState(ST_READOUT);
    if (!readoutDone) {
      readoutDone = true;
      Serial.println(F("--- SD RECORD ---"));
      if (!sdDump(Serial)) Serial.println(F("No SD/log"));
      Serial.println(F("---- END ----"));
      buzzPlay(BUZZ_OK);
    }
    ledSet(LED_SLOW);
    return;              /* no tracking while being read out */
  }
  if (state_ == ST_READOUT) enterState(ST_TRANSIT);

  /* ---- impact detection runs in every active state ---- */
  if (accelOk) handleImpact(impact.update());

  /* ---- announce the first fix; it is the moment the demo gets real ---- */
  if (!sawFixOnce && gpsHasFix()) {
    sawFixOnce = true;
    buzzPlay(BUZZ_FIX);
    Serial.println(F("[gps] first fix"));
  }

  uint32_t now = millis();

  /* ---- state machine ---- */
  switch (state_) {

    case ST_PREP:
      /* Dispatched: either someone picked the parcel up, or we have been
       * sitting on the bench long enough that it clearly already left. */
      if (impact.moving() || (uint32_t)(now - stateEnteredMs) > prepTimeout()) {
        enterState(ST_TRANSIT);
      }
      break;

    case ST_TRANSIT:
      if ((uint32_t)(now - lastHeartbeat) >= heartbeatPeriod()) {
        lastHeartbeat = now;
        logRow(0 /* heartbeat */, false);
      }
#if ENABLE_ESP
      if ((uint32_t)(now - lastUpload) >= uploadPeriod()) {
        lastUpload = now;
        if (espState() == ESP_JOINED) {
          attemptUpload();
        } else if ((uint32_t)(now - lastEspRetry) >= ESP_RETRY_MS) {
          lastEspRetry = now;
          Serial.println(F("[wifi] retrying join"));
          if (espJoin()) attemptUpload();
        }
      }
#endif
      break;

    case ST_ARRIVED:
      /* Hold station. The handover read-out is DIP mode 11 or console 'd'. */
      if ((uint32_t)(now - lastHeartbeat) >= heartbeatPeriod() * 4) {
        lastHeartbeat = now;
        logRow(0, false);
      }
      break;

    default:
      break;
  }

  updateLed();
}
