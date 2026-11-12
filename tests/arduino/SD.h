#pragma once
#include "Arduino.h"

constexpr uint8_t OUTPUT = 1;
inline void pinMode(uint8_t, uint8_t) {}

constexpr uint8_t SPI_HALF_SPEED = 1;
constexpr uint8_t O_READ = 1, O_WRITE = 2, O_CREAT = 4, O_APPEND = 8;

inline bool mockCardPresent = false;
inline bool mockCloseFails = false;
inline bool mockWriteFails = false;

class Sd2Card {
public:
  bool init(uint8_t, uint8_t) { return mockCardPresent; }
};

class SdVolume {
public:
  static void cacheClear() {}
  bool init(Sd2Card *) { return true; }
};

class SdFile : public Print {
public:
  bool isOpen() const { return open_; }
  bool openRoot(SdVolume *) { return open_ = true; }
  bool open(SdFile *, const char *, uint8_t) { return open_ = true; }
  uint32_t fileSize() const { return 0; }
  int read() { return -1; }
  size_t write(uint8_t) override { return mockWriteFails ? 0 : 1; }
  int getWriteError() const { return mockWriteFails; }
  bool close() {
    open_ = false;
    return !mockCloseFails;
  }
  static bool remove(SdFile *, const char *) { return true; }
private:
  bool open_ = false;
};
