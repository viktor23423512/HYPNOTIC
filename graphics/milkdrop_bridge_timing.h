#pragma once
#include <stdint.h>
#include <stdio.h>
struct MilkdropBridgeTiming {
 unsigned mode=1,frames=0;bool started=false,sweepDone=true,comparison=false;uint64_t window=0,phaseStart=0,total[7]={};
 void clearWindow(uint64_t now){frames=0;window=now;for(auto &v:total)v=0;}
 void reset(uint64_t now){mode=comparison?0:1;started=true;sweepDone=!comparison;phaseStart=now;clearWindow(now);}
 void debug(bool enabled,uint64_t now){comparison=enabled;reset(now);}
 bool releaseContext()const{return mode==0;}
 void tick(uint64_t now){if(comparison&&!sweepDone&&mode==0&&now-phaseStart>=10000000){mode=1;sweepDone=true;clearWindow(now);printf("HYPNOTIC AB BOOSTED - SAME PRESET STATE\n");}}
 void add(const uint64_t *v){for(unsigned i=0;i<7;++i)total[i]+=v[i];++frames;}
 bool due(uint64_t now)const{return started&&frames&&now-window>=4000000;}
 void report(uint64_t now){printf("HYPNOTIC MILKDROP BRIDGE TIMING MODE %u %s FPS %.2f FRAMES %u RELEASE %llu NATIVE %llu RESTORE %llu COPY %llu UPLOAD %llu DRAW_SWAP %llu FRAME %llu US\n",mode,mode?"CACHED_DMA":"REFERENCE",frames*1000000./(now-window),frames,(unsigned long long)(total[0]/frames),(unsigned long long)(total[1]/frames),(unsigned long long)(total[2]/frames),(unsigned long long)(total[3]/frames),(unsigned long long)(total[4]/frames),(unsigned long long)(total[5]/frames),(unsigned long long)(total[6]/frames));clearWindow(now);}
 void manual(uint64_t now){if(!comparison)return;mode^=1;sweepDone=true;clearWindow(now);}
};
