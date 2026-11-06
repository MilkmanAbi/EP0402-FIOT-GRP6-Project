/*
 * Persist.h — the parts of the evidence that must survive a power cut.
 *
 * A dropped parcel and a disconnected battery look identical from the
 * outside, and a tracker whose event counter resets to zero every time
 * the power blinks is not evidence of anything. The cumulative count,
 * the session number and the shipment id therefore live in EEPROM.
 *
 * EEPROM is good for roughly 100,000 writes per cell, so we only write
 * when a value actually changes. Mishandling events are rare; this is
 * nowhere near the endurance limit.
 */
#ifndef PG_PERSIST_H
#define PG_PERSIST_H

#include <Arduino.h>

struct PersistData {
  uint16_t magic;        /* PERSIST_MAGIC when the record is valid */
  uint16_t bootCount;
  uint32_t eventCount;   /* cumulative mishandling events          */
  uint16_t shipmentId;
  uint16_t generation;  /* alternating EEPROM journal sequence */
  uint16_t checksum;
};

/* Load the record, or initialise a fresh one if EEPROM is blank or
 * corrupt. Increments and stores the boot counter. */
void persistBegin(uint16_t defaultShipmentId);

uint16_t persistBootCount();
uint16_t persistShipmentId();
uint32_t persistEventCount();

/* Writes only if the value differs from what is already stored. */
void persistSetEventCount(uint32_t n);

/* Zero the counters — console command only, never automatic. */
void persistReset(uint16_t shipmentId);

#endif /* PG_PERSIST_H */
