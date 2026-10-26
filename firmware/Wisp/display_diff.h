#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// One-bit row-major framebuffer; x endpoints are byte-aligned, ends exclusive.
struct DisplayRegion {
 int x0=800, y0=480, x1=0, y1=0;
 bool changed() const { return x1>x0 && y1>y0; }
 size_t bytes() const { return changed() ? size_t((x1-x0)/8)*(y1-y0) : 0; }
};
inline DisplayRegion displayDifference(const uint8_t* before,const uint8_t* after) {
 DisplayRegion r;
 for(int y=0;y<480;y++)for(int b=0;b<100;b++)if(before[y*100+b]!=after[y*100+b]){
  if(b*8<r.x0)r.x0=b*8;if((b+1)*8>r.x1)r.x1=(b+1)*8;
  if(y<r.y0)r.y0=y;if(y+1>r.y1)r.y1=y+1;
 }
 return r;
}
inline void packDisplayRegion(const uint8_t* frame,uint8_t* packed,const DisplayRegion& r) {
 const int stride=(r.x1-r.x0)/8;
 for(int y=r.y0;y<r.y1;y++)memcpy(packed+(y-r.y0)*stride,frame+y*100+r.x0/8,stride);
}
// Defer periodic ghosting cleanup until navigation has been idle.
inline bool displayCleanupDue(unsigned updates,uint32_t now,uint32_t lastInput) {
 return updates>=40 && uint32_t(now-lastInput)>=15000;
}
