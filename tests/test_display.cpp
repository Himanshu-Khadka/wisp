#include "../firmware/Wisp/display_diff.h"
#include <array>
#include <cassert>
#include <iostream>
int main(){
 std::array<uint8_t,48000> oldFrame,next,packed;
 oldFrame.fill(255);next=oldFrame;packed.fill(0);
 assert(!displayDifference(oldFrame.data(),next.data()).changed());
 // Opposite corners exercise exclusive endpoints and full-size packing.
 next[0]=0;next.back()=0;
 auto r=displayDifference(oldFrame.data(),next.data());
 assert(r.x0==0&&r.y0==0&&r.x1==800&&r.y1==480&&r.bytes()==48000);
 packDisplayRegion(next.data(),packed.data(),r);assert(next==packed);
 // Two distant cursor outlines: old pixels must be erased as well as new drawn.
 oldFrame.fill(255);next=oldFrame;oldFrame[85*100+3]=0;next[192*100+35]=0;
 r=displayDifference(oldFrame.data(),next.data());
 assert(r.x0==24&&r.x1==288&&r.y0==85&&r.y1==193);
 packDisplayRegion(next.data(),packed.data(),r);
 for(int y=85;y<193;y++)for(int x=3;x<36;x++)assert(packed[(y-85)*33+x-3]==next[y*100+x]);
 assert(packed[0]==255);assert(packed[r.bytes()-1]==0);
 oldFrame=next;assert(!displayDifference(oldFrame.data(),next.data()).changed());
 assert(!displayCleanupDue(39,20000,0));assert(!displayCleanupDue(40,14999,0));
 assert(displayCleanupDue(40,15000,0));assert(!displayCleanupDue(100,15000,14900));
 assert(displayCleanupDue(40,14900,UINT32_MAX-100));
 std::cout<<"PASS: unchanged frames, bounds, packed stride, cursor erasure, idle cleanup, millis rollover\n";
}
