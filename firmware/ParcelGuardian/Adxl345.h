/*
 * Adxl345.h — minimal from-scratch driver for the ADXL345 3-axis accelerometer.
 *
 * Written against the register map rather than pulling in a vendor library:
 * we only need four registers, and a hand-rolled driver costs ~40 bytes of
 * SRAM instead of several hundred. On a 2 KB part that matters.
 *
 * The chip answers on 0x53 or 0x1D depending on how the SDO pin is strapped,
 * so begin() probes both and verifies the DEVID register reads 0xE5.
 */
#ifndef PG_ADXL345_H
#define PG_ADXL345_H

#include <Arduino.h>

class Adxl345 {
public:
  Adxl345();

  /* Probe both addresses, verify DEVID, configure and start measuring.
   * Returns false if no ADXL345 answered. */
  bool begin();

  /* Read one sample in g. Returns false on an I2C fault. */
  bool read(float &gx, float &gy, float &gz);

  /* Vector magnitude in g — orientation-independent, which is the whole
   * reason we use it instead of per-axis thresholds. ~1.0 at rest. */
  bool readMagnitude(float &mag);

  uint8_t address() const { return addr_; }
  bool    present() const { return addr_ != 0; }

private:
  uint8_t addr_;

  bool write8(uint8_t reg, uint8_t value);
  bool readBytes(uint8_t reg, uint8_t *buf, uint8_t len);
  bool probe(uint8_t candidate);
};

#endif /* PG_ADXL345_H */
