#include "Persist.h"
#include "Config.h"
#include <EEPROM.h>

static PersistData data_;

/* Additive checksum over every byte except the checksum field itself.
 * Not cryptographic — it only has to catch a half-written record or a
 * blank chip, and for that it is entirely adequate. */
static uint16_t computeChecksum(const PersistData &d) {
  const uint8_t *p = (const uint8_t *)&d;
  uint16_t sum = 0;
  for (uint8_t i = 0; i < sizeof(PersistData) - sizeof(uint16_t); i++) {
    sum += p[i];
    sum = (uint16_t)((sum << 1) | (sum >> 15));   /* rotate, so order matters */
  }
  return sum;
}

static void store() {
  data_.checksum = computeChecksum(data_);
  EEPROM.put(PERSIST_ADDR, data_);
}

void persistBegin(uint16_t defaultShipmentId) {
  EEPROM.get(PERSIST_ADDR, data_);

  bool valid = (data_.magic == PERSIST_MAGIC) &&
               (data_.checksum == computeChecksum(data_));

  if (!valid) {
    data_.magic      = PERSIST_MAGIC;
    data_.bootCount  = 0;
    data_.eventCount = 0;
    data_.shipmentId = defaultShipmentId;
  }

  data_.bootCount++;
  store();
}

uint16_t persistBootCount()  { return data_.bootCount; }
uint16_t persistShipmentId() { return data_.shipmentId; }
uint32_t persistEventCount() { return data_.eventCount; }

void persistSetEventCount(uint32_t n) {
  if (n == data_.eventCount) return;   /* spare the cell */
  data_.eventCount = n;
  store();
}

void persistReset(uint16_t shipmentId) {
  data_.magic      = PERSIST_MAGIC;
  data_.bootCount  = 1;
  data_.eventCount = 0;
  data_.shipmentId = shipmentId;
  store();
}
