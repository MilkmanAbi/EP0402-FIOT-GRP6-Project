/*
 * GpsRawTest — prove the NEO-6M is talking before you trust any parser.
 *
 * Serial Monitor at 9600. You are looking for lines like:
 *
 *   $GPGGA,064512.00,0122.12345,N,10348.67890,E,1,07,1.2,15.3,M,...
 *   $GPRMC,064512.00,A,0122.12345,N,10348.67890,E,0.05,,061126,,,A*7C
 *
 * READING THE RESULT
 *   Nothing at all        -> wiring or power. The GPS TX must reach D4.
 *                            Swapping D4 and D5 is the usual culprit.
 *   Garbage characters    -> wrong baud. Try 4800 or 38400 below.
 *   Sentences but ",,,,"  -> the receiver works, it just has no fix yet.
 *                            That IS success for this test.
 *   $GPRMC with 'V'       -> void, i.e. still no fix.
 *   $GPRMC with 'A'       -> active, you have a real position.
 *
 * A cold NEO-6M can take several minutes for its first fix and needs
 * actual sky. Indoors next to a window is usually not enough; go outside.
 * The on-board fix LED (if your module has one) blinks once per second
 * when it has locked on.
 *
 * The counter line every 5 seconds tells you characters are flowing even
 * when the sentences themselves look empty.
 */

#include <SoftwareSerial.h>

#define BAUD 9600      /* NEO-6M default; try 4800 or 38400 if garbled */

/* SoftwareSerial(rxPin, txPin) from the UNO's point of view. */
SoftwareSerial gps(4, 5);

unsigned long chars = 0;
unsigned long lastReport = 0;
unsigned int  sentences = 0;

void setup() {
  Serial.begin(9600);
  gps.begin(BAUD);
  delay(100);
  Serial.println();
  Serial.print(F("NEO-6M raw NMEA at "));
  Serial.print(BAUD);
  Serial.println(F(" baud"));
  Serial.println(F("------------------------------------"));
}

void loop() {
  while (gps.available()) {
    char c = gps.read();
    Serial.write(c);
    chars++;
    if (c == '$') sentences++;
  }

  if (millis() - lastReport > 5000) {
    lastReport = millis();
    Serial.println();
    Serial.print(F("[chars="));
    Serial.print(chars);
    Serial.print(F(" sentences="));
    Serial.print(sentences);
    Serial.println(F("]"));
    if (chars == 0) {
      Serial.println(F("[NO DATA - check GPS TX -> Uno D4, and GPS power]"));
    }
  }
}
