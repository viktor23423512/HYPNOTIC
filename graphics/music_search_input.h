#pragma once
#include <string.h>
#include <stdio.h>
struct MusicSearchInput {
 bool open=false;int selected=0;char text[64]={};
 static const char *keys(){return "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_.";}
 void start(const char *query){snprintf(text,sizeof(text),"%s",query);selected=0;open=true;}
 void erase(){size_t n=strlen(text);if(n)text[n-1]=0;}
 void clear(){text[0]=0;}
 void move(int x,int y){if(y<0){if(selected>=40)selected=selected==40?30:39;else if(selected>=10)selected-=10;}
  if(y>0){if(selected<30)selected+=10;else if(selected<40)selected=selected<35?40:41;}
  if(x){int base=selected>=40?40:(selected/10)*10,cols=selected>=40?2:10;selected=base+(selected-base+x+cols)%cols;}}
 // 0 edits,1submit,-1cancel. No editing ever changes playback.
 int choose(){if(selected>=40){open=false;return selected==40?1:-1;}size_t n=strlen(text);if(n<sizeof(text)-1){text[n]=keys()[selected];text[n+1]=0;}return 0;}
};
