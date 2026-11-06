#pragma once
#include "Arduino.h"
#include <stdexcept>
struct PowerCut {};
struct MockEEPROM {
  uint8_t bytes[1024];int cut=-1;
  MockEEPROM(){memset(bytes,255,sizeof(bytes));}
  void update(int addr,uint8_t value){
    if(bytes[addr]==value)return;
    if(cut==0)throw PowerCut{};
    if(cut>0)cut--;
    bytes[addr]=value;
  }
  template<class T>void get(int addr,T &out){memcpy(&out,bytes+addr,sizeof(out));}
  template<class T>void put(int addr,const T &in){memcpy(bytes+addr,&in,sizeof(in));}
};
inline MockEEPROM EEPROM;
