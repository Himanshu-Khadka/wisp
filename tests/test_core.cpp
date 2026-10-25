#include <cassert>
#include <cstdio>
#include "../firmware/Wisp/core.h"
int main(){
 FocusTimer f; f.toggle(100);assert(!f.tick(60100));assert(f.remaining==1440);
 f.toggle(60100);assert(!f.tick(600100));assert(f.remaining==1440);
 f.toggle(600100);assert(f.tick(2040100));assert(f.rest&&!f.running&&f.remaining==300);
 f.toggle(2040100);assert(f.tick(2340100));assert(!f.rest&&f.remaining==1500&&!f.running);
 f.reset(0);f.toggle(UINT32_MAX-499);f.tick(500);assert(f.remaining==1499);
 f.reset(0);f.toggle(0);f.tick(3500);assert(f.remaining==1497);f.tick(4000);assert(f.remaining==1496);
 uint8_t bytes[]={0xbe,0xef};assert(sensorCRC(bytes,2)==0x92);
 puts("PASS: elapsed time, pause, focus/break transitions, millis rollover, fractional seconds, sensor CRC");
}
