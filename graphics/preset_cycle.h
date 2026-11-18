#pragma once
#include <stdint.h>
struct PresetCycleTimer {
 bool enabled=false,menuOpen=false,draftEnabled=false,active=false;unsigned seconds=30,draftSeconds=30;int row=1;
 uint64_t elapsed=0,last=0;
 void reset(){elapsed=last=0;active=false;}
 void begin(){menuOpen=true;draftEnabled=true;draftSeconds=seconds;row=1;}
 void move(int direction){row=direction<0?0:1;}
 void adjust(int direction){if(!row){draftEnabled=!draftEnabled;return;}int n=(int)draftSeconds+(direction<0?-5:5);draftSeconds=n<5?5:n>600?600:n;}
 void apply(){enabled=draftEnabled;seconds=draftSeconds;menuOpen=false;reset();}
 void cancel(){menuOpen=false;}
 bool tick(uint64_t now,bool allowed){
  if(!enabled || !allowed || menuOpen){active=false;last=now;return false;}
  if(!active){active=true;last=now;return false;}
  uint64_t delta=now>=last?now-last:0;last=now;const uint64_t interval=(uint64_t)seconds*1000000;
  if(delta>=interval-elapsed){elapsed=0;return true;}elapsed+=delta;return false;
 }
};
