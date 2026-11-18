#include <cstdint>
#include <cstdio>
#include <cassert>
static uint64_t clockNow=100;static uint64_t sceKernelGetProcessTime(){return clockNow;}
#include "player_native_timing.hpp"
int main(){PlayerNativeTiming t;t.reset();uint64_t v[12];for(unsigned i=0;i<12;++i)v[i]=i*100;t.add(v,false);assert(t.frames==1);for(auto n:t.total)assert(n==100);t.add(v,true);assert(t.mode&&t.frames==1);for(auto n:t.total)assert(n==100);clockNow=4000101;t.add(v,true);assert(t.frames==0);for(auto n:t.total)assert(n==0);t.reset();assert(t.window==clockNow);puts("PASS all11 stages, mode switch reset, report window and accumulation reset");}
