#pragma once
#include <stdint.h>
// Only navigation repeats. Never repeat apply, favorite or playback actions.
struct MenuRepeat {
 uint32_t held=0;uint64_t started[32]={},next[32]={};int context=-1;
 void reset(){held=0;context=-1;}
 uint32_t update(uint32_t buttons,uint32_t directions,uint32_t horizontal,uint64_t now,int view){
  if(view!=context){held=0;context=view;}uint32_t result=0;buttons&=directions;
  for(unsigned i=0;i<32;++i){uint32_t bit=uint32_t(1)<<i;if(!(buttons&bit))continue;
   if(!(held&bit)){started[i]=now;next[i]=now+360000;}
   else if(now>=next[i]){result|=bit;next[i]=now+((horizontal&bit)?220000:now-started[i]>=1800000?65000:110000);}
  }held=buttons;return result;
 }
};
struct HudVisibility {
 uint64_t until=0,delay=5000000;bool pinned=false;
 void reveal(uint64_t now){until=now+delay;}
 bool visible(uint64_t now,bool paused)const{return paused||pinned||now<until;}
 void toggle(uint64_t now){pinned=!pinned;until=pinned?now+delay:now;}
};
