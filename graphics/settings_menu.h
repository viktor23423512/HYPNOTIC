#pragma once
#include "status_text.h"
template<class Scene> void drawAppSettings(Scene *scene,const UiSettings &ui,int row,bool hd,bool credits,int sensitivity){
 scene->DrawRectangle(48,164,1824,636,Color{4,10,16});drawStatusText(scene,credits?"HYPNOTIC / CREDITS":"HYPNOTIC / SETTINGS",194,80,96);
 if(credits){const char *lines[]={"HYPNOTIC - MUSIC AND VISUALS","OPENORBIS / PS4 HOMEBREW COMMUNITY","MILKDROP: RYAN GEISS / PROJECTM CONTRIBUTORS","PRESETS: CREAM OF THE CROP / COMMUNITY ARTISTS","SHADERS: OPEN GNM / MESA / GLSLANG / HLSLPARSER","AUDIO: DR_LIBS / DAVID REID - STB / SEAN BARRETT","USB ACCESS: LIBJBC CONTRIBUTORS","THANK YOU TO THE PRESET ARTISTS AND TESTERS"};for(int i=0;i<8;++i)drawStatusText(scene,lines[i],286+i*49,80,96);}
 else for(int i=0;i<8;++i){int y=286+i*55;if(i==row)scene->DrawRectangle(64,y-8,1792,38,Color{0,55,72});char text[128];switch(i){case 0:snprintf(text,sizeof(text),"THEME                  %s",ui.themeName());break;case 1:snprintf(text,sizeof(text),"FONT                   %s",ui.fontName());break;case 2:snprintf(text,sizeof(text),"HUD AUTO-HIDE          %d SECONDS",ui.hudSeconds);break;case 3:snprintf(text,sizeof(text),"SENSITIVITY            %.2f X",.5f+sensitivity*.25f);break;case 4:snprintf(text,sizeof(text),"MILKDROP QUALITY       %s - AFTER RELAUNCH",hd?"1080P":"288P");break;case 5:snprintf(text,sizeof(text),"FPS / TEMPERATURE      %s",ui.stats?"ON":"OFF");break;case 6:snprintf(text,sizeof(text),"MIC TEST               X OPEN");break;default:snprintf(text,sizeof(text),"CREDITS                X OPEN");break;}drawStatusText(scene,text,y,80,96);}
 scene->DrawRectangle(64,744,1792,2,Color{27,45,55});drawStatusText(scene,credits?"\x86 BACK TO SETTINGS  \x87 CLOSE":"\x80\x81 SELECT  \x82\x83 ADJUST  X APPLY  \x86 BACK  \x87 CLOSE",768,80,98);
}

template<class Scene> void drawMicTest(Scene *scene,const char *status,const char *details,int peak){
 scene->DrawRectangle(48,164,1824,636,Color{4,10,16});drawStatusText(scene,"SETTINGS / MIC TEST",194,80,94);
 drawStatusText(scene,status,286,80,96);drawStatusText(scene,"SPEAK INTO YOUR MICROPHONE - WATCH THE LEVEL",365,80,96);
 scene->DrawRectangle(80,420,1760,24,Color{20,35,44});int p=peak<0?0:peak>32768?32768:peak;int width=p*1760/32768;if(width)scene->DrawRectangle(80,420,width,24,Color{0,200,230});
 char text[64];snprintf(text,sizeof(text),"PEAK %d - 16000 HZ MONO",p);drawStatusText(scene,text,478,80,90);drawStatusText(scene,details,550,80,96);
 drawStatusText(scene,"INPUT LEVEL AND DIAGNOSTICS ONLY",650,80,94);scene->DrawRectangle(64,744,1792,2,Color{27,45,55});drawStatusText(scene,"\x86 BACK TO SETTINGS - \x87 CLOSE",768,80,96);
}
