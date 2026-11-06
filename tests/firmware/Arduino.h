#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#define F(x) x
#define PSTR(x) x
#define PROGMEM
typedef const char *PGM_P;
#define pgm_read_byte(p) (*(const char *)(p))
#define strcpy_P strcpy
#define strlen_P strlen
#define strncmp_P strncmp
extern uint32_t fakeClock;
inline uint32_t millis(){return fakeClock;}
inline void delay(uint32_t n){fakeClock+=n;}
inline char *ultoa(unsigned long n,char *s,int){sprintf(s,"%lu",n);return s;}
class Print {
public:
  virtual ~Print()=default;
  virtual size_t write(uint8_t)=0;
  size_t print(const char *s){size_t n=0;while(*s)n+=write(*s++);return n;}
  size_t print(char *s){return print((const char *)s);}
  template<class T>size_t print(T n){return print(std::to_string(n).c_str());}
};
#pragma pack(1)
