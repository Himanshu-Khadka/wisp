#pragma once
#include <stdint.h>
struct FocusTimer {
  uint32_t remaining = 25*60, last = 0; bool running = false, rest = false;
  void toggle(uint32_t now) { running = !running; last = now; }
  void reset(uint32_t now) { running=false; rest=false; remaining=25*60; last=now; }
  bool tick(uint32_t now) {
    if (!running) return false;
    uint32_t elapsed=(now-last)/1000; if (!elapsed) return false;
    last += elapsed*1000;
    if (elapsed >= remaining) { remaining=rest?25*60:5*60; rest=!rest; running=false; return true; }
    remaining-=elapsed; return false;
  }
};
inline uint8_t sensorCRC(const uint8_t *data, unsigned n) {
 uint8_t crc=255; for(unsigned i=0;i<n;i++){crc^=data[i];for(int j=0;j<8;j++)crc=(crc&128)?(crc<<1)^0x31:crc<<1;} return crc;
}
