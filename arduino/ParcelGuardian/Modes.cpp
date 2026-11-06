#include "Modes.h"
#include "Config.h"

void modesBegin() {
  pinMode(PIN_DIP_1, INPUT_PULLUP);
  pinMode(PIN_DIP_2, INPUT_PULLUP);
}

uint8_t dipRaw() {
  uint8_t s1 = digitalRead(PIN_DIP_1);
  uint8_t s2 = digitalRead(PIN_DIP_2);

#if DIP_ACTIVE_LOW
  s1 = (s1 == LOW) ? 1 : 0;
  s2 = (s2 == LOW) ? 1 : 0;
#else
  s1 = (s1 == HIGH) ? 1 : 0;
  s2 = (s2 == HIGH) ? 1 : 0;
#endif

  return (uint8_t)((s1 << 1) | s2);
}

RunMode modeRead() {
  switch (dipRaw()) {
    case 1:  return MODE_DEMO;
    case 2:  return MODE_SILENT;
    case 3:  return MODE_READOUT;
    default: return MODE_NORMAL;
  }
}

const __FlashStringHelper *modeName(RunMode m) {
  switch (m) {
    case MODE_DEMO:    return F("DEMO");
    case MODE_SILENT:  return F("SILENT");
    case MODE_READOUT: return F("READOUT");
    default:           return F("NORMAL");
  }
}
