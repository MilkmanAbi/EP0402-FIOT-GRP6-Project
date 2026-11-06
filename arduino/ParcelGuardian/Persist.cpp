#include "Persist.h"
#include "Config.h"
#include <EEPROM.h>

static PersistData data_;
static const uint16_t JOURNAL_MAGIC = PERSIST_MAGIC + 1;
static const uint8_t SLOT_A = 16, SLOT_B = 32;

static uint16_t checksum(const void *value, uint8_t size) {
  const uint8_t *p=(const uint8_t *)value;
  uint16_t sum=0;
  for(uint8_t i=0;i<size-2;i++) {
    sum+=p[i];sum=(uint16_t)((sum<<1)|(sum>>15));
  }
  return sum;
}
static bool valid(const PersistData &d) {
  return d.magic==JOURNAL_MAGIC && d.checksum==checksum(&d,sizeof(d));
}
static void store() {
  data_.magic=JOURNAL_MAGIC;
  data_.generation++;
  data_.checksum=checksum(&data_,sizeof(data_));
  uint8_t addr=(data_.generation&1)?SLOT_A:SLOT_B;
  const uint8_t *p=(const uint8_t *)&data_;
  // Invalidate the target, write payload and checksum, commit magic last.
  // The other slot remains valid if power fails during any of these writes.
  EEPROM.update(addr,0);
  for(uint8_t i=1;i<sizeof(data_);i++) EEPROM.update(addr+i,p[i]);
  EEPROM.update(addr,p[0]);
}
void persistBegin(uint16_t shipment) {
  PersistData other;
  EEPROM.get(SLOT_A,data_);EEPROM.get(SLOT_B,other);
  bool a=valid(data_),b=valid(other);
  if(b && (!a || (int16_t)(other.generation-data_.generation)>0))data_=other;
  if(!a && !b) {
    // Read the original single-record layout once, without rewriting it.
    struct Legacy {uint16_t magic,boot;uint32_t events;uint16_t shipment,check;} old;
    EEPROM.get(PERSIST_ADDR,old);
    data_={JOURNAL_MAGIC,0,0,shipment,0,0};
    if(old.magic==PERSIST_MAGIC && old.check==checksum(&old,sizeof(old))) {
      data_.bootCount=old.boot;data_.eventCount=old.events;data_.shipmentId=old.shipment;
    }
  }
  if(data_.shipmentId!=shipment) {data_.shipmentId=shipment;data_.eventCount=0;}
  data_.bootCount++;store();
}
uint16_t persistBootCount(){return data_.bootCount;}
uint16_t persistShipmentId(){return data_.shipmentId;}
uint32_t persistEventCount(){return data_.eventCount;}
void persistSetEventCount(uint32_t n){if(n!=data_.eventCount){data_.eventCount=n;store();}}
void persistReset(uint16_t shipment){data_.shipmentId=shipment;data_.eventCount=0;store();}
