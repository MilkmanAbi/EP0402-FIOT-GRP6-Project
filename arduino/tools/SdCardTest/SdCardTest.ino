/*
 * SdCardTest — prove the microSD slot works in isolation.
 *
 * Test this on its own BEFORE combining it with GPS and WiFi. The SD
 * library is the single largest consumer of both flash and SRAM in this
 * project, and SPI writes block long enough to cost you GPS characters.
 * Knowing the card itself is sound removes one variable from that mess.
 *
 * Serial Monitor at 9600. A pass looks like:
 *
 *   SD.begin ok
 *   wrote 3 rows
 *   ---- file contents ----
 *   test,1,<millis>
 *   test,2,<millis>
 *   test,3,<millis>
 *   -----------------------
 *   PASS
 *
 * IF SD.begin FAILS
 *   - The card must be FAT16 or FAT32. Format it; exFAT will not work,
 *     and most cards over 32 GB ship as exFAT.
 *   - CS is D10 on this shield. If you changed it, change it below too.
 *   - D13 is the SPI clock AND the Uno's built-in LED. You cannot use
 *     the on-board LED as an indicator while the card is active.
 *   - Reseat the card. Partially inserted cards enumerate intermittently.
 */

#include <SPI.h>
#include <SD.h>

#define SD_CS       10
#define TEST_FILE   "PGTEST.CSV"

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println();
  Serial.println(F("microSD test - CS on D10"));

  pinMode(SD_CS, OUTPUT);

  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD.begin FAILED"));
    Serial.println(F("-> format as FAT32, reseat the card, check CS=D10"));
    return;
  }
  Serial.println(F("SD.begin ok"));

  /* Start clean so repeated runs are comparable. */
  if (SD.exists(TEST_FILE)) SD.remove(TEST_FILE);

  uint8_t written = 0;
  for (uint8_t i = 1; i <= 3; i++) {
    File f = SD.open(TEST_FILE, FILE_WRITE);
    if (!f) { Serial.print(F("open failed on row ")); Serial.println(i); break; }
    f.print(F("test,"));
    f.print(i);
    f.print(',');
    f.println(millis());
    f.close();                 /* close per row, like the real firmware */
    written++;
  }
  Serial.print(F("wrote ")); Serial.print(written); Serial.println(F(" rows"));

  File f = SD.open(TEST_FILE, FILE_READ);
  if (!f) { Serial.println(F("reopen for read FAILED")); return; }

  Serial.println(F("---- file contents ----"));
  while (f.available()) Serial.write(f.read());
  f.close();
  Serial.println(F("-----------------------"));

  Serial.println(written == 3 ? F("PASS") : F("FAIL"));
}

void loop() {}
