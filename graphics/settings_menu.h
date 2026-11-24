#pragma once
#include "modern_ui.h"
#include "app_settings_model.h"
template<class Scene> void drawAppSettings(Scene *scene,const UiSettings &ui,int row,bool hd,bool credits,int sensitivity,int tab=0,bool debug=false){
 if(credits){scene->DrawRectangle(48,164,1824,636,Color{4,10,16});drawStatusText(scene,"HYPNOTIC / CREDITS",194,80,96);const char *lines[]={"HYPNOTIC - MUSIC AND VISUALS","OPENORBIS / PS4 HOMEBREW COMMUNITY","MILKDROP: RYAN GEISS / PROJECTM CONTRIBUTORS","PRESETS: CREAM OF THE CROP / COMMUNITY ARTISTS","SHADERS: OPEN GNM / MESA / GLSLANG / HLSLPARSER","AUDIO: DR_LIBS / DAVID REID - STB / SEAN BARRETT","USB ACCESS: LIBJBC CONTRIBUTORS","FONTS: ATKINSON HYPERLEGIBLE / JETBRAINS / RAJDHANI"};for(int i=0;i<8;++i)drawStatusText(scene,lines[i],286+i*49,80,96);drawStatusText(scene,"\x86 BACK TO SETTINGS  \x87 CLOSE",768,80,98);return;}
 const bool modern=ui.style==1;const int rows=appSettingsRowCount(tab);if(modern){uiModernPanel(scene,"Settings",row+1,rows);int x=128;for(int t=0;t<4;++t)x+=uiModernTab(scene,appSettingsTabName(t),x,t==tab);}else{scene->DrawRectangle(48,164,1824,636,Color{4,10,16});drawStatusText(scene,"HYPNOTIC / SETTINGS",194,80,96);for(int t=0;t<4;++t){int x=80+t*438;scene->DrawRectangle(x,244,420,48,t==tab?Color{0,200,230}:Color{18,35,44});drawStatusText(scene,appSettingsTabName(t),258,x+18,28,t==tab?Color{0,20,26}:Color{160,180,190});}}
 for(int i=0;i<rows;++i){int y=(modern?306:330)+i*68;auto setting=appSettingsAction(tab,i);char value[128];appSettingValue(value,sizeof(value),setting,ui,hd,sensitivity);if(setting==AppSetting::DebugComparison)snprintf(value,sizeof(value),"%s",debug?"On - reference / boosted":"Off");if(i==row)scene->DrawRectangle(modern?116:64,y-8,modern?1688:1792,44,Color{0,55,72});if(modern){uiLabel(scene,appSettingLabel(setting),143,y,26,Color{160,180,190},650);uiLabel(scene,value,920,y+1,24,Color{160,180,190},830);}else{drawStatusText(scene,appSettingLabel(setting),y,80,40);drawStatusText(scene,value,y,872,52);}}
 if(modern)uiModernFooter(scene,"L1 R1 Tabs    \x80\x81 Select    \x82\x83 Adjust    X Open / Toggle");else{scene->DrawRectangle(64,744,1792,2,Color{27,45,55});drawStatusText(scene,"L1 R1 TABS  \x80\x81 SELECT  \x82\x83 ADJUST  X OPEN / TOGGLE  \x86 BACK",768,80,98);}
}
template<class Scene> void drawAudioEqualizer(Scene *s,const UiSettings &ui,int band){
 const bool modern=ui.style==1;const Color text{160,180,190},accent{0,200,230};
 if(modern)uiModernBrand(s);
 s->DrawRectangle(modern?88:48,modern?100:164,modern?1530:1570,modern?680:636,Color{4,10,16});
 uiLabel(s,modern?"Equalizer":"MUSIC / EQUALIZER",modern?128:80,modern?132:194,modern?36:24,text,1000);
 if(modern)uiLabel(s,"Music / Equalizer",128,186,21,text,700);
 uiLabel(s,ui.audio.eqEnabled?"\x84 EQ On":"\x84 EQ Off",1136,modern?147:200,22,ui.audio.eqEnabled?accent:text,200);
 uiLabel(s,"\x85 Reset Flat",1350,modern?147:200,22,text,230);
 static const char *hz[]={"60 Hz","250 Hz","1 kHz","4 kHz","12 kHz"};
 static const char *names[]={"Bass","Low mids","Mids","Presence","Treble"};
 for(int i=0;i<5;++i){int x=410+i*240,gain=ui.audio.bands[i],knob=476-gain*152/12;
  if(i==band)s->DrawRectangle(x-70,246,140,464,Color{0,55,72});
  char value[24];snprintf(value,sizeof(value),"%+d dB",gain);uiLabel(s,value,x,280,24,i==band?accent:text,132,1);
  s->DrawRectangle(x-4,324,8,304,Color{20,35,44});
  for(int tick=0;tick<5;++tick)s->DrawRectangle(x-19,324+tick*76,38,1,Color{82,106,124});
  if(gain)s->DrawRectangle(x-4,knob<476?knob:476,8,abs(knob-476),ui.audio.eqEnabled?accent:text);
  s->DrawRectangle(x-26,knob-6,52,13,i==band?accent:text);
  uiLabel(s,hz[i],x,649,24,text,190,1);uiLabel(s,names[i],x,681,19,text,210,1);
 }
 uiLabel(s,"\x82\x83 Band   \x80\x81 Adjust   \x85 Flat   \x84 EQ On / Off   \x86 Back",modern?128:80,modern?737:752,20,text,1450);
}

template<class Scene> void drawMicTest(Scene *scene,const char *status,const char *details,int peak){
 scene->DrawRectangle(48,164,1824,636,Color{4,10,16});drawStatusText(scene,"SETTINGS / MIC TEST",194,80,94);
 drawStatusText(scene,status,286,80,96);drawStatusText(scene,"SPEAK INTO YOUR MICROPHONE - WATCH THE LEVEL",365,80,96);
 scene->DrawRectangle(80,420,1760,24,Color{20,35,44});int p=peak<0?0:peak>32768?32768:peak;int width=p*1760/32768;if(width)scene->DrawRectangle(80,420,width,24,Color{0,200,230});
 char text[64];snprintf(text,sizeof(text),"PEAK %d - 16000 HZ MONO",p);drawStatusText(scene,text,478,80,90);drawStatusText(scene,details,550,80,96);
 drawStatusText(scene,"INPUT LEVEL AND DIAGNOSTICS ONLY",650,80,94);scene->DrawRectangle(64,744,1792,2,Color{27,45,55});drawStatusText(scene,"\x86 BACK TO SETTINGS - \x87 CLOSE",768,80,96);
}
