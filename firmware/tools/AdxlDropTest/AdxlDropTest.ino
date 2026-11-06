/*
 * AdxlDropTest — find the real impact thresholds for your shield.
 *
 * The main firmware ships with IMPACT_ROUGH_G = 0.40 and
 * IMPACT_SEVERE_G = 1.20. Those are starting points, not measurements.
 * This sketch is how you turn them into measurements, which is the
 * difference between "we picked some numbers" and "we calibrated it"
 * when someone asks in the Q&A.
 *
 * METHOD
 *   1. Upload this, open Serial Monitor at 9600.
 *   2. Let it calibrate — keep the board perfectly still for two seconds.
 *   3. Put the board in the parcel you will actually demo with.
 *   4. Drop the parcel onto a folded towel from 10 cm. Note PEAK.
 *      Repeat five times, take the median.
 *   5. Repeat at 30 cm and at 60 cm.
 *   6. Also record: carrying it around, setting it down normally, and a
 *      firm shake. Those must all stay BELOW your rough threshold or you
 *      will log false positives all day.
 *   7. Set ROUGH just above the "normal handling" peaks, and SEVERE just
 *      below your 30 cm median.
 *
 * Send 'r' to reset the peak, 'c' to recalibrate.
 *
 * Record your numbers in docs/CALIBRATION.md.
 */

#include <util/twi.h>
#include <math.h>

#define TWI_FREQ_HZ  400000UL
#define TWI_WAIT_MAX 3000

static uint8_t addr = 0;
static float baseline = 1.0f;
static float peak = 0.0f;
static uint32_t lastPrint = 0;

static bool twiWait() {
  uint16_t g = TWI_WAIT_MAX;
  while (!(TWCR & _BV(TWINT))) if (--g == 0) return false;
  return true;
}
static void twiStop() {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
  uint16_t g = TWI_WAIT_MAX;
  while ((TWCR & _BV(TWSTO)) && --g) {}
}
static bool twiStart(uint8_t a, bool rd) {
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
  if (!twiWait()) return false;
  uint8_t st = TW_STATUS;
  if (st != TW_START && st != TW_REP_START) { twiStop(); return false; }
  TWDR = (uint8_t)((a << 1) | (rd ? 1 : 0));
  TWCR = _BV(TWINT) | _BV(TWEN);
  if (!twiWait()) return false;
  st = TW_STATUS;
  if (rd ? (st != TW_MR_SLA_ACK) : (st != TW_MT_SLA_ACK)) { twiStop(); return false; }
  return true;
}
static bool twiWr(uint8_t b) {
  TWDR = b; TWCR = _BV(TWINT) | _BV(TWEN);
  if (!twiWait()) return false;
  if (TW_STATUS != TW_MT_DATA_ACK) { twiStop(); return false; }
  return true;
}
static bool twiRd(uint8_t *o, bool ack) {
  TWCR = ack ? (_BV(TWINT) | _BV(TWEN) | _BV(TWEA)) : (_BV(TWINT) | _BV(TWEN));
  if (!twiWait()) return false;
  uint8_t st = TW_STATUS;
  if (ack ? (st != TW_MR_DATA_ACK) : (st != TW_MR_DATA_NACK)) { twiStop(); return false; }
  *o = TWDR; return true;
}
static bool regWrite(uint8_t r, uint8_t v) {
  if (!twiStart(addr, false)) return false;
  if (!twiWr(r)) return false;
  if (!twiWr(v)) return false;
  twiStop(); return true;
}
static bool regRead(uint8_t r, uint8_t *b, uint8_t n) {
  if (!twiStart(addr, false)) return false;
  if (!twiWr(r)) return false;
  if (!twiStart(addr, true)) return false;
  for (uint8_t i = 0; i < n; i++) if (!twiRd(&b[i], i < n - 1)) return false;
  twiStop(); return true;
}

static bool readMag(float &m) {
  uint8_t raw[6];
  if (!regRead(0x32, raw, 6)) return false;
  int16_t x = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
  int16_t y = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
  int16_t z = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);
  float gx = x / 256.0f, gy = y / 256.0f, gz = z / 256.0f;
  m = sqrt(gx * gx + gy * gy + gz * gz);
  return true;
}

static void calibrate() {
  Serial.println(F("calibrating - hold still"));
  float sum = 0; uint16_t n = 0;
  for (uint16_t i = 0; i < 200; i++) {
    float m;
    if (readMag(m)) { sum += m; n++; }
    delay(10);
  }
  if (n) baseline = sum / n;
  peak = 0;
  Serial.print(F("baseline = ")); Serial.print(baseline, 4);
  Serial.println(F(" g"));
  Serial.println(F("ready - drop it. 'r' resets peak, 'c' recalibrates."));
}

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println(F("\nADXL345 drop-test / threshold calibration"));

  PORTC |= _BV(PC4) | _BV(PC5);
  TWSR = 0;
  TWBR = (uint8_t)(((F_CPU / TWI_FREQ_HZ) - 16) / 2);
  TWCR = _BV(TWEN);

  uint8_t id = 0;
  addr = 0x53;
  if (!(regRead(0x00, &id, 1) && id == 0xE5)) {
    addr = 0x1D;
    if (!(regRead(0x00, &id, 1) && id == 0xE5)) {
      Serial.println(F("ADXL345 NOT FOUND on 0x53 or 0x1D."));
      Serial.println(F("Check the shield is seated and A4/A5 are clear."));
      addr = 0;
      return;
    }
  }
  Serial.print(F("found at 0x")); Serial.println(addr, HEX);

  regWrite(0x31, 0x0B);   /* full resolution, +/-16 g */
  regWrite(0x2C, 0x0A);   /* 100 Hz */
  regWrite(0x2D, 0x08);   /* measure */
  delay(20);
  calibrate();
}

void loop() {
  if (!addr) { delay(1000); return; }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') { peak = 0; Serial.println(F("peak reset")); }
    if (c == 'c') calibrate();
  }

  float m;
  if (readMag(m)) {
    float d = fabs(m - baseline);
    if (d > peak) {
      peak = d;
      Serial.print(F("** NEW PEAK  "));
      Serial.print(peak, 3);
      Serial.print(F(" g   -> tier: "));
      if      (peak >= 1.20f) Serial.println(F("SEVERE"));
      else if (peak >= 0.40f) Serial.println(F("rough"));
      else                    Serial.println(F("normal (ignored)"));
    }
  }

  /* Steady trickle so you can see it is alive between drops. */
  if (millis() - lastPrint > 2000) {
    lastPrint = millis();
    Serial.print(F("live=")); Serial.print(fabs(m - baseline), 3);
    Serial.print(F("g  peak=")); Serial.print(peak, 3);
    Serial.println(F("g"));
  }
  delay(5);
}
