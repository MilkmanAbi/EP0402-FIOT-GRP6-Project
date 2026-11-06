/*
 * EspAtTest — serial passthrough to the ESP-01.
 *
 * RUN THIS BEFORE THE MAIN FIRMWARE. If the ESP-01 does not answer "AT"
 * with "OK" here, it will not work there either, and you will waste an
 * afternoon debugging the wrong layer.
 *
 * Serial Monitor: 9600 baud, line ending "Both NL & CR".
 *
 * Commands worth trying, in order:
 *   AT            -> OK                 (the module is alive)
 *   AT+GMR        -> firmware version   (tells you which AT dialect)
 *   AT+CWMODE=1   -> OK                 (station mode)
 *   AT+CWLAP      -> list of networks   (the radio actually works)
 *   AT+CWJAP="ssid","password"  -> OK   (it can join YOUR hotspot)
 *   AT+CIFSR      -> your IP address    (DHCP gave you a lease)
 *
 * IF "AT" RETURNS NOTHING OR GARBAGE
 *   The module is almost certainly at 115200, not 9600. SoftwareSerial on
 *   an Uno is not reliable at 115200, so the main firmware needs it moved
 *   down. Change BAUD below to 115200, re-upload, confirm AT works, then:
 *
 *       AT+UART_DEF=9600,8,1,0,0
 *
 *   ...which saves 9600 permanently. Set BAUD back to 9600 and confirm.
 *   Older firmware calls it AT+CIOBAUD instead. If neither exists, ask
 *   your lecturer before reflashing a module that is not yours.
 *
 * Other things that cause silence: the ESP-01 is a 3.3 V part and draws
 * current in bursts when the radio transmits. Check the shield's regulator
 * before assuming the module is dead.
 */

#include <SoftwareSerial.h>

#define BAUD 9600       /* try 115200 if you get nothing */

/* SoftwareSerial(rxPin, txPin) from the UNO's point of view. */
SoftwareSerial esp(2, 3);

void setup() {
  Serial.begin(9600);
  esp.begin(BAUD);
  delay(100);
  Serial.println();
  Serial.print(F("ESP-01 passthrough at "));
  Serial.print(BAUD);
  Serial.println(F(" baud."));
  Serial.println(F("Set line ending to 'Both NL & CR' and type AT"));
  Serial.println(F("----------------------------------------------"));
}

void loop() {
  while (esp.available())    Serial.write(esp.read());
  while (Serial.available()) esp.write(Serial.read());
}
