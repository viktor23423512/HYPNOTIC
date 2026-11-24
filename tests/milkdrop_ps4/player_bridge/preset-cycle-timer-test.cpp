#include "preset_cycle.h"
#include <cassert>
#include <cstdio>
int main(){PresetCycleTimer t;assert(!t.enabled&&!t.tick(0,true));t.begin();assert(t.menuOpen&&!t.draftEnabled&&t.row==1&&!t.enabled);t.adjust(-1);assert(t.draftSeconds==25);t.cancel();assert(!t.enabled&&t.seconds==30);
 t.begin();t.move(-1);t.adjust(1);assert(t.draftEnabled&&!t.enabled);t.move(1);for(int i=0;i<500;++i)t.adjust(-1);assert(t.draftSeconds==5);t.apply();assert(t.enabled&&t.seconds==5);
 assert(!t.tick(100,true));assert(!t.tick(2000100,true));assert(!t.tick(3000100,false));assert(!t.tick(9000100,false));assert(!t.tick(10000100,true));assert(!t.tick(12900100,true));assert(t.tick(13000100,true));assert(!t.tick(13000100,true));assert(t.tick(999999999,true));assert(!t.tick(999999999,true));
 t.reset();assert(!t.tick(1,true));assert(!t.tick(4000001,true));t.begin();assert(t.draftEnabled&&t.enabled&&!t.tick(8000001,true));t.cancel();assert(!t.tick(10000001,true));assert(t.tick(11000001,true));
 t.begin();for(int i=0;i<500;++i)t.adjust(1);assert(t.draftSeconds==600);t.move(-1);t.adjust(-1);assert(!t.draftEnabled&&t.enabled);t.apply();assert(!t.enabled&&!t.tick(999999999999,true));
 t.begin();assert(!t.draftEnabled&&!t.enabled);t.apply();assert(!t.enabled&&t.seconds==600);t.begin();t.move(-1);t.adjust(1);t.apply();assert(t.enabled);assert(!t.tick(100,true));assert(t.tick(600000100,true));
 puts("PASS reopen Off/On accurately, unchanged apply/cancel, 5-600s limits, pause/menu freeze, manual reset, no catch-up bursts");}
