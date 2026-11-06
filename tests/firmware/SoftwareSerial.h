#pragma once
#include "Arduino.h"
class SoftwareSerial:public Print {
public:
  std::string input,output;size_t cursor=0;bool listening=false;
  SoftwareSerial(int,int){}
  void begin(unsigned long){}
  bool isListening(){return listening;}
  void listen(){listening=true;}
  int available(){return (int)(input.size()-cursor);}
  int read(){return available()?(uint8_t)input[cursor++]:-1;}
  size_t write(uint8_t c)override{output+=(char)c;return 1;}
  void feed(const std::string &s){input=s;cursor=0;}
};
