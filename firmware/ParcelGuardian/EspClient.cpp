#include "EspClient.h"
#include "Config.h"
#include "Secrets.h"
#include "GpsModule.h"
#include "Fmt.h"
#include "Persist.h"
#include <SoftwareSerial.h>
#include <avr/pgmspace.h>
#if ENABLE_ESP
#if ENABLE_WATCHDOG
#include <avr/wdt.h>
#endif

extern void pgServiceUplink();
static SoftwareSerial port(PIN_ESP_RX, PIN_ESP_TX);
static EspState state = ESP_DOWN;
static uint16_t okCount=0, failCount=0;
static uint32_t lastOk=0;

#if UPLINK_PHONE
static const char HOST[] PROGMEM = PHONE_SERVER_HOST;
static const char KEY[] PROGMEM = PHONE_API_KEY;
#define SERVER_PORT PHONE_SERVER_PORT
static_assert(sizeof(PHONE_API_KEY)>=17 && sizeof(PHONE_API_KEY)<=33,
              "Phone key must be 16..32 URL-safe characters");
#else
static const char HOST[] PROGMEM = THINGSPEAK_HOST;
static const char KEY[] PROGMEM = THINGSPEAK_WRITE_KEY;
#define SERVER_PORT THINGSPEAK_PORT
#endif
static const char H1[] PROGMEM=" HTTP/1.1\r\nHost: ";
static const char H2[] PROGMEM="\r\nConnection: close\r\n\r\n";

static void service() {
#if ENABLE_WATCHDOG
  wdt_reset();
#endif
  pgServiceUplink();
}
static void printP(PGM_P s) {
  char c;
  while ((c=pgm_read_byte(s++))) { port.write(c); service(); }
}
static void flushInput() { uint8_t n=120; while(port.available() && n--) port.read(); }

// Match suffixes directly in flash; no large AT response buffers.
static uint8_t waitFor(PGM_P a, PGM_P b, uint32_t timeout) {
  char win[17]={0};
  uint32_t start=millis();
  while ((uint32_t)(millis()-start)<timeout) {
    service();
    if (!port.available()) continue;
    memmove(win,win+1,15); win[15]=(char)port.read();
    uint8_t n=strlen_P(a);
    if (!strncmp_P(win+16-n,a,n)) return 1;
    if (b) { n=strlen_P(b); if(!strncmp_P(win+16-n,b,n)) return 2; }
  }
  return 0;
}
static uint8_t cmd(PGM_P s,uint32_t timeout=ESP_CMD_TIMEOUT) {
  flushInput(); printP(s); port.print(F("\r\n"));
  return waitFor(PSTR("\r\nOK\r\n"),PSTR("ERROR"),timeout);
}
void espListen() { if(!port.isListening()) port.listen(); }
bool espBegin() {
  port.begin(BAUD_ESP); espListen();
  uint8_t r=0;
  for(uint8_t i=0;i<3 && r!=1;i++) r=cmd(PSTR("AT"));
  if(r!=1) { state=ESP_DOWN; gpsListen(); return false; }
  cmd(PSTR("ATE0")); // Echo otherwise makes response parsing ambiguous.
  bool configured=cmd(PSTR("AT+CWMODE=1"))==1 && cmd(PSTR("AT+CIPMUX=0"))==1;
  state=configured?ESP_IDLE:ESP_DOWN;
  gpsListen(); return configured;
}

// Quote AT parameters, including SSIDs/passwords containing comma or quote.
static void credential(PGM_P s) {
  char c;
  while((c=pgm_read_byte(s++))) {
    if(c=='"' || c=='\\' || c==',') port.write('\\');
    if(c!='\r' && c!='\n') port.write(c);
    service();
  }
}
bool espJoin() {
  if(state==ESP_DOWN && !espBegin()) return false;
  espListen();flushInput();
  port.print(F("AT+CWJAP=\""));credential(PSTR(WIFI_SSID));
  port.print(F("\",\""));credential(PSTR(WIFI_PASSWORD));port.print(F("\"\r\n"));
  state=waitFor(PSTR("\r\nOK\r\n"),PSTR("FAIL"),ESP_JOIN_TIMEOUT)==1?ESP_JOINED:ESP_IDLE;
  gpsListen();return state==ESP_JOINED;
}

// Stream the query twice: first count bytes, then transmit. No query buffer
// or unbounded string builder can overrun the Uno's 2 KB RAM.
class Counter : public Print {
public: uint16_t count=0;
  size_t write(uint8_t) override { count++;return 1; }
};
static void fixed(Print &out,float value,uint8_t dp) {
  char b[20];fmtFixed(b,value,dp);out.print(b);service();
}
static void keyTo(Print &out) {
  char c;PGM_P s=KEY;
  while((c=pgm_read_byte(s++))) out.write(c);
}
static void query(Print &out,bool fix,double lat,double lon,float peak,
                  uint8_t status,uint32_t events,uint32_t stamp) {
  out.print(F("/update?api_key="));keyTo(out);service();
  out.print(F("&field1="));if(fix) fixed(out,lat,6);
  out.print(F("&field2="));if(fix) fixed(out,lon,6);
  out.print(F("&field3="));fixed(out,peak,3);
  out.print(F("&field4="));out.print(status);service();
  out.print(F("&field5="));out.print(events);service();
#if UPLINK_PHONE
  out.print(F("&id="));out.print(SHIPMENT_ID);
  out.print(F("&boot="));out.print(persistBootCount());
  out.print(F("&ms="));out.print(stamp);service();
#else
  (void)stamp;
#endif
}

// Decode +IPD,<length>: frames before interpreting HTTP. Frames can split
// anywhere, including between the HTTP status, headers and numeric body.
class Ipd {
  uint8_t prefix=0;
  uint16_t remaining=0, length=0;
  bool size=false;
public:
  int feed(char c) {
    if(remaining) { remaining--;return (uint8_t)c; }
    if(size) {
      if(c>='0' && c<='9') {
        if(length>6000) {size=false;length=0;} else length=length*10+c-'0';
      } else if(c==':' && length) {remaining=length;size=false;length=0;}
      else {size=false;length=0;}
      return -1;
    }
    if(c==pgm_read_byte(PSTR("+IPD,")+prefix)) {
      if(++prefix==5) {prefix=0;size=true;length=0;}
    } else prefix=(c=='+')?1:0;
    return -1;
  }
};
static bool accepted() {
  Ipd ipd;
  uint8_t statusPos=0, blank=0;
  bool statusOk=true,body=false,digit=false,nonzero=false;
  uint32_t start=millis();
  while((uint32_t)(millis()-start)<ESP_TCP_TIMEOUT) {
    service();if(!port.available())continue;
    int value=ipd.feed((char)port.read());if(value<0)continue;
    char c=(char)value;
    if(statusPos<12) {
      char expected=pgm_read_byte(PSTR("HTTP/1.1 200")+statusPos);
      if(statusPos==7) {if(c!='0' && c!='1')statusOk=false;}
      else if(c!=expected)statusOk=false;
      statusPos++;continue;
    }
    if(!body) {
      if(c==pgm_read_byte(PSTR("\r\n\r\n")+blank))blank++;
      else blank=(c=='\r')?1:0;
      if(blank==4) {if(!statusOk)return false;body=true;}
    } else {
      if(c>='0' && c<='9') {digit=true;if(c!='0')nonzero=true;}
      else if((c=='\r' || c=='\n') && digit)return nonzero;
      else return false; // Reject HTML, errors, chunks, and partial bodies.
    }
  }
  // ThingSpeak's numeric body may omit trailing CRLF; require a complete
  // IPD frame and a numeric nonzero body. Phone replies always include LF.
#if UPLINK_PHONE
  return false;
#else
  return body && digit && nonzero;
#endif
}

bool espUpload(bool fix,double lat,double lon,float peak,uint8_t status,uint32_t events) {
  if(state!=ESP_JOINED) {failCount++;return false;}
  espListen();
  uint32_t stamp=millis();
  Counter counter;query(counter,fix,lat,lon,peak,status,events,stamp);
  uint16_t len=4+counter.count+strlen_P(H1)+strlen_P(HOST)+strlen_P(H2);
  flushInput();port.print(F("AT+CIPSTART=\"TCP\",\""));printP(HOST);
  port.print(F("\","));port.print(SERVER_PORT);port.print(F("\r\n"));
  bool success=false;
  if(waitFor(PSTR("\r\nOK\r\n"),PSTR("ALREADY"),ESP_TCP_TIMEOUT)) {
    flushInput();port.print(F("AT+CIPSEND="));port.print(len);port.print(F("\r\n"));
    if(waitFor(PSTR(">"),PSTR("ERROR"),ESP_CMD_TIMEOUT)==1) {
      port.print(F("GET "));query(port,fix,lat,lon,peak,status,events,stamp);
      printP(H1);printP(HOST);printP(H2);
      // Parse IPD immediately. Waiting for SEND OK first can consume an
      // early response or overflow the 64-byte SoftwareSerial buffer.
      success=accepted();
    }
  }
  cmd(PSTR("AT+CIPCLOSE"),1000UL);
  if(success) {okCount++;lastOk=millis();}
  else {failCount++;state=ESP_IDLE;} // Forces a bounded rejoin after outage.
  gpsListen();return success;
}
EspState espState(){return state;}
uint16_t espUploadsOk(){return okCount;}
uint16_t espUploadsFailed(){return failCount;}
uint32_t espLastOkMs(){return lastOk;}
#endif
