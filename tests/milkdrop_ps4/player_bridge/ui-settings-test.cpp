#include <cassert>
#include <cstdio>
#include <cstring>
#include "visualizer_library.h"
int main(){
 MilkdropBrowser milk;const char *names[]={"WAVEFORM"};VisualizerLibrary a(milk,names,1),b(milk,names,1);
 assert(a.appearance.style==0&&a.appearance.font==0);
 for(int style=0;style<2;++style)for(int theme=0;theme<UiSettings::themeCount;++theme)for(int font=0;font<UiSettings::fontCount;++font){
  a.appearance.setStyle(style);a.appearance.theme=theme;a.appearance.setFont(font);a.appearance.hudSeconds=15;a.appearance.stats=true;
  assert(b.decode(a.encode()));assert(b.appearance.style==style&&b.appearance.font==font&&b.appearance.theme==theme&&b.appearance.stats);
 }
 a.appearance.setStyle(0);a.appearance.setFont(2);a.appearance.setStyle(1);a.appearance.setFont(5);a.appearance.setStyle(0);assert(a.appearance.font==2);assert(b.decode(a.encode()));b.appearance.setStyle(1);assert(b.appearance.font==5);
 assert(b.decode("HYPNOTIC_UI_1\nU\t4,1,10,3,1\nF\tbuiltin:0\nR\tbuiltin:0\n"));assert(b.appearance.style==0&&b.appearance.font==1&&b.appearance.styleFonts[1]==3&&b.favorite(-1)&&b.recentCount==1);
 b.appearance.setStyle(1);assert(b.appearance.font==3);b.appearance.setStyle(0);assert(b.appearance.font==1);
 for(const char *bad:{"9,0,5,2,0","0,6,5,2,0","0,0,0,2,0","0,0,5,9,0","0,0,5,2,5","0,0,5,2,0oops"}){assert(b.decode(std::string("HYPNOTIC_UI_1\nU\t")+bad+"\n"));assert(b.appearance.theme==0&&b.appearance.font==0&&b.appearance.hudSeconds==5&&!b.appearance.stats);}
 for(const char *bad:{"2,0,3","1,0,6","1,-1,3","1,0,3oops"}){assert(b.decode(std::string("HYPNOTIC_UI_1\nS\t")+bad+"\n"));assert(b.appearance.style==0&&b.appearance.font==0);}
 a.appearance.audio.volume=35;a.appearance.audio.muted=true;a.appearance.audio.bands[0]=12;a.appearance.audio.bands[4]=-12;assert(b.decode(a.encode()));assert(b.appearance.audio.packed()==a.appearance.audio.packed());
 assert(b.decode("HYPNOTIC_UI_1\nU\t4,1,10,3,1\nF\tbuiltin:0\n"));assert(b.appearance.audio.volume==100&&!b.appearance.audio.muted&&b.appearance.audio.isFlat()&&b.favorite(-1));
 for(const char *bad:{"101,0,1,0,0,0,0,0","50,2,1,0,0,0,0,0","50,0,2,0,0,0,0,0","50,0,1,13,0,0,0,0","50,0,1,0,0,0,0,0extra"}){assert(b.decode(std::string("HYPNOTIC_UI_1\nA\t")+bad+"\n"));assert(b.appearance.audio.volume==100&&b.appearance.audio.isFlat());}
 puts("PASS 108 appearance combinations, independent style fonts, old preferences, favorites/recents and invalid settings");
}
