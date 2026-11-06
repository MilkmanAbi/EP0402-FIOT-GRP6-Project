#include "Adxl345.h"
#include <math.h>
#include <util/twi.h>

/*
 * A bare-metal TWI (hardware I2C) driver sits underneath this class
 * instead of the Wire library.
 *
 * WHY: Wire costs roughly 1.3 KB of flash and 128 bytes of SRAM for its
 * two ring buffers, and this project needs exactly three transaction
 * shapes — write a register, write a register plus a value, and read a
 * block. At 101% of flash, that trade was no longer optional.
 *
 * Every wait loop is bounded. A stuck SDA line on a half-seated shield
 * would otherwise hang the whole tracker inside an I2C read, and a
 * tracker that stops logging because a wire is loose is worse than one
 * that reports a sensor fault and carries on.
 */

#define TWI_FREQ_HZ   400000UL    /* the ADXL345 is happy at 400 kHz */
#define TWI_WAIT_MAX  3000        /* loop iterations, not microseconds */

static bool twiWait() {
  uint16_t guard = TWI_WAIT_MAX;
  while (!(TWCR & _BV(TWINT))) {
    if (--guard == 0) return false;
  }
  return true;
}

static void twiStop() {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
  uint16_t guard = TWI_WAIT_MAX;
  while ((TWCR & _BV(TWSTO)) && --guard) { /* spin until released */ }
}

static void twiInit() {
  /* Internal pull-ups. The shield should have its own, but enabling
   * these costs nothing and rescues a board that does not. */
  PORTC |= _BV(PC4) | _BV(PC5);     /* A4 = SDA, A5 = SCL */

  TWSR = 0;                          /* prescaler 1 */
  TWBR = (uint8_t)(((F_CPU / TWI_FREQ_HZ) - 16) / 2);
  TWCR = _BV(TWEN);
}

static bool twiStart(uint8_t addr, bool read) {
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
  if (!twiWait()) return false;

  uint8_t st = TW_STATUS;
  if (st != TW_START && st != TW_REP_START) { twiStop(); return false; }

  TWDR = (uint8_t)((addr << 1) | (read ? 1 : 0));
  TWCR = _BV(TWINT) | _BV(TWEN);
  if (!twiWait()) return false;

  st = TW_STATUS;
  if (read ? (st != TW_MR_SLA_ACK) : (st != TW_MT_SLA_ACK)) { twiStop(); return false; }
  return true;
}

static bool twiWriteByte(uint8_t b) {
  TWDR = b;
  TWCR = _BV(TWINT) | _BV(TWEN);
  if (!twiWait()) return false;
  if (TW_STATUS != TW_MT_DATA_ACK) { twiStop(); return false; }
  return true;
}

static bool twiReadByte(uint8_t *out, bool ack) {
  TWCR = ack ? (_BV(TWINT) | _BV(TWEN) | _BV(TWEA))
             : (_BV(TWINT) | _BV(TWEN));
  if (!twiWait()) return false;

  uint8_t st = TW_STATUS;
  if (ack ? (st != TW_MR_DATA_ACK) : (st != TW_MR_DATA_NACK)) { twiStop(); return false; }

  *out = TWDR;
  return true;
}

/* ---- ADXL345 register map (datasheet Rev. E) ---- */
#define REG_DEVID        0x00
#define REG_BW_RATE      0x2C
#define REG_POWER_CTL    0x2D
#define REG_DATA_FORMAT  0x31
#define REG_DATAX0       0x32

#define DEVID_EXPECTED   0xE5
#define BW_RATE_100HZ    0x0A   /* 100 Hz output data rate        */
#define POWER_CTL_MEASURE 0x08  /* leave standby, start measuring */
#define DATA_FORMAT_FULLRES_16G 0x0B

/* In FULL_RES mode the scale factor is a fixed 3.9 mg/LSB at every range,
 * so one LSB is 1/256 g whatever the range bits say. */
#define LSB_PER_G        256.0f

Adxl345::Adxl345() : addr_(0) {}

bool Adxl345::write8(uint8_t reg, uint8_t value) {
  if (!twiStart(addr_, false)) return false;
  if (!twiWriteByte(reg))      return false;
  if (!twiWriteByte(value))    return false;
  twiStop();
  return true;
}

bool Adxl345::readBytes(uint8_t reg, uint8_t *buf, uint8_t len) {
  if (!twiStart(addr_, false)) return false;
  if (!twiWriteByte(reg))      return false;

  /* Repeated start, then read len bytes, NACKing the last one. */
  if (!twiStart(addr_, true))  return false;
  for (uint8_t i = 0; i < len; i++) {
    if (!twiReadByte(&buf[i], i < (uint8_t)(len - 1))) return false;
  }
  twiStop();
  return true;
}

bool Adxl345::probe(uint8_t candidate) {
  uint8_t saved = addr_;
  addr_ = candidate;

  uint8_t id = 0;
  bool ok = readBytes(REG_DEVID, &id, 1) && (id == DEVID_EXPECTED);

  if (!ok) addr_ = saved;
  return ok;
}

bool Adxl345::begin() {
  twiInit();
  addr_ = 0;

  /* SDO low -> 0x53, SDO high -> 0x1D. Try the common one first. */
  if      (probe(0x53)) addr_ = 0x53;
  else if (probe(0x1D)) addr_ = 0x1D;
  else { addr_ = 0; return false; }

  /* Full resolution at +/-16 g: nothing clips on a hard drop, and the
   * sensitivity stays constant so the g maths needs no range handling. */
  if (!write8(REG_DATA_FORMAT, DATA_FORMAT_FULLRES_16G)) return false;
  if (!write8(REG_BW_RATE,     BW_RATE_100HZ))           return false;
  if (!write8(REG_POWER_CTL,   POWER_CTL_MEASURE))       return false;

  delay(10);   /* let the first conversion land */
  return true;
}

bool Adxl345::read(float &gx, float &gy, float &gz) {
  if (!addr_) return false;

  uint8_t raw[6];
  if (!readBytes(REG_DATAX0, raw, 6)) return false;

  /* Little-endian, two's complement, sign-extended into int16_t. */
  int16_t x = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
  int16_t y = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
  int16_t z = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);

  gx = x / LSB_PER_G;
  gy = y / LSB_PER_G;
  gz = z / LSB_PER_G;
  return true;
}

bool Adxl345::readMagnitude(float &mag) {
  float x, y, z;
  if (!read(x, y, z)) return false;
  mag = sqrt(x * x + y * y + z * z);
  return true;
}
