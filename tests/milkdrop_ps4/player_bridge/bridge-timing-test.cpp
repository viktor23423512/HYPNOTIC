#include "milkdrop_bridge_timing.h"
#include <cassert>
#include <cstdio>
int main(){MilkdropBridgeTiming t;assert(t.mode==1&&!t.comparison);t.reset(100);assert(!t.releaseContext()&&t.sweepDone);uint64_t v[]={1,2,3,4,5,6,21};t.add(v);t.report(4000100);assert(t.mode==1);t.reset(5000100);assert(t.mode==1);t.manual(6000100);assert(t.mode==1);t.debug(true,7000100);assert(t.mode==0&&!t.sweepDone);t.add(v);t.report(11000100);assert(t.mode==0);t.tick(17000099);assert(t.mode==0);t.tick(17000100);assert(t.mode==1&&t.sweepDone);t.manual(18000100);assert(t.mode==0);t.tick(28000100);assert(t.mode==0);t.debug(false,30000100);assert(t.mode==1&&!t.comparison);puts("PASS immediate boosted normal/preset reset, reference10sec then boosted only in debug, manual debug toggle, disabling restores boosted");}
