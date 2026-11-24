#pragma once
#include "status_text.h"
#include "modern_ui.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct PlayerHudData{const char *track,*visualizer;uint64_t position,total;bool shuffle;int repeat,selected;bool autoCycle=false;unsigned cycleSeconds=30;bool paused=true;int volume=100;bool muted=false;};
#include "hud_icons.h"
template<class Scene> void drawPlayerHud(Scene *s,const PlayerHudData &d){
 if(currentUiSettings().style==1){
  const Color accent{0,200,230},muted{160,180,190},dark{0,20,26};s->DrawRectangle(88,800,1744,232,Color{4,10,16});uiLabel(s,d.track,128,824,27,muted,1000);uiLabel(s,d.visualizer,1792,828,23,muted,520,2);
  uint64_t position=d.position<d.total?d.position:d.total;s->DrawRectangle(128,877,1664,5,Color{20,35,44});int fill=d.total?(int)((double)position/d.total*1664):0;if(fill)s->DrawRectangle(128,877,fill,5,accent);
  char text[40];snprintf(text,sizeof(text),"%llu:%02llu",(unsigned long long)(position/60000),(unsigned long long)(position/1000%60));uiLabel(s,text,128,893,20);snprintf(text,sizeof(text),"%llu:%02llu",(unsigned long long)(d.total/60000),(unsigned long long)(d.total/1000%60));uiLabel(s,text,1792,893,20,muted,180,2);
  const int x[]={128,240,840,940,1040,1620,1740},kinds[]={3,4,0,1,2,5,6};
  for(int i=0;i<7;++i)if(i!=3&&d.selected==i)hudHighlight(s,i==6&&(d.muted||d.volume==0)?7:kinds[i],x[i],945);
  hudIcon(s,3,128,945,d.shuffle?accent:muted);hudIcon(s,4,240,945,d.repeat?accent:muted);
  hudIcon(s,0,840,945,muted);hudIcon(s,2,1040,945,muted);
  if(d.selected==3)uiRounded(s,921,921,78,78,39,muted);uiRounded(s,924,924,72,72,36,accent);
  if(d.paused)hudIcon(s,1,940,944,dark);else{s->DrawRectangle(947,945,7,30,dark);s->DrawRectangle(967,945,7,30,dark);}
  hudIcon(s,5,1620,945,d.autoCycle?accent:muted);hudIcon(s,d.muted||d.volume==0?7:6,1740,945,muted);

  if(d.repeat==2)uiLabel(s,"1",281,947,17,accent,20);
  if(d.selected>=0)uiLabel(s,"\x82\x83 Select   X Apply   L1 R1 Presets   L2 R2 Scrub   L3 Pin HUD",960,1007,17,Color{119,144,161},1500,1);return;
 }
 s->DrawRectangle(48,820,1824,238,Color{4,10,16});drawStatusText(s,d.paused?"\x89":"\x88",840,80,1,Color{0,200,230});
 drawStatusText(s,d.track,840,112,52);char name[40];snprintf(name,sizeof(name),"%.38s",d.visualizer);drawStatusText(s,name,840,1856-uiTextWidth(name,38),38);
 uint64_t position=d.position<d.total?d.position:d.total;s->DrawRectangle(80,884,1760,10,Color{20,35,44});int fill=d.total?(int)((double)position/d.total*1760):0;if(fill)s->DrawRectangle(80,884,fill,10,Color{0,200,230});
 char time[32];snprintf(time,sizeof(time),"%02llu:%02llu",(unsigned long long)(position/60000),(unsigned long long)(position/1000%60));drawStatusText(s,time,906,80,16);
 snprintf(time,sizeof(time),"%02llu:%02llu",(unsigned long long)(d.total/60000),(unsigned long long)(d.total/1000%60));drawStatusText(s,time,906,1856-uiTextWidth(time,16),16);
 const int positions[]={80,224,680,896,1112,1648,1792},icons[]={3,4,0,1,2,5,6};
 const Color accent{0,200,230},dim{82,106,124};
 for(int i=0;i<7;++i){int x=positions[i];if(d.selected==i){if(i==3&&!d.paused)s->DrawRectangle(x-13,937,64,64,Color{0,55,72});else hudHighlight(s,i==6&&(d.muted||d.volume==0)?7:icons[i],x,954);}bool enabled=i==0?d.shuffle:i==1?d.repeat!=0:i==5?d.autoCycle:true;Color c=enabled?accent:dim;
  if(i==3&&!d.paused){s->DrawRectangle(x+8,956,6,26,c);s->DrawRectangle(x+24,956,6,26,c);}else hudIcon(s,i==6&&(d.muted||d.volume==0)?7:icons[i],x,954,c);

 }
 if(d.repeat==2)drawStatusText(s,"1",955,270,1,accent);
 drawStatusText(s,d.selected<0?"\x8f MUSIC - \x87 SETTINGS - \x84 VISUALIZERS":d.paused?"\x89  \x82\x83 SELECT - X APPLY - L1 R1 PRESETS - L2 R2 SCRUB - L3 PIN HUD":"\x88  \x82\x83 SELECT - X APPLY - L1 R1 PRESETS - L2 R2 SCRUB - L3 PIN HUD",1030,80,98);
}

template<class Scene> void drawPresetCycleMenu(Scene *s,bool enabled,unsigned seconds,int row){
 s->DrawRectangle(624,568,672,244,Color{4,10,16});drawStatusText(s,"AUTO-CYCLE PRESETS",592,648,34);
 s->DrawRectangle(640,635+row*44,640,32,Color{0,55,72});drawStatusText(s,"MODE",640,656,10);drawStatusText(s,enabled?"ON":"OFF",640,1080,8);
 drawStatusText(s,"INTERVAL",684,656,12);char value[20];snprintf(value,sizeof(value),"%u SEC",seconds);drawStatusText(s,value,684,1080,10);
 drawStatusText(s,"UP DOWN SELECT - LEFT RIGHT ADJUST",728,648,34);drawStatusText(s,"X APPLY - CIRCLE CANCEL",770,648,34);
}

template<class Scene> void drawVolumePopover(Scene *s,const AudioOutputSettings &audio){
 const bool modern=currentUiSettings().style==1;const int x=modern?1652:1692,y=modern?360:380,cx=x+90;const Color text{160,180,190},accent{0,200,230};
 s->DrawRectangle(x,y,180,420,Color{4,10,16});char label[16];snprintf(label,sizeof(label),"%d%%",audio.volume);uiLabel(s,label,cx,y+28,28,text,156,1);
 const int top=y+88,bottom=y+284,knob=bottom-audio.volume*196/100;s->DrawRectangle(cx-5,top,10,196,Color{20,35,44});if(knob<bottom)s->DrawRectangle(cx-5,knob,10,bottom-knob,audio.muted?text:accent);s->DrawRectangle(cx-23,knob-6,46,13,audio.muted?text:accent);
 hudIcon(s,audio.muted||audio.volume==0?7:6,cx-20,y+298,audio.muted?text:accent);
 uiLabel(s,"\x80\x81 Adjust",cx,y+344,17,text,156,1);uiLabel(s,"\x86 Mute",cx,y+366,17,text,156,1);uiLabel(s,"X Close",cx,y+388,17,text,156,1);
}
