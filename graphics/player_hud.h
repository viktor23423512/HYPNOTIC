#pragma once
#include "status_text.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct PlayerHudData{const char *track,*visualizer;uint64_t position,total;bool shuffle;int repeat,selected;bool autoCycle=false;unsigned cycleSeconds=30;bool paused=true;};
template<class Scene> void hudLine(Scene *s,int x,int y,int xx,int yy,Color color){int dx=xx-x,dy=yy-y,steps=abs(dx)>abs(dy)?abs(dx):abs(dy);if(!steps)steps=1;for(int i=0;i<=steps;++i)s->DrawRectangle(x+dx*i/steps,y+dy*i/steps,3,3,color);}
template<class Scene> void hudIcon(Scene *s,int kind,int x,int y,Color c){
 if(kind<3){if(kind==0)s->DrawRectangle(x-7,y,4,24,c);if(kind==2)s->DrawRectangle(x+25,y,4,24,c);
  for(int i=0;i<20;++i){int h=24-i;int xx=kind==0?x+19-i:x+i;s->DrawRectangle(xx,y+i/2,2,h,c);}return;}
 if(kind==3){hudLine(s,x,y+3,x+30,y+21,c);hudLine(s,x,y+21,x+30,y+3,c);hudLine(s,x+22,y+3,x+30,y+3,c);hudLine(s,x+30,y+3,x+30,y+11,c);hudLine(s,x+22,y+21,x+30,y+21,c);hudLine(s,x+30,y+13,x+30,y+21,c);}
 else if(kind==5){
  hudLine(s,x,y+3,x+23,y+3,c);hudLine(s,x,y+3,x,y+20,c);hudLine(s,x,y+20,x+10,y+20,c);hudLine(s,x+4,y,x+27,y,c);hudLine(s,x+27,y,x+27,y+8,c);
  const int cx=x+23,cy=y+18;int points[9][2]={{0,-9},{6,-6},{9,0},{6,6},{0,9},{-6,6},{-9,0},{-6,-6},{0,-9}};
  for(int i=0;i<8;++i)hudLine(s,cx+points[i][0],cy+points[i][1],cx+points[i+1][0],cy+points[i+1][1],c);hudLine(s,cx,cy,cx,cy-5,c);hudLine(s,cx,cy,cx+4,cy,c);
 }
 else{hudLine(s,x,y+5,x+30,y+5,c);hudLine(s,x+30,y+5,x+30,y+20,c);hudLine(s,x+30,y+20,x,y+20,c);hudLine(s,x,y+20,x,y+5,c);hudLine(s,x+24,y,x+30,y+5,c);hudLine(s,x+24,y+10,x+30,y+5,c);}
}
template<class Scene> void drawPlayerHud(Scene *s,const PlayerHudData &d){
 s->DrawRectangle(48,820,1824,238,Color{4,10,16});drawStatusText(s,d.paused?"\x89":"\x88",840,80,1,Color{0,200,230});
 drawStatusText(s,d.track,840,112,52);char name[40];snprintf(name,sizeof(name),"%.38s",d.visualizer);drawStatusText(s,name,840,1856-(int)strlen(name)*18,38);
 uint64_t position=d.position<d.total?d.position:d.total;s->DrawRectangle(80,884,1760,10,Color{20,35,44});int fill=d.total?(int)((double)position/d.total*1760):0;if(fill)s->DrawRectangle(80,884,fill,10,Color{0,200,230});
 char time[32];snprintf(time,sizeof(time),"%02llu:%02llu",(unsigned long long)(position/60000),(unsigned long long)(position/1000%60));drawStatusText(s,time,906,80,16);
 snprintf(time,sizeof(time),"%02llu:%02llu",(unsigned long long)(d.total/60000),(unsigned long long)(d.total/1000%60));drawStatusText(s,time,906,1856-(int)strlen(time)*18,16);
 const char *repeat[]={"REPEAT OFF","REPEAT ALL","REPEAT ONE"};char autoLabel[24];if(d.autoCycle)snprintf(autoLabel,sizeof(autoLabel),"AUTO %uS",d.cycleSeconds);else snprintf(autoLabel,sizeof(autoLabel),"AUTO OFF");
 const char *labels[]={d.shuffle?"SHUFFLE ON":"SHUFFLE OFF",repeat[d.repeat>=0&&d.repeat<3?d.repeat:0],"PREVIOUS",d.paused?"PLAY":"PAUSE","NEXT",autoLabel};
 const int positions[]={80,328,640,864,1088,1624},widths[]={232,232,192,192,192,232},icons[]={3,4,0,1,2,5};
 for(int i=0;i<6;++i){int x=positions[i],w=widths[i];if(d.selected==i)s->DrawRectangle(x,948,w,76,Color{0,55,72});if(i==3&&!d.paused){s->DrawRectangle(x+w/2-12,956,7,24,Color{0,200,230});s->DrawRectangle(x+w/2+5,956,7,24,Color{0,200,230});}else hudIcon(s,icons[i],x+w/2-16,956,Color{0,200,230});drawStatusText(s,labels[i],994,x+(w-(int)strlen(labels[i])*18)/2,12);}
 drawStatusText(s,d.selected<0?"\x8f MUSIC - \x87 SETTINGS - \x84 VISUALIZERS":d.paused?"\x89  \x82\x83 SELECT - X APPLY - L1 R1 PRESETS - L2 R2 SCRUB - L3 PIN HUD":"\x88  \x82\x83 SELECT - X APPLY - L1 R1 PRESETS - L2 R2 SCRUB - L3 PIN HUD",1030,80,98);
}

template<class Scene> void drawPresetCycleMenu(Scene *s,bool enabled,unsigned seconds,int row){
 s->DrawRectangle(624,568,672,244,Color{4,10,16});drawStatusText(s,"AUTO-CYCLE PRESETS",592,648,34);
 s->DrawRectangle(640,635+row*44,640,32,Color{0,55,72});drawStatusText(s,"MODE",640,656,10);drawStatusText(s,enabled?"ON":"OFF",640,1080,8);
 drawStatusText(s,"INTERVAL",684,656,12);char value[20];snprintf(value,sizeof(value),"%u SEC",seconds);drawStatusText(s,value,684,1080,10);
 drawStatusText(s,"UP DOWN SELECT - LEFT RIGHT ADJUST",728,648,34);drawStatusText(s,"X APPLY - CIRCLE CANCEL",770,648,34);
}
