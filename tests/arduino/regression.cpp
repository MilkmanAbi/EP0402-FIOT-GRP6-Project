#include <stdio.h>
#include <assert.h>
#include <string>
#include "Arduino.h"
uint32_t fakeClock=100;
void pgServiceUplink(){fakeClock++;}
#include "../../arduino/ParcelGuardian/Persist.cpp"
#include "../../arduino/ParcelGuardian/GpsModule.cpp"
#include "../../arduino/ParcelGuardian/Fmt.cpp"
#include "../../arduino/ParcelGuardian/EspClient.cpp"

static std::string nmea(std::string s){
  unsigned char sum=0;for(char c:s)sum^=c;
  char tail[8];sprintf(tail,"*%02X\r\n",sum);return "$"+s+tail;
}
static void feedGps(const std::string &s){
  gpsPort.feed(nmea(s));while(gpsPort.available())gpsService();
}
static std::string frame(const std::string &s){return "+IPD,"+std::to_string(s.size())+":"+s;}
static bool ack(const std::string &s){port.feed(s);return accepted();}
int main(){
  static_assert(sizeof(PersistData)==14,"Host must emulate AVR struct layout");
  persistBegin(1001);persistSetEventCount(7);
  uint8_t before[1024];memcpy(before,EEPROM.bytes,sizeof(before));
  for(int cut=0;cut<24;cut++) {
    memcpy(EEPROM.bytes,before,sizeof(before));EEPROM.cut=-1;persistBegin(1001);
    EEPROM.cut=cut;try{persistSetEventCount(8);}catch(PowerCut&){}
    EEPROM.cut=-1;persistBegin(1001);
    assert(persistEventCount()==7 || persistEventCount()==8);
    assert(persistShipmentId()==1001);
  }
  persistSetEventCount(123);persistBegin(2002);
  assert(persistShipmentId()==2002 && persistEventCount()==0);
  data_.generation=65534;store();persistSetEventCount(9);persistBegin(2002);
  assert(persistEventCount()==9);
  // A valid original single-record EEPROM image migrates without losing counts.
  memset(EEPROM.bytes,255,sizeof(EEPROM.bytes));
  struct LegacyTest{uint16_t magic,boot;uint32_t events;uint16_t shipment,check;} old={PERSIST_MAGIC,5,42,1001,0};
  old.check=checksum(&old,sizeof(old));EEPROM.put(0,old);persistBegin(1001);
  assert(persistBootCount()==6 && persistEventCount()==42);
  puts("EEPROM: every interrupted-write point, wrap, shipment and legacy migration passed");

  gpsBegin();fakeClock=1000;
  const std::string valid="GPRMC,120000.00,A,0118.0000,N,10348.0000,E,0,0,061126,,,A";
  feedGps(valid);assert(gpsHasFix());
  assert(fabs(gpsLat()-1.3)<0.00001 && fabs(gpsLon()-103.8)<0.00001);
  feedGps("GPRMC,120001.00,V,,,,,,,061126,,,N");assert(!gpsHasFix());
  feedGps(valid);feedGps("GPGGA,120002.00,,,,,0,00,0,0,M,0,M,,");assert(!gpsHasFix());
  for(const auto &bad:{
    "GPRMC,120000.00,A,9160.0000,N,10348.0000,E,0,0,061126,,,A",
    "GPRMC,120000.00,A,0118.0000,Q,10348.0000,E,0,0,061126,,,A",
    "GPRMC,120000.00,A,01x8.0000,N,10348.0000,E,0,0,061126,,,A"}) {
    feedGps(valid);feedGps(bad);assert(!gpsHasFix());
  }
  feedGps(valid);fakeClock+=10000;assert(!gpsHasFix());
  char utc[20];assert(!gpsTimestamp(utc,sizeof(utc)));
  feedGps(valid);assert(gpsTimestamp(utc,sizeof(utc)));assert(!strcmp(utc,"2026-11-06 12:00:00"));
  port.feed("");puts("GPS: valid, void, no-fix, malformed, range and stale time passed");

  const std::string response="HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n12\n";
  assert(ack("\r\nSEND OK\r\n"+frame(response)+"\r\nCLOSED\r\n"));
  for(size_t split=1;split<response.size();split++) {
    assert(ack(frame(response.substr(0,split))+frame(response.substr(split))));
  }
  assert(!ack("SEND OK\r\nCLOSED\r\n123\n"));
  assert(!ack(frame("HTTP/1.1 401 Unauthorized\r\n\r\n123\n")));
  assert(!ack(frame("HTTP/1.1 200 OK\r\n\r\n0\n")));
  assert(!ack(frame("HTTP/1.1 200 OK\r\n\r\n<html>12</html>\n")));
  assert(!ack(frame("HTTP/1.1 200 OK\r\n\r\n12"))); // no LF, incomplete ACK
  assert(!ack(frame("HTTP/1.1 500 Error\r\n\r\n12\n")));
  puts("ESP: every IPD split, HTTP status, rejection, HTML and truncated ACK passed");
  return 0;
}
